/*
    Mercurium C/C++ Compiler
    Copyright (C) 2006-2009 - Roger Ferrer Ibanez <roger.ferrer@bsc.es>
    Barcelona Supercomputing Center - Centro Nacional de Supercomputacion
    Universitat Politecnica de Catalunya

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "hlt-fusion.hpp"
#include "hlt-exception.hpp"

using namespace TL::HLT;

TL::Source LoopFusion::get_source()
{
    return do_fusion();
}

TL::Source LoopFusion::do_fusion()
{
    if (_for_stmt_list.size() == 1)
    {
        return _for_stmt_list[0].prettyprint();
    }

    Source result;
    Source for_header, integrated_bodies;

    result
        << for_header
        << "{"
        << integrated_bodies
        << "}"
        ;

    if (!_for_stmt_list[0].is_regular_loop())
    {
        throw HLTException(_for_stmt_list[0], "is not a regular loop");
    }

    for_header
        << "for("
        // << _for_stmt_list[0].get_iterating_init() 
        // << _for_stmt_list[0].get_iterating_condition()
        // << _for_stmt_list[0].get_iterating_expression() 
        << ")"
        ;

    TL::IdExpression ref_induction_var = _for_stmt_list[0].get_induction_variable();

    for (TL::ObjectList<TL::ForStatement>::iterator it = _for_stmt_list.begin();
            it != _for_stmt_list.end();
            it++)
    {
        TL::ForStatement for_stmt(*it);

        if (!for_stmt.is_regular_loop())
        {
            throw HLTException(for_stmt, "is not a regular loop");
        }

        TL::IdExpression current_induction_var = for_stmt.get_induction_variable();

        // Check if we have to change something
        if (current_induction_var.prettyprint() == ref_induction_var.prettyprint())
        {
            // Easy case: nothing must be changed
            integrated_bodies << for_stmt.get_loop_body();
        }
        else
        {
            // We have to fix the code 
            TL::ReplaceIdExpression replace;
            replace.add_replacement(current_induction_var.get_symbol(), ref_induction_var.prettyprint());

            integrated_bodies << replace.replace(for_stmt.get_loop_body());
        }
    }
}

LoopFusion::LoopFusion(ObjectList<ForStatement> for_stmt_list)
    : _for_stmt_list(for_stmt_list)
{
    if (_for_stmt_list.empty())
    {
        throw HLTException("empty list of for-statements");
    }
}

LoopFusion TL::HLT::loop_fusion(TL::ObjectList<TL::ForStatement> for_statement)
{
    return LoopFusion(for_statement);
}
