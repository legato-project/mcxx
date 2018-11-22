/*--------------------------------------------------------------------
  (C) Copyright 2006-2015 Barcelona Supercomputing Center
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


#include "tl-omp-lowering-utils.hpp"

#include "tl-scope.hpp"

#include "fortran03-typeutils.h"

#include "cxx-cexpr.h"
#include "cxx-driver-utils.h"

#include <string>
#include <fstream>
#include <iomanip>
namespace TL { namespace OpenMP { namespace Lowering { namespace Utils { namespace Fortran {

    Nodecl::NodeclBase get_lower_bound(Nodecl::NodeclBase expr, int dimension_num)
    {
        ERROR_CONDITION(!IS_FORTRAN_LANGUAGE, "This is only for Fortran", 0);
        TL::Source src;
        if (expr.is<Nodecl::ArraySubscript>())
        {
            expr = expr.as<Nodecl::ArraySubscript>().get_subscripted();
        }

        src << "LBOUND(" << as_expression(expr) << ", " << dimension_num << ")";

        return src.parse_expression(TL::Scope(CURRENT_COMPILED_FILE->global_decl_context));
    }

    Nodecl::NodeclBase get_upper_bound(Nodecl::NodeclBase expr, int dimension_num)
    {
        ERROR_CONDITION(!IS_FORTRAN_LANGUAGE, "This is only for Fortran", 0);
        TL::Source src;
        if (expr.is<Nodecl::ArraySubscript>())
        {
            expr = expr.as<Nodecl::ArraySubscript>().get_subscripted();
        }

        src << "UBOUND(" << as_expression(expr) << ", " << dimension_num << ")";

        return src.parse_expression(TL::Scope(CURRENT_COMPILED_FILE->global_decl_context));
    }

    Nodecl::NodeclBase get_size_for_dimension(
            const TL::DataReference& data_reference,
            TL::Type array_type,
            int fortran_dimension)
    {
        ERROR_CONDITION(!IS_FORTRAN_LANGUAGE, "This is only for Fortran", 0);
        Nodecl::NodeclBase n = array_type.array_get_size();

        // Let's try to get the size using SIZE
        if (n.is_null())
        {
            // Craft a SIZE if possible
            TL::Symbol sym = data_reference.get_base_symbol();

            if (sym.is_parameter()
                    && sym.get_type().no_ref().is_array()
                    && !sym.get_type().no_ref().array_requires_descriptor()
                    && fortran_dimension == fortran_get_rank_of_type(array_type.no_ref().get_internal_type()))
            {
                Nodecl::NodeclBase expr = data_reference;
                if (expr.is<Nodecl::ArraySubscript>())
                {
                    expr = expr.as<Nodecl::ArraySubscript>().get_subscripts();

                    expr = expr.as<Nodecl::List>()[0];

                    if (expr.is<Nodecl::Range>())
                    {
                        // Use the subscript
                        TL::Source src;
                        Nodecl::NodeclBase lower =  expr.as<Nodecl::Range>().get_lower().shallow_copy();
                        if (lower.is_null())
                        {
                            lower = const_value_to_nodecl(const_value_get_signed_int(1));
                        }
                        src << "(" << as_expression(expr.as<Nodecl::Range>().get_upper().shallow_copy()) << ")"
                            << " - "
                            << "(" << as_expression(lower) << ")"
                            << " + 1";
                        n = src.parse_expression(Scope(CURRENT_COMPILED_FILE->global_decl_context));
                    }
                    else if (fortran_dimension != 1)
                    {
                        n = const_value_to_nodecl(const_value_get_signed_int(1));
                    }
                }
                else
                {
                    internal_error("Assumed size array is not fully specified", 0);
                }
            }
            else
            {
                Source src;

                Nodecl::NodeclBase expr = data_reference;
                if (expr.is<Nodecl::ArraySubscript>())
                {
                    expr = expr.as<Nodecl::ArraySubscript>().get_subscripted();
                }

                src << "SIZE(" << as_expression(expr.shallow_copy()) << ", " << fortran_dimension << ")";

                n = src.parse_expression(Scope(CURRENT_COMPILED_FILE->global_decl_context));
            }
        }

        return n;
    }

    void preprocess_api(Nodecl::NodeclBase top_level)
    {
        ERROR_CONDITION(!IS_FORTRAN_LANGUAGE, "This is only for Fortran", 0);

        const char** old_preprocessor_options = CURRENT_CONFIGURATION->preprocessor_options;

        int num_orig_args = count_null_ended_array((void**)old_preprocessor_options);
        int num_args = num_orig_args;

        // -x c
        num_args += 2;

        // NULL ended
        num_args += 1;

        const char** preprocessor_options = new const char*[num_args];

        for (int i = 0;  i < num_orig_args; i++)
        {
            preprocessor_options[i] = old_preprocessor_options[i];
        }

        // We add -x c since we want /dev/null be preprocessed as an empty C file
        // FIXME - This is very gcc specific
        preprocessor_options[num_args - 3] = "-x";
        preprocessor_options[num_args - 2] = "c";
        preprocessor_options[num_args - 1] = NULL;

        CURRENT_CONFIGURATION->preprocessor_options = preprocessor_options;

        const char* output_filename = preprocess_file("/dev/null");

        delete[] preprocessor_options;

        // Restore old flags
        CURRENT_CONFIGURATION->preprocessor_options = old_preprocessor_options;

        TL::Source src;

        std::ifstream preproc_file(output_filename);

        if (preproc_file.is_open())
        {
            std::string str;

            while (preproc_file.good())
            {
                std::getline(preproc_file, str);
                src << str << "\n";
            }
            preproc_file.close();
        }
        else
        {
            fatal_error("Could not open Nanos6 include");
        }

        Source::source_language = SourceLanguage::C;

        Nodecl::NodeclBase new_tree = src.parse_global(top_level);
        // This is actually a top level tree!
        new_tree = Nodecl::TopLevel::make(new_tree);
        // FIXME - keep this?

        Source::source_language = SourceLanguage::Current;
    }
} } } } }

