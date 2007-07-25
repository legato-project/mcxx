/*
    Mercurium C/C++ Compiler
    Copyright (C) 2006-2007 - Roger Ferrer Ibanez <roger.ferrer@bsc.es>
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
#include <cstdio>
#include <vector>
#include <dlfcn.h>
#include "cxx-driver.h"
#include "cxx-utils.h"
#include "cxx-compilerphases.hpp"
#include "tl-compilerphase.hpp"
#include "tl-ast.hpp"
#include "tl-scopelink.hpp"
#include "tl-refptr.hpp"

/*
   void *dlopen(const char *filename, int flag);

   char *dlerror(void);

   void *dlsym(void *handle, const char *symbol);

   int dlclose(void *handle);
 */

namespace TL
{
    class CompilerPhaseRunner
    {
        private:
            typedef std::vector<TL::CompilerPhase*> compiler_phases_t;
            static compiler_phases_t compiler_phases;
        public :
            static void start_compiler_phase_execution(translation_unit_t* translation_unit)
            {
                // Create the DTO
                TL::DTO dto;

                RefPtr<TL::AST_t> ast(new TL::AST_t(translation_unit->parsed_tree));
                dto.set_object("translation_unit", ast);

                RefPtr<TL::ScopeLink> scope(new TL::ScopeLink(translation_unit->scope_link));
                dto.set_object("scope_link", scope);

                for (compiler_phases_t::iterator it = compiler_phases.begin();
                        it != compiler_phases.end();
                        it++)
                {
                    TL::CompilerPhase* phase = (*it);
                    phase->run(dto);

                    if (phase->get_phase_status() != CompilerPhase::PHASE_STATUS_OK)
                    {
                        // Ideas to improve this are welcome :)
                        running_error("Phase '%s' did not end successfully. Ending compilation", 
                                phase->get_phase_name().c_str());
                    }

                }
            }

            static void add_compiler_phase(TL::CompilerPhase* new_phase)
            {
                compiler_phases.push_back(new_phase);
            }

            static void phases_help(void)
            {
                if (!compiler_phases.empty())
                {
                    std::cerr << std::endl;
                    std::cerr << "Loaded compiler phases in this profile (in the order they will be run)" << std::endl;
                    std::cerr << std::endl;
                    for (compiler_phases_t::iterator it = compiler_phases.begin();
                            it != compiler_phases.end();
                            it++)
                    {
#define BLANK_INDENT "   "
                        TL::CompilerPhase* phase = (*it);

                        std::cerr 
                            << "Phase: " << phase->get_phase_name() << std::endl 
                            << std::endl
                            << BLANK_INDENT << phase->get_phase_description() << std::endl
                            << std::endl;

                        // TODO - For every registered parameter, print it and its description
                        std::vector<CompilerPhaseParameter*> parameters = phase->get_parameters();
                        if (!parameters.empty())
                        {
                            for (std::vector<CompilerPhaseParameter*>::iterator it = parameters.begin();
                                    it != parameters.end();
                                    it++)
                            {
                                CompilerPhaseParameter *parameter(*it);
                                std::cerr << BLANK_INDENT << "--variable=" << parameter->name() << std::endl;
                                std::cerr << BLANK_INDENT << parameter->description() << std::endl;
                                std::cerr << BLANK_INDENT << "Default value : '" << parameter->get_value() << "'" << std::endl;
                                std::cerr << std::endl;
                            }
                        }
                        else
                        {
                            std::cerr << BLANK_INDENT << "No parameters registered by the phase" << std::endl;
                        }

                        std::cerr << std::endl;
#undef BLANK_INDENT
                    }
                }
                else
                {
                    std::cerr << "No phases loaded in this profile" << std::endl;
                }
            }

            static void phases_update_parameters(void)
            {
                for (compiler_phases_t::iterator it = compiler_phases.begin();
                        it != compiler_phases.end();
                        it++)
                {
                    TL::CompilerPhase* phase = (*it);
                    std::vector<CompilerPhaseParameter*> parameters = phase->get_parameters();
                    
                    // This is blatantly inefficient, I know
                    for (int i = 0; i < compilation_process.num_external_vars; i++)
                    {
                        external_var_t* ext_var = compilation_process.external_vars[i];

                        for (std::vector<CompilerPhaseParameter*>::iterator it = parameters.begin();
                                it != parameters.end();
                                it++)
                        {
                            CompilerPhaseParameter* param(*it);

                            if (param->name() == std::string(ext_var->name))
                            {
                                param->set_value(ext_var->value);
                            }
                        }
                    }
                }
            }
    };

    CompilerPhaseRunner::compiler_phases_t CompilerPhaseRunner::compiler_phases;
}


extern "C"
{
    void load_compiler_phases_cxx(void)
    {
        int num = CURRENT_CONFIGURATION(num_compiler_phases);

        int i;
        for (i = 0; i < num; i++)
        {
            char* library_name = CURRENT_CONFIGURATION(compiler_phases[i]);

             DEBUG_CODE()
             {
                fprintf(stderr, "Loading compiler phase '%s'\n", library_name);
            }
            void* handle = dlopen(library_name, RTLD_NOW | RTLD_LOCAL);

            if (handle == NULL)
            {
                fprintf(stderr, "Cannot open '%s'.\nReason: '%s'\n", library_name, dlerror());
                fprintf(stderr, "Skipping '%s'\n", library_name);
                continue;
            }
            DEBUG_CODE()
            {
                fprintf(stderr, "'%s' properly loaded\n", library_name);
            }

            // Now get the function
            DEBUG_CODE()
            {
                fprintf(stderr, "Getting the factory function 'give_compiler_phase_object'\n");
            }
            void* factory_function_sym = dlsym(handle, "give_compiler_phase_object");

            if (factory_function_sym == NULL)
            {
                fprintf(stderr, "Cannot get the factory function 'give_compiler_phase_object'\n");
                fprintf(stderr, "%s\n", dlerror());
                fprintf(stderr, "Skipping\n");
                return;
            }
            DEBUG_CODE()
            {
                fprintf(stderr, "Factory function obtained\n");
            }

            typedef TL::CompilerPhase*(*factory_function_t)(void);
            factory_function_t factory_function = (factory_function_t) factory_function_sym;

            TL::CompilerPhase* new_phase = (factory_function)();

            DEBUG_CODE()
            {
                fprintf(stderr, "Adding '%s' phase object to the compiler pipeline\n", library_name);
            }

            // If the phase did not set its own phase name, use the DSO name
            if (new_phase->get_phase_name() == "")
            {
                new_phase->set_phase_name(library_name);
            }
            // Likewise for the phase description
            if (new_phase->get_phase_description() == "")
            {
                new_phase->set_phase_description("No description available");
            }

            TL::CompilerPhaseRunner::add_compiler_phase(new_phase);
        }
    }

    void start_compiler_phase_execution(translation_unit_t* translation_unit)
    {
        DEBUG_CODE()
        {
            fprintf(stderr, "Starting the compiler phase pipeline\n");
        }
        TL::CompilerPhaseRunner::phases_update_parameters();
        TL::CompilerPhaseRunner::start_compiler_phase_execution(translation_unit);
    }

    void phases_help(void)
    {
        TL::CompilerPhaseRunner::phases_help();
    }
}
