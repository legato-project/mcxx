#include "codegen-fortran.hpp"
#include "fortran03-buildscope.h"

namespace Codegen
{
    void FortranBase::visit(const Nodecl::TopLevel& node)
    {
        Nodecl::List list = node.get_top_level().as<Nodecl::List>();

#if 0
        nodecl_codegen_pre_visitor_t pre_visitor;
        memset(&pre_visitor, 0, sizeof(pre_visitor));

        nodecl_init_walker((nodecl_external_visitor_t*)&pre_visitor, NULL);

        NODECL_VISITOR(&pre_visitor)->visit_function_code = pre_visit_function_code;
        NODECL_VISITOR(&pre_visitor)->visit_object_init = pre_visit_object_init;
        nodecl_walk((nodecl_external_visitor_t*)&pre_visitor, list);

        int i;
        for (i = 0; i < pre_visitor.num_modules; i++)
        {
            scope_entry_t* old_module = visitor->current_module;

            scope_entry_t* current_module = pre_visitor.modules[i]->module;

            current_module->entity_specs.codegen_status = CODEGEN_STATUS_DEFINED;

            visitor->current_module = current_module;

            codegen_module_header(visitor, current_module);

            int j;
            for (j = 0; j < pre_visitor.modules[i]->num_nodes; j++)
            {
                nodecl_t node = pre_visitor.modules[i]->nodes[j];

                walk(node);
            }

            codegen_module_footer(visitor, current_module);

            visitor->current_module = old_module;
        }
#endif

        walk(list);
    }

    void FortranBase::codegen_procedure(TL::Symbol entry, Nodecl::List statement_seq, Nodecl::List internal_subprograms)
    {
        inc_indent();

        declare_use_statements(statement_seq);

        // Check every related entries lest they required stuff coming from other modules
        TL::ObjectList<TL::Symbol> related_symbols = entry.get_related_symbols();

        for (TL::ObjectList<TL::Symbol>::iterator it = related_symbols.begin();
                it != related_symbols.end();
                it++)
        {
            emit_use_statement_if_symbol_comes_from_module(*it);
        }

        indent();
        file << "IMPLICIT NONE\n";

        if (entry.is_function())
        {
            for (TL::ObjectList<TL::Symbol>::iterator it = related_symbols.begin();
                    it != related_symbols.end();
                    it++)
            {
                declare_symbol(*it);
            }
        }

        declare_everything_needed(statement_seq);

        // Could we improve the name of this function?
        TL::Symbol data_symbol = ::get_data_symbol_info(entry.get_scope().get_decl_context());
        if (data_symbol.is_valid())
        {
            walk(data_symbol.get_initialization());
        }
        
        // Could we improve the name of this function?
        TL::Symbol equivalence_symbol = get_equivalence_symbol_info(entry.get_scope().get_decl_context());
        if (equivalence_symbol.is_valid())
        {
            walk(equivalence_symbol.get_initialization());
        }

        walk(statement_seq);
        dec_indent();

        if (!internal_subprograms.is_null())
        {
            indent();
            file << "CONTAINS\n";

            inc_indent();
            walk(internal_subprograms);
            dec_indent();
        }
    }

    void FortranBase::visit(const Nodecl::FunctionCode& node)
    {
        TL::Symbol entry = node.get_symbol();
        Nodecl::Context context = node.get_statements().as<Nodecl::Context>();
        Nodecl::List statement_seq = context.get_in_context().as<Nodecl::List>();
        Nodecl::List internal_subprograms = node.get_internal_functions().as<Nodecl::List>();

        // Module procedures are only printed if we are in the current module
        if (state.current_module != TL::Symbol(entry.get_internal_symbol()->entity_specs.in_module))
            return;

        TL::Symbol old_sym = state.current_symbol;
        state.current_symbol = entry;

        if (get_codegen_status(entry) == CODEGEN_STATUS_DEFINED)
            return;
        set_codegen_status(entry, CODEGEN_STATUS_DEFINED);

        if (entry.is_fortran_main_program())
        {
            // // If it is __MAIN__ do not print the name
            std::string program_name = entry.get_name();

            if (program_name[0] == '_')
                program_name = "MAIN__";

            file << "PROGRAM " << program_name;
            inc_indent();

            codegen_procedure(entry, statement_seq, internal_subprograms);

            dec_indent();
            file << "END PROGRAM " << program_name;
        }
        else if (entry.is_function())
        {
            // codegen_procedure_declaration_header(visitor, entry);
            // codegen_procedure(visitor, entry, statement_seq, internal_subprograms);
            // codegen_procedure_declaration_footer(visitor, entry);
        }
        else
        {
            internal_error("Unexpected symbol kind %s", symbol_kind_name(entry.get_internal_symbol()));
        }

        state.current_symbol = old_sym;
    }

    void FortranBase::visit(const Nodecl::Context& node)
    {
    }

    void FortranBase::visit(const Nodecl::CompoundStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::ExpressionStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::ObjectInit& node)
    {
    }

    void FortranBase::visit(const Nodecl::Plus& node)
    {
    }

    void FortranBase::visit(const Nodecl::Neg& node)
    {
    }

    void FortranBase::visit(const Nodecl::LogicalNot& node)
    {
    }

