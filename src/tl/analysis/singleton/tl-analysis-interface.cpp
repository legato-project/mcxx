/*--------------------------------------------------------------------
  (C) Copyright 2006-2013 Barcelona Supercomputing Center
                          Centro Nacional de Supercomputacion
  This file is part of Mercurium C/C++ source-to-source compiler.
  See AUTHORS file in the top level directory for information
  regarding developers and contributors.
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
 
#include "tl-analysis-interface.hpp"

//#include "cxx-process.h"
//#include "tl-analysis-utils.hpp"
//#include "tl-analysis-static-info.hpp"
//#include "tl-expression-reduction.hpp"
//#include "tl-use-def.hpp"
 
namespace TL  {
namespace Analysis {


    AnalysisInterface::AnalysisInterface( ) {}

    AnalysisInterface::AnalysisInterface( const Nodecl::NodeclBase& n, WhichAnalysis analysis_mask,
            WhereAnalysis nested_analysis_mask, int nesting_level )
        : AnalysisStaticInfo(n, analysis_mask, nested_analysis_mask, nesting_level)
    {
    }

    AnalysisInterface::~AnalysisInterface( ) {}


    Node* AnalysisInterface::retrieve_scope_node_from_nodecl(const Nodecl::NodeclBase& scope,
            ExtensibleGraph* pcfg) 
    {
        nodecl_to_node_map_t::const_iterator it = _scope_nodecl_to_node_map.find(scope);

        if(it != _scope_nodecl_to_node_map.end())
        {
            return it->second;
        }
        else // Insert new scope in the map
        {
            Node* scope_node = pcfg->find_nodecl_pointer(scope);
            ERROR_CONDITION(scope_node==NULL, "No PCFG node found for scope Nodecl '%s:%s'. \n",
                    scope.get_locus_str().c_str(), scope.prettyprint().c_str());

            _scope_nodecl_to_node_map.insert(nodecl_to_node_pair_t(scope, scope_node));

            return scope_node;
        }
    }

    ExtensibleGraph* AnalysisInterface::retrieve_pcfg_from_func(const Nodecl::NodeclBase& n) const
    {
        TL::Symbol func_sym = Nodecl::Utils::get_enclosing_function(n);

        if(func_sym.is_valid() && n.is<Nodecl::FunctionCode>())
        {
            WARNING_MESSAGE("FunctionCode nested in another FunctionCode. Retrieving enclosing FunctionCode PCFG Node.\n", 0);
        }

        // If n is a FunctionCode, maybe could be FunctionCode nested in another FunctionCode
        if(!func_sym.is_valid() && n.is<Nodecl::FunctionCode>())
            func_sym = n.as<Nodecl::FunctionCode>().get_symbol();

        ERROR_CONDITION(!func_sym.is_valid(), 
                "Invalid Nodecl '%s' on expecting non top-level nodecls\n", n.prettyprint().c_str());

        Nodecl::NodeclBase func = func_sym.get_function_code();

        // TODO Check 'func' is a valid nodecl

        nodecl_to_pcfg_map_t::const_iterator it =_func_to_pcfg_map.find(func);
        ERROR_CONDITION(it == _func_to_pcfg_map.end(), 
                "No PCFG found corresponding to function %s\n", func.get_symbol().get_name().c_str());
        return it->second;
    }

    /* Example:
     *
     * if( i < 5 ) k = N;
     * else k = M;
     *
     * k; <--- target
     *
     * reach_defs_depends_on_iv analyzes reaching definitions, 'N' and 'M' in the example,
     * and their enclosing scopes, 'if( i < 5 )' in the example, to determine if any IV
     * is involved in the evolution of 'k'.
     *
     * In this example, being 'i' and IV, the result is 'true'.
     * Note the different between this query and reach_defs_contain_iv, where the result
     * would be 'false'.
     */
 
    // Beter name: evolution_depends_on_iv??
    DEPRECATED static bool reach_defs_depend_on_iv_rec(const Nodecl::NodeclBase& n, const ObjectList<Nodecl::NodeclBase>& ivs, ExtensibleGraph* pcfg)
    {
        if(n.is<Nodecl::Undefined>())
            return false;
            
        // Get reaching definitions for 'n' in its corresponding node
        Node* n_node = pcfg->find_nodecl_pointer(n);
        Utils::ext_sym_map reach_defs = n_node->get_reaching_definitions_in();
        
        // Get the PCFG nodes where the reaching definitions where produced
        ObjectList<Node*> reach_defs_nodes;
        for(Utils::ext_sym_map::iterator it = reach_defs.begin(); it != reach_defs.end(); ++it)
            if(!it->second.first.is<Nodecl::Undefined>())
            {
                Nodecl::NodeclBase stmt_reach_def = it->second.second.is_null() ? it->second.first : it->second.second;
                reach_defs_nodes.append(pcfg->find_nodecl_pointer(stmt_reach_def));
            }
        
        // For each reaching definition node:
        // 1.- check whether any of its outer nodes depend on an induction variable
        bool depends_on_iv = false;
        for(ObjectList<Node*>::iterator it = reach_defs_nodes.begin(); it != reach_defs_nodes.end(); ++it)
        {
            Node* outer_node = (*it)->get_outer_node();
            while(outer_node!=NULL)
            {
                if(outer_node->is_ifelse_statement() || outer_node->is_loop_node())
                {
                    Node* cond = outer_node->get_condition_node();
                    ObjectList<Nodecl::NodeclBase> stmts = cond->get_statements();
                    for(ObjectList<Nodecl::NodeclBase>::iterator itt = stmts.begin(); itt != stmts.end(); ++itt)
                    {
                        for(ObjectList<Nodecl::NodeclBase>::const_iterator ittt = ivs.begin(); ittt != ivs.end(); ++ittt)
                        {
                            if(Nodecl::Utils::stmtexpr_contains_nodecl_structurally(*itt, *ittt))
                            {
                                depends_on_iv = true;
                                goto end_depends;
                            }
                        }
                    }
                }
            }
        }
        // 2.- otherwise, recursively check the variables in the RHS of the reaching definition
        for(Utils::ext_sym_map::iterator it = reach_defs.begin(); it != reach_defs.end(); ++it)
        {
            Nodecl::NodeclBase stmt_reach_def = it->second.second.is_null() ? it->second.first : it->second.second;
            ObjectList<Nodecl::NodeclBase> vars = Nodecl::Utils::get_all_memory_accesses(stmt_reach_def);
            for(ObjectList<Nodecl::NodeclBase>::iterator itt = vars.begin(); itt != vars.end(); ++itt)
            {
                if(reach_defs_depend_on_iv_rec(*itt, ivs, pcfg))
                {
                    depends_on_iv = true;
                    goto end_depends;
                }
            }
        }
        
end_depends:
        return depends_on_iv;
    }
 
    DEPRECATED bool AnalysisInterface::reach_defs_depend_on_iv(const Nodecl::NodeclBase& scope, const Nodecl::NodeclBase& n)
    {
        if(Nodecl::Utils::nodecl_is_literal(n))
            return true;
        
        bool result = false;
 
        ExtensibleGraph* pcfg = retrieve_pcfg_from_func(scope);
        
        // Get the induction variables involved in the scope
        Node* scope_node = pcfg->find_nodecl_pointer(scope);
        if(!scope_node->is_loop_node())
            return result;
        ObjectList<Utils::InductionVariableData*> ivs = scope_node->get_induction_variables();
        if(ivs.empty())
            return result;
        ObjectList<Nodecl::NodeclBase> nodecl_ivs;
        for(ObjectList<Utils::InductionVariableData*>::iterator it = ivs.begin(); it != ivs.end(); ++it)
            nodecl_ivs.append((*it)->get_variable().get_nodecl());
        
        result = reach_defs_depend_on_iv_rec(n, nodecl_ivs, pcfg);
        
        return result;
    }

    bool AnalysisInterface::variable_is_constant_at_statement(
            const Nodecl::NodeclBase& scope, const Nodecl::NodeclBase& n)
    {
        if(Nodecl::Utils::nodecl_is_literal(n))
            return true;
        
        // Retrieve pcfg
        ExtensibleGraph* pcfg = retrieve_pcfg_from_func(scope);
        // Retrieve scope
        Node* scope_node = retrieve_scope_node_from_nodecl(scope, pcfg);

        // Retrieve node
        Node* stmt_node = pcfg->find_nodecl_pointer(n);
        ERROR_CONDITION(stmt_node==NULL, "No PCFG node found for nodecl '%s:%s'. \n",
                n.get_locus_str().c_str(), n.prettyprint().c_str());

        return variable_is_constant_at_statement(scope_node, stmt_node, n, pcfg);
    }

    bool AnalysisInterface::variable_is_constant_at_statement(
            Node* const scope_node,
            Node* const stmt_node,
            const Nodecl::NodeclBase& n,
            ExtensibleGraph* const pcfg)
    {
        if(Nodecl::Utils::nodecl_is_literal(n))
            return true;
        
        Utils::ext_sym_map reach_defs_in = stmt_node->get_reaching_definitions_in();
        ERROR_CONDITION(reach_defs_in.find(n)==reach_defs_in.end(),
                "No reaching definition arrives for nodecl %s.\n", n.prettyprint().c_str());
        
        std::pair<Utils::ext_sym_map::iterator, Utils::ext_sym_map::iterator> bounds = reach_defs_in.equal_range(n);
        Utils::ext_sym_map::iterator rd_it = bounds.first;
        while(rd_it != bounds.second)
        {
            std::pair<Nodecl::NodeclBase, Nodecl::NodeclBase> rd_value = rd_it->second;
            ++rd_it;
            
            if(rd_value.first.is<Nodecl::Undefined>())
                continue;
            
            // Get the PCFG nodes where the reaching definitions where produced
            Nodecl::NodeclBase stmt_reach_def = rd_value.second.is_null() ? rd_value.first : rd_value.second;
            Node* reach_defs_node = pcfg->find_nodecl_pointer(stmt_reach_def);
            if(ExtensibleGraph::node_contains_node(scope_node, stmt_node))
            {
                Node* control_structure = ExtensibleGraph::get_enclosing_control_structure(reach_defs_node);
                if((control_structure != NULL) && 
                   (ExtensibleGraph::node_contains_node(scope_node, control_structure) || (scope_node==control_structure)))
                {
                    Node* cond_node = control_structure->get_condition_node();
                    ObjectList<Nodecl::NodeclBase> stmts = cond_node->get_statements();
                    for(ObjectList<Nodecl::NodeclBase>::const_iterator it = stmts.begin(); it != stmts.end(); ++it)
                    {
                        const ObjectList<Nodecl::NodeclBase> mem_accesses = Nodecl::Utils::get_all_memory_accesses(*it);
                        for(ObjectList<Nodecl::NodeclBase>::const_iterator itt = mem_accesses.begin(); itt != mem_accesses.end(); ++itt)
                        {
                            //TODO: translate is_constant
                            if(!is_constant(scope_node->get_graph_related_ast(), *itt) || 
                               !variable_is_constant_at_statement(scope_node, cond_node, *itt, pcfg))
                                return false;
                        }
                    }
                }
            }
        }
        
        return true;
    }
}
}