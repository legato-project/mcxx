/*--------------------------------------------------------------------
  (C) Copyright 2006-2009 Barcelona Supercomputing Center 
                          Centro Nacional de Supercomputacion
  
  This file is part of Mercurium C/C++ source-to-source compiler.
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 3 of the License, or (at your option) any later version.
  
  Mercurium C/C++ source-to-source compiler is distributed in the hope
  that it will be useful, but WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the GNU Lesser General Public License for more
  details.
  
  You should have received a copy of the GNU Lesser General Public
  License along with Mercurium C/C++ source-to-source compiler; if
  not, write to the Free Software Foundation, Inc., 675 Mass Ave,
  Cambridge, MA 02139, USA.
--------------------------------------------------------------------*/

#include "hlt-simd.hpp"
#include "tl-generic_vector.hpp"
#include "tl-datareference.hpp"
#include <sstream>

using namespace TL::HLT;


const char* ReplaceSimdSrc::prettyprint_callback (AST a, void* data)
{
    //Standar prettyprint_callback
    const char *c = ReplaceSrcIdExpression::prettyprint_callback(a, data);

    ReplaceSimdSrc *_this = reinterpret_cast<ReplaceSimdSrc*>(data);

    //scalar declarations to __attribute__((generic_vector))
    if(c == NULL)
    {
        ReplaceSimdSrc *_this = reinterpret_cast<ReplaceSimdSrc*>(data);

        AST_t ast(a);

        if (TL::TypeSpec::predicate(ast))
        {
            return Type((TypeSpec(ast, _this->_sl)).get_type())
                .get_generic_vector_to()
                .get_simple_declaration(_this->_sl.get_scope(ast), "")
                .c_str();
        }
        else if (TL::DataReference::predicate(ast))
        {
            DataReference dataref(ast, _this->_sl);

            if (dataref.is_valid())
            {
                Symbol sym = dataref.get_base_symbol();

                if (_this->_simd_id_exp_list.contains(functor(&IdExpression::get_symbol), sym))
                {
                    std::stringstream result;

                    result << BUILTIN_VR_NAME 
                        << "("
                        << DataReference(ast, _this->_sl).prettyprint()
                        << ")";

                    return result.str().c_str();
                }
            }
        }
    }       

    return c; 

#if 0   
    //builtin_vector_expansion
    //Standar prettyprint_callback
    const char *c = ReplaceSrcIdExpression::prettyprint_callback(a, data);

    if(c == NULL)
    {
        ReplaceSimdSrc *_this = reinterpret_cast<ReplaceSimdSrc*>(data);

        AST_t ast(a);

        if (Expression::predicate(ast))
        {
            Expression expr(ast, _this->_sl);

            if (expr.is_assignment())
            {
                Expression left_expr(expr.get_first_operand());

                if (left_expr.is_array_subscript())
                {
                    Expression array_sub_expr(left_expr.get_subscripted_expression());
                    if (array_sub_expr.is_id_expression())
                    {
                        Symbol sym = array_sub_expr.get_id_expression().get_symbol();
                        if (_this->_repl_map.find(sym) != _this->_repl_map.end())
                        {
                            Expression right_expr(expr.get_second_operand());
                            if (right_expr.is_constant())
                            {
                                (*_this->is_generic_vector) = true;
                            }
                        }       
                    }
                }
            }
            else if (expr.is_constant() && (*_this->is_generic_vector))
            {
                (*_this->is_generic_vector) = false;
                std::stringstream output;

                output << BUILTIN_VE_NAME << "("
                    << expr.prettyprint() 
                    << ")"
                    ;

                return output.str().c_str();
            }

            return NULL;
        }
    }

    return c;
#endif
}

TL::Source ReplaceSimdSrc::replace(AST_t a) const
{
    Source result;

    char *c = prettyprint_in_buffer_callback(a.get_internal_ast(),
            &ReplaceSimdSrc::prettyprint_callback, (void*)this);

    // Not sure whether this could happen or not
    if (c != NULL)
    {
        result << std::string(c);
    }

    // The returned pointer came from C code, so 'free' it
    free(c);

    return result;
}

TL::Source ReplaceSimdSrc::replace(TL::LangConstruct a) const
{
    return ReplaceSimdSrc::replace(a.get_ast());
}

LoopSimdization TL::HLT::simdize_loop(TL::ForStatement& for_stmt, const ObjectList<IdExpression>& simd_id_exp_list, unsigned char& min_stmt_size)
{
    return LoopSimdization(for_stmt, simd_id_exp_list, min_stmt_size);
}


LoopSimdization::LoopSimdization(ForStatement& for_stmt, const ObjectList<IdExpression>& simd_id_exp_list, unsigned char& min_stmt_size) 
: _for_stmt(for_stmt), _simd_id_exp_list(simd_id_exp_list), _replacement(_for_stmt.get_loop_body().get_scope_link(), simd_id_exp_list), _min_stmt_size(min_stmt_size)
{
    is_simdizable = true;

    if ((!_for_stmt.get_iterating_condition().is_binary_operation()) ||
            (!_for_stmt.is_regular_loop()) ||
            (!_for_stmt.get_step().is_constant()))
    {
        _ostream
            << _for_stmt.get_ast().get_locus()
            << ": warning: is not a regular loop. Simdization will not be applied"
            << std::endl;

        is_simdizable = false;
    }
}


TL::Source LoopSimdization::get_source()
{
    return do_simdization();
}