    void FortranBase::visit(const Nodecl::Mul& node)
    {
    }

    void FortranBase::visit(const Nodecl::Div& node)
    {
    }

    void FortranBase::visit(const Nodecl::Add& node)
    {
    }

    void FortranBase::visit(const Nodecl::Minus& node)
    {
    }

    void FortranBase::visit(const Nodecl::LowerThan& node)
    {
    }

    void FortranBase::visit(const Nodecl::LowerOrEqualThan& node)
    {
    }

    void FortranBase::visit(const Nodecl::GreaterThan& node)
    {
    }

    void FortranBase::visit(const Nodecl::GreaterOrEqualThan& node)
    {
    }

    void FortranBase::visit(const Nodecl::LogicalAnd& node)
    {
    }

    void FortranBase::visit(const Nodecl::LogicalOr& node)
    {
    }

    void FortranBase::visit(const Nodecl::Power& node)
    {
    }

    void FortranBase::visit(const Nodecl::Concat& node)
    {
    }

    void FortranBase::visit(const Nodecl::ClassMemberAccess& node)
    {
    }

    void FortranBase::visit(const Nodecl::Range& node)
    {
    }

    void FortranBase::visit(const Nodecl::StringLiteral& node)
    {
    }

    void FortranBase::visit(const Nodecl::Text& node)
    {
    }

    void FortranBase::visit(const Nodecl::StructuredValue& node)
    {
    }

    void FortranBase::visit(const Nodecl::BooleanLiteral& node)
    {
    }

    void FortranBase::visit(const Nodecl::IntegerLiteral& node)
    {
    }

    void FortranBase::visit(const Nodecl::ComplexLiteral& node)
    {
    }

    void FortranBase::visit(const Nodecl::FloatingLiteral& node)
    {
    }

    void FortranBase::visit(const Nodecl::Symbol& node)
    {
    }

    void FortranBase::visit(const Nodecl::Assignment& node)
    {
    }

    void FortranBase::visit(const Nodecl::Equal& node)
    {
    }

    void FortranBase::visit(const Nodecl::Different& node)
    {
    }

    void FortranBase::visit(const Nodecl::Derreference& node)
    {
    }

    void FortranBase::visit(const Nodecl::Reference& node)
    {
    }

    void FortranBase::visit(const Nodecl::ParenthesizedExpression& node)
    {
    }

    void FortranBase::visit(const Nodecl::ArraySubscript& node)
    {
    }

    void FortranBase::visit(const Nodecl::FunctionCall& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranNamedPairSpec& node)
    {
    }

    void FortranBase::visit(const Nodecl::EmptyStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::IfElseStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::ReturnStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::LabeledStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::GotoStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::ForStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::WhileStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::LoopControl& node)
    {
    }

    void FortranBase::visit(const Nodecl::SwitchStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::CaseStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::DefaultStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::BreakStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::ContinueStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranIoSpec& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranPrintStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranWriteStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranReadStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranStopStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranPauseStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranComputedGotoStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranIoStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranOpenStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranCloseStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranAllocateStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranDeallocateStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranNullifyStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranArithmeticIfStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranLabelAssignStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranAssignedGotoStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranImpliedDo& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranData& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranEquivalence& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranAlternateReturnArgument& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranAlternateReturnStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranForall& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranWhere& node)
    {
    }

    void FortranBase::visit(const Nodecl::FortranBozLiteral& node)
    {
    }

    void FortranBase::visit(const Nodecl::FieldDesignator& node)
    {
    }

    void FortranBase::visit(const Nodecl::Conversion& node)
    {
    }

    void FortranBase::visit(const Nodecl::UnknownPragma& node)
    {
    }

    void FortranBase::visit(const Nodecl::PragmaCustomClause& node)
    {
    }

    void FortranBase::visit(const Nodecl::PragmaCustomLine& node)
    {
    }

    void FortranBase::visit(const Nodecl::PragmaCustomStatement& node)
    {
    }

    void FortranBase::visit(const Nodecl::PragmaCustomDirective& node)
    {
    }

    void FortranBase::visit(const Nodecl::PragmaClauseArg& node)
    {
    }

    void FortranBase::set_codegen_status(TL::Symbol sym, codegen_status_t status)
    {
        _codegen_status[sym] = status;
    }

    codegen_status_t FortranBase::get_codegen_status(TL::Symbol sym)
    {
        std::map<TL::Symbol, codegen_status_t>::iterator it = _codegen_status.find(sym);

        if (it == _codegen_status.end())
        {
            return CODEGEN_STATUS_NONE;
        }
        else
        {
            return it->second;
        }
    }

    void FortranBase::indent()
    {
        for (int i = 0; i < state._indent_level; i++)
        {
            file << "  ";
        }
    }

    void FortranBase::inc_indent(int n)
    {
        state._indent_level += n;
    }

    void FortranBase::dec_indent(int n)
    {
        state._indent_level -= n;
    }

    int FortranBase::get_indent_level()
    {
        return state._indent_level;
    }

    void FortranBase::set_indent_level(int n)
    {
        state._indent_level = n;
    }

}
