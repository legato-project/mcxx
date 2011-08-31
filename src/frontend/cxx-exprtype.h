/*--------------------------------------------------------------------
  (C) Copyright 2006-2011 Barcelona Supercomputing Center 
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



#ifndef CXX_EXPRTYPE_H
#define CXX_EXPRTYPE_H

#include "libmcxx-common.h"
#include "cxx-ast-decls.h"
#include "cxx-exprtype-decls.h"
#include "cxx-scope-decls.h"
#include "cxx-buildscope-decls.h"
#include "cxx-typeutils.h"
#include "cxx-cexpr.h"
#include "cxx-nodecl-output.h"

MCXX_BEGIN_DECLS

/*
 * Computes the type of an expression
 */

LIBMCXX_EXTERN AST advance_expression_nest(AST expr);
LIBMCXX_EXTERN AST advance_expression_nest_flags(AST expr, char advance_parentheses);

LIBMCXX_EXTERN char can_be_called_with_number_of_arguments(scope_entry_t *entry, int num_arguments);

LIBMCXX_EXTERN char check_expression(AST a, decl_context_t decl_context, nodecl_t* nodecl_output);

LIBMCXX_EXTERN char check_expression_list(AST expression_list, decl_context_t decl_context, nodecl_t* nodecl_output);

LIBMCXX_EXTERN char check_initialization(AST initializer, decl_context_t decl_context, type_t* declared_type, nodecl_t* nodecl_output);

// Used in some TL phases, do not remove
LIBMCXX_EXTERN void check_initializer_clause(AST initializer, decl_context_t decl_context, type_t* declared_type, nodecl_t* nodecl_output);

LIBMCXX_EXTERN char check_default_initialization(scope_entry_t* entry, decl_context_t decl_context, 
        const char* filename, int line,
        scope_entry_t** constructor);

LIBMCXX_EXTERN char check_default_initialization_and_destruction_declarator(scope_entry_t* entry, decl_context_t decl_context,
        const char* filename,
        int line);

LIBMCXX_EXTERN char check_copy_constructor(scope_entry_t* entry,
        decl_context_t decl_context,
        char has_const,
        const char* filename, int line,
        scope_entry_t** constructor);

LIBMCXX_EXTERN char check_copy_assignment_operator(scope_entry_t* entry,
        decl_context_t decl_context,
        char has_const,
        const char* filename, int line,
        scope_entry_t** constructor);

LIBMCXX_EXTERN unsigned long long exprtype_used_memory(void);

LIBMCXX_EXTERN scope_entry_list_t* unfold_and_mix_candidate_functions(
        scope_entry_list_t* result_from_lookup,
        scope_entry_list_t* builtin_list,
        type_t** argument_types,
        int num_arguments,
        decl_context_t decl_context,
        const char *filename,
        int line,
        template_parameter_list_t *explicit_template_parameters
        );

LIBMCXX_EXTERN type_t* compute_type_for_type_id_tree(AST type_id, decl_context_t decl_context);

LIBMCXX_EXTERN scope_entry_t* get_std_initializer_list_template(decl_context_t decl_context, 
        const char* filename,
        int line, 
        char mandatory);

LIBMCXX_EXTERN type_t* actual_type_of_conversor(scope_entry_t* conv);

LIBMCXX_EXTERN void diagnostic_candidates(scope_entry_list_t* entry_list, const char* filename, int line);

LIBMCXX_EXTERN void ensure_function_is_emitted(scope_entry_t* entry,
        const char* filename,
        int line);

LIBMCXX_EXTERN char check_nontype_template_argument_expression(AST expression, decl_context_t decl_context, nodecl_t*);

// Like nodecl_make_function_call but takes care of virtual function calls
LIBMCXX_EXTERN nodecl_t cxx_nodecl_make_function_call(nodecl_t, nodecl_t, type_t*, const char* filename, int line);
 
// Not meant to be LIBMCXX_EXTERN (used by cxx-cuda.c)
void check_function_arguments(AST arguments, decl_context_t decl_context, nodecl_t* nodecl_output);
void check_nodecl_function_call(nodecl_t nodecl_called, nodecl_t nodecl_argument_list, decl_context_t decl_context, nodecl_t* nodecl_output);

MCXX_END_DECLS

#endif