void LoopSimdization::gen_vector_type(IdExpression id){

    TL::Symbol sym = id.get_computed_symbol();
    TL::Type type = sym.get_type();
    std::string old_sym_name = sym.get_name();
    std::stringstream new_sym_name;

    //Save the smallest type size
//    if (_min_stmt_size < type.basic_type().get_size())
//        _min_stmt_size = type.basic_type().get_size();

    if (type.is_array())
    {
        type = type.array_element();
    }
    else if(type.is_pointer())
    {
        type = type.points_to();
    }
    else
    {
        running_error("%s: error: symbol '%s' does not have a vectorizable type'\n",
                id.get_ast().get_locus().c_str(),
                old_sym_name.c_str());
    }
    /*
       new_sym_name << "*((__attribute__((" << ATTR_GEN_VEC_NAME << ")) " 
       << type.get_declaration(sym.get_scope(), "") 
       << " *) &(" 
       << sym.get_name() 
       << ")"
       ;
     */

    //Simd pointer declaration & init
    /*    _before_loop 
          << type.get_generic_vector_to()
          .get_pointer_to().get_declaration(sym.get_scope(), new_sym_name)
          << "="
          << "("
          << type.get_generic_vector_to()
          .get_pointer_to().get_declaration(sym.get_scope(), "")  
          << ")" 
          << old_sym_name 
          << ";"
          ;
     */
    _replacement.add_replacement(sym, new_sym_name.str());

}

bool isExpressionAssignment::do_(const AST_t& ast) const
{
    if (!ast.is_valid())
        return false;

    if (Statement::predicate(ast))
    {
        Statement stmt(ast, _sl);

        if (stmt.is_expression())
        {
            Expression expr = stmt.get_expression();

            if (expr.is_assignment() || expr.is_operation_assignment())
            {
                return true;
            }
        }
    }
    return false;
}

void LoopSimdization::compute_min_stmt_size()
{
    unsigned char statement_type_size;
    unsigned char min = 100;

    ObjectList<AST_t> assignment_list = _for_stmt.get_loop_body().
        get_ast().depth_subtrees(isExpressionAssignment(_for_stmt.get_scope_link()));

    for (ObjectList<AST_t>::iterator it = assignment_list.begin();
            it != assignment_list.end();
            it++)
    {
        Expression exp ((AST_t)*it, _for_stmt.get_scope_link());
        statement_type_size = exp.get_first_operand().get_type().get_size();
        min = (min <= statement_type_size) ? min : statement_type_size;
    }

    _min_stmt_size = min;
}

TL::Source LoopSimdization::do_simdization()
{
    if (!is_simdizable)
    {
        return _for_stmt.prettyprint();
    }

    //It computes the smallest type of the first operand of an assignment and stores it in _min_stmt_size
    compute_min_stmt_size();

    //Simd variable initialization & sustitution
    std::for_each(_simd_id_exp_list.begin(), _simd_id_exp_list.end(), 
            std::bind1st(std::mem_fun(&LoopSimdization::gen_vector_type), this));

    Statement initial_loop_body = _for_stmt.get_loop_body();
    Source replaced_loop_body = _replacement.replace(initial_loop_body);

    bool step_evaluation;

    AST_t it_init_ast (_for_stmt.get_iterating_init());
    Source it_init_source;

    _result
        << "{"
//        << _induction_var_decl
//        << _before_loop
        << _loop
//        << _after_loop
        << _epilog
        << "}"
        ;

    if (Declaration::predicate(it_init_ast))
    {
        Declaration decl(it_init_ast, _for_stmt.get_scope_link());
        ObjectList<DeclaredEntity> decl_ent_list = decl.get_declared_entities();

        if(!decl_ent_list[0].has_initializer())
        {
            running_error("%s: error: Declared Entity does not have initializer'\n",
                    it_init_ast.get_locus().c_str());
        }

        it_init_source 
            << decl.get_declaration_specifiers()
            << decl_ent_list[0].get_declared_symbol().get_name()
            << "="
            << BUILTIN_IV_NAME << "(" << decl_ent_list[0].get_initializer() << ");"
            ;
    }
    else if (Expression::predicate(it_init_ast))
    {
        Expression exp(it_init_ast, _for_stmt.get_scope_link());

        if (!exp.is_assignment())
        {
            running_error("%s: error: Iterating initializacion is an Expression but not an assignment'\n",
                    it_init_ast.get_locus().c_str());
        }

        it_init_source
            << exp.get_first_operand()
            << "="
            << BUILTIN_IV_NAME << "(" << exp.get_second_operand() << ");"
            ;
    }
    else
    {
        running_error("%s: error: Iterating initializacion is not a Declaration or a Expression'\n",
                it_init_ast.get_locus().c_str());
    }

    _loop << "for(" 
        << it_init_source
        << _for_stmt.get_iterating_condition() << ";"
        << _for_stmt.get_iterating_expression()
        << ")"
        << replaced_loop_body
        ;
/*    
    _loop << "for(" 
        << it_init_source
        << _for_stmt.get_iterating_condition() << ";"
        << BUILTIN_VL_NAME << "(" << _for_stmt.get_induction_variable() << ","
        << _for_stmt.get_step().evaluate_constant_int_expression(step_evaluation) << "," 
        << _min_stmt_size << "))"
        << replaced_loop_body
        ;

    if (!step_evaluation){
        running_error("%s: error: the loop is not simdizable. The step is not a compile-time evaluable constant.'\n",
                _for_stmt.get_ast().get_locus().c_str());
    }
*/

    //Epilog

    _epilog
        << "for(; " 
        << _for_stmt.get_iterating_condition() 
        << "; " 
        << _for_stmt.get_iterating_expression()
        << ")"
        << initial_loop_body
        ;

    return _result;
}

