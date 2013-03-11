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


#include "tl-devices.hpp"
#include "nanox-cuda.hpp"
#include "tl-nanos.hpp"
#include "tl-multifile.hpp"
#include "tl-compilerpipeline.hpp"
#include "cxx-profile.h"

#include "codegen-phase.hpp"
#include "codegen-cuda.hpp"
#include "cxx-cexpr.h"

#include <errno.h>
#include "cxx-driver-utils.h"

using namespace TL;
using namespace TL::Nanox;

static std::string cuda_outline_name(const std::string & name)
{
    return "gpu_" + name;
}

bool DeviceCUDA::is_gpu_device() const
{
    return true;
}

void DeviceCUDA::generate_ndrange_additional_code(
        const TL::Symbol& called_task,
        const TL::Symbol& unpacked_function,
        const TL::ObjectList<Nodecl::NodeclBase>& ndrange_args,
        TL::Source& code_ndrange)
{
    int num_args_ndrange = ndrange_args.size();
    Nodecl::Utils::SimpleSymbolMap translate_parameters_map;

    TL::ObjectList<TL::Symbol> parameters_called = called_task.get_function_parameters();
    TL::ObjectList<TL::Symbol> parameters_unpacked = unpacked_function.get_function_parameters();
    ERROR_CONDITION(parameters_called.size() != parameters_unpacked.size(), "Code unreachable", 0);

    int num_params = parameters_called.size();
    for (int i = 0; i < num_params; ++i)
    {
        translate_parameters_map.add_map(
                parameters_called[i],
                parameters_unpacked[i]);
    }

    TL::ObjectList<Nodecl::NodeclBase> new_ndrange;
    for (int i = 0; i < num_args_ndrange; ++i)
    {

        new_ndrange.append(Nodecl::Utils::deep_copy(
                    ndrange_args[i],
                    unpacked_function.get_related_scope(),
                    translate_parameters_map));
    }

    ERROR_CONDITION(!new_ndrange[0].is_constant(), "The first argument of the 'ndrange' clause must be a literal", 0);

    int num_dim = const_value_cast_to_4(new_ndrange[0].get_constant());

    ERROR_CONDITION(num_dim < 1 || num_dim > 3, "invalid number of dimensions for 'ndrange' clause. Valid values: 1, 2 and 3." , 0);

    bool check_dim = !(new_ndrange[num_args_ndrange - 1].is_constant()
            && const_value_is_string(new_ndrange[num_args_ndrange - 1].get_constant())
            && (strcmp(const_value_string_unpack_to_string(new_ndrange[num_args_ndrange-1].get_constant()),"noCheckDim") == 0));

    ERROR_CONDITION(((num_dim * 2) + 1 + !check_dim) != num_args_ndrange, "invalid number of arguments for 'ndrange' clause", 0);

    code_ndrange << "dim3 dimGrid;";
    code_ndrange << "dim3 dimBlock;";
    const char* field[3] = { "x", "y", "z"};
    for (int i = 1; i <= 3; ++i)
    {
        if (check_dim)
        {
            if (i <= num_dim)
            {
                code_ndrange << "dimBlock." << field[i-1] << " = "
                    << "(("
                    << as_expression(new_ndrange[i])
                    << " < " << as_expression(new_ndrange[num_dim + i])
                    << ") ? (" << as_expression(new_ndrange[i])
                    << ") : (" << as_expression(new_ndrange[num_dim + i])
                    << "));";

                code_ndrange << "dimGrid."  << field[i-1] << " = "
                    << "(("
                    << as_expression(new_ndrange[i])
                    << " < " << as_expression(new_ndrange[num_dim + i])
                    << ") ? 1 : (("
                    << as_expression(new_ndrange[i]) << "/" << as_expression(new_ndrange[num_dim + i])
                    << ") + ((" << as_expression(new_ndrange[i]) << " %  " << as_expression(new_ndrange[num_dim + i])
                    << " == 0) ? 0 : 1)));";
            }
            else
            {
                code_ndrange << "dimBlock." << field[i-1] << " = 1;";
                code_ndrange << "dimGrid."  << field[i-1] << " = 1;";
            }
        }
        else
        {
            if (i <= num_dim)
            {
                code_ndrange << "dimBlock." << field[i-1] << " = " << as_expression(new_ndrange[num_dim + i]) << ";";
                code_ndrange << "dimGrid."  << field[i-1] << " = " << as_expression(new_ndrange[i]) << "/" << as_expression(new_ndrange[num_dim + i]) << ";";
            }
            else
            {
                code_ndrange << "dimBlock." << field[i-1] << " = 1;";
                code_ndrange << "dimGrid."  << field[i-1] << " = 1;";
            }
        }
    }

}

void DeviceCUDA::generate_ndrange_kernel_call(
        const Scope& scope,
        const Nodecl::NodeclBase& original_statements,
        Nodecl::NodeclBase& output_statements)
{
    Nodecl::NodeclBase function_call_nodecl =
        original_statements.as<Nodecl::List>().begin()->as<Nodecl::ExpressionStatement>().get_nest();

    ObjectList<Nodecl::NodeclBase> cuda_kernel_config;
    Symbol dim_grid  = scope.get_symbol_from_name("dimGrid");
    Symbol dim_block = scope.get_symbol_from_name("dimBlock");
    Symbol exec_stream = scope.get_symbol_from_name("nanos_get_kernel_execution_stream");
    ERROR_CONDITION(!dim_grid.is_valid() || !dim_block.is_valid() || !exec_stream.is_valid(), "Unreachable code", 0);

    cuda_kernel_config.append(
            Nodecl::Symbol::make(dim_grid,
                original_statements.get_filename(),
                original_statements.get_line()));

    cuda_kernel_config.append(
            Nodecl::Symbol::make(dim_block,
                original_statements.get_filename(),
                original_statements.get_line()));

    cuda_kernel_config.append(
            Nodecl::IntegerLiteral::make(
                TL::Type::get_int_type(),
                const_value_get_zero(TL::Type::get_int_type().get_size(), /* sign */ 1),
                original_statements.get_filename(),
                original_statements.get_line()));

    cuda_kernel_config.append(
            Nodecl::FunctionCall::make(
                Nodecl::Symbol::make(
                    exec_stream,
                    original_statements.get_filename(),
                    original_statements.get_line()),
                /* arguments */ nodecl_null(),
                /* alternate_name */ nodecl_null(),
                /* function_form */ nodecl_null(),
                TL::Type::get_void_type(),
                original_statements.get_filename(),
                original_statements.get_line()));

    Nodecl::NodeclBase kernell_call =
        Nodecl::CudaKernelCall::make(
                Nodecl::List::make(cuda_kernel_config),
                function_call_nodecl,
                TL::Type::get_void_type(),
                original_statements.get_filename(),
                original_statements.get_line());

    Nodecl::NodeclBase expression_stmt =
        Nodecl::ExpressionStatement::make(
                kernell_call,
                original_statements.get_filename(),
                original_statements.get_line());

    // In this case, we should change the output statements!
    output_statements = expression_stmt;
}

// This visitor completes the configuration of every cuda function task
class UpdateKernelConfigsVisitor : public Nodecl::ExhaustiveVisitor<void>
{
    public:
        void visit(const Nodecl::CudaKernelCall& node)
        {
            Nodecl::List kernel_config = node.get_kernel_config().as<Nodecl::List>();

            ERROR_CONDITION(kernel_config.size() < 2
                    || kernel_config.size() > 4,
                    "A kernel call configuration must have between 2 and 4 parameters", 0);

            if (kernel_config.size() == 2
                    || kernel_config.size() == 3)
            {
                // We should complete the kernel configuration
                if (kernel_config.size() == 2)
                {
                    // Append to the kernel configuration the size of shared memory (0, in this case)
                    kernel_config.append(
                            Nodecl::IntegerLiteral::make(
                                TL::Type::get_int_type(),
                                const_value_get_zero(TL::Type::get_int_type().get_size(), /* sign */ 1),
                                node.get_filename(),
                                node.get_line()));
                }

                Symbol exec_stream =
                    node.retrieve_context().get_symbol_from_name("nanos_get_kernel_execution_stream");

                ERROR_CONDITION(!exec_stream.is_valid(), "Unreachable code", 0);

                // Append to the kernel configuration the stream
                kernel_config.append(
                        Nodecl::FunctionCall::make(
                            Nodecl::Symbol::make(
                                exec_stream,
                                node.get_filename(),
                                node.get_line()),
                            /* arguments */ nodecl_null(),
                            /* alternate_name */ nodecl_null(),
                            /* function_form */ nodecl_null(),
                            TL::Type::get_void_type(),
                            node.get_filename(),
                            node.get_line()));
            }
        }
};

void DeviceCUDA::update_all_kernel_configurations(Nodecl::NodeclBase task_code)
{
    UpdateKernelConfigsVisitor update_kernel_visitor;
    update_kernel_visitor.walk(task_code);
}

void DeviceCUDA::create_outline(CreateOutlineInfo &info,
        Nodecl::NodeclBase &outline_placeholder,
        Nodecl::NodeclBase &output_statements,
        Nodecl::Utils::SymbolMap* &symbol_map)
{
    if (IS_FORTRAN_LANGUAGE)
        running_error("Fortran for CUDA devices is not supported yet\n", 0);

    // Unpack DTO
    const std::string& device_outline_name = cuda_outline_name(info._outline_name);
    const Nodecl::NodeclBase& original_statements = info._original_statements;

    // This symbol is only valid for function tasks
    const TL::Symbol& called_task = info._called_task;
    bool is_function_task = called_task.is_valid();

    output_statements = original_statements;

    ERROR_CONDITION(is_function_task && !called_task.is_function(),
            "The '%s' symbol is not a function", called_task.get_name().c_str());

    TL::Symbol current_function =
        original_statements.retrieve_context().get_decl_context().current_scope->related_entry;

    if (current_function.is_nested_function())
    {
        if (IS_C_LANGUAGE || IS_CXX_LANGUAGE)
            running_error("%s: error: nested functions are not supported\n",
                    original_statements.get_locus().c_str());
    }

    Source unpacked_arguments, private_entities;

    TL::ObjectList<OutlineDataItem*> data_items = info._data_items;
    for (TL::ObjectList<OutlineDataItem*>::iterator it = data_items.begin();
            it != data_items.end();
            it++)
    {
        switch ((*it)->get_sharing())
        {
            case OutlineDataItem::SHARING_PRIVATE:
                {
                    if (IS_CXX_LANGUAGE)
                    {
                        // We need the declarations of the private symbols!
                        private_entities << as_statement(Nodecl::CxxDef::make(Nodecl::NodeclBase::null(), (*it)->get_symbol()));
                    }
                    break;
                }
            case OutlineDataItem::SHARING_SHARED:
            case OutlineDataItem::SHARING_CAPTURE:
            case OutlineDataItem::SHARING_CAPTURE_ADDRESS:
                {
                    TL::Type param_type = (*it)->get_in_outline_type();

                    Source argument;
                    if (IS_C_LANGUAGE || IS_CXX_LANGUAGE)
                    {
                        // Normal shared items are passed by reference from a pointer,
                        // derreference here
                        if ((*it)->get_sharing() == OutlineDataItem::SHARING_SHARED
                                && !(IS_CXX_LANGUAGE && (*it)->get_symbol().get_name() == "this"))
                        {
                            argument << "*(args." << (*it)->get_field_name() << ")";
                        }
                        // Any other thing is passed by value
                        else
                        {
                            argument << "args." << (*it)->get_field_name();
                        }

                        if (IS_CXX_LANGUAGE
                                && (*it)->get_allocation_policy() == OutlineDataItem::ALLOCATION_POLICY_TASK_MUST_DESTROY)
                        {
                            internal_error("Not yet implemented: call the destructor", 0);
                        }
                    }
                    else
                    {
                        internal_error("running error", 0);
                    }

                    unpacked_arguments.append_with_separator(argument, ", ");
                    break;
                }
            case OutlineDataItem::SHARING_REDUCTION:
                {
                    // Pass the original reduced variable as if it were a shared
                    Source argument;
                    if (IS_C_LANGUAGE || IS_CXX_LANGUAGE)
                    {
                        argument << "*(args." << (*it)->get_field_name() << ")";
                    }
                    else
                    {
                        internal_error("running error", 0);
                    }
                    unpacked_arguments.append_with_separator(argument, ", ");
                    break;
                }
            default:
                {
                    internal_error("Unexpected data sharing kind", 0);
                }
        }
    }


    // Update the kernel configurations of every cuda function call of the current task
    Nodecl::NodeclBase task_code =
        (is_function_task) ? called_task.get_function_code() : output_statements;
    if (!task_code.is_null())
    {
        update_all_kernel_configurations(task_code);
    }

    // Add the user function to the intermediate file if It is a function task
    // (This action must be done always after the update of the kernel configurations
    // because the code of the user function may be changed if It contains one or more cuda function calls)
    if (is_function_task
            && !called_task.get_function_code().is_null())
    {
        _cuda_file_code.append(Nodecl::Utils::deep_copy(
                    called_task.get_function_code(),
                    called_task.get_scope()));
    }

    // Create the new unpacked function
    Source dummy_initial_statements, dummy_final_statements;
    TL::Symbol unpacked_function = new_function_symbol_unpacked(
            current_function,
            device_outline_name + "_unpacked",
            info,
            symbol_map,
            dummy_initial_statements,
            dummy_final_statements);

    Source ndrange_code;
    if (is_function_task
            && info._target_info.get_ndrange().size() > 0)
    {
        generate_ndrange_additional_code(called_task,
                unpacked_function,
                info._target_info.get_ndrange(),
                ndrange_code);
    }

    // The unpacked function must not be static and must have external linkage because
    // this function is called from the original source but It is defined in cudacc_filename.cu
    unpacked_function.get_internal_symbol()->entity_specs.is_static = 0;
    if (IS_C_LANGUAGE)
    {
        unpacked_function.get_internal_symbol()->entity_specs.linkage_spec = "\"C\"";
    }

    Nodecl::NodeclBase unpacked_function_code, unpacked_function_body;
    build_empty_body_for_function(unpacked_function,
            unpacked_function_code,
            unpacked_function_body);

    Source unpacked_source;
    unpacked_source
        << "{"
        << private_entities
        << ndrange_code
        << statement_placeholder(outline_placeholder)
        << "}"
        ;

    Nodecl::NodeclBase new_unpacked_body =
        unpacked_source.parse_statement(unpacked_function_body);
    unpacked_function_body.replace(new_unpacked_body);

    if (is_function_task
            && info._target_info.get_ndrange().size() > 0)
    {
        generate_ndrange_kernel_call(
                outline_placeholder.retrieve_context(),
                original_statements,
                output_statements);
    }

    // Add the unpacked function to the intermediate cuda file
    _cuda_file_code.append(unpacked_function_code);

    // Add a declaration of the unpacked function symbol in the original source
    if (IS_CXX_LANGUAGE)
    {
        Nodecl::NodeclBase nodecl_decl = Nodecl::CxxDecl::make(
                /* optative context */ nodecl_null(),
                unpacked_function,
                original_statements.get_filename(),
                original_statements.get_line());
        Nodecl::Utils::prepend_to_enclosing_top_level_location(original_statements, nodecl_decl);
    }

    // Create the outline function
    //The outline function has always only one parameter which name is 'args'
    ObjectList<std::string> structure_name;
    structure_name.append("args");

    //The type of this parameter is an struct (i. e. user defined type)
    ObjectList<TL::Type> structure_type;
    structure_type.append(TL::Type(
                get_user_defined_type(
                    info._arguments_struct.get_internal_symbol())).get_lvalue_reference_to());

    TL::Symbol outline_function = new_function_symbol(
            current_function,
            device_outline_name,
            TL::Type::get_void_type(),
            structure_name,
            structure_type);

    Nodecl::NodeclBase outline_function_code, outline_function_body;
    build_empty_body_for_function(outline_function,
            outline_function_code,
            outline_function_body);

    Source outline_src,
           instrument_before,
           instrument_after;

    outline_src
        << "{"
        <<      instrument_before
        <<      device_outline_name << "_unpacked(" << unpacked_arguments << ");"
        <<      instrument_after
        << "}"
        ;

    if (instrumentation_enabled())
    {
        get_instrumentation_code(
                info._called_task,
                outline_function,
                outline_function_body,
                info._task_label,
                original_statements.get_filename(),
                original_statements.get_line(),
                instrument_before,
                instrument_after);
    }

    Nodecl::NodeclBase new_outline_body = outline_src.parse_statement(outline_function_body);
    outline_function_body.replace(new_outline_body);
    Nodecl::Utils::prepend_to_enclosing_top_level_location(original_statements, outline_function_code);
}

DeviceCUDA::DeviceCUDA()
    : DeviceProvider(/* device_name */ std::string("cuda")) //, _cudaFilename(""), _cudaHeaderFilename("")
{
    set_phase_name("Nanox CUDA support");
    set_phase_description("This phase is used by Nanox phases to implement CUDA device support");
}


void DeviceCUDA::get_device_descriptor(DeviceDescriptorInfo& info,
        Source &ancillary_device_description,
        Source &device_descriptor,
        Source &fortran_dynamic_init UNUSED_PARAMETER)
{
    const std::string& device_outline_name = cuda_outline_name(info._outline_name);
    if (Nanos::Version::interface_is_at_least("master", 5012))
    {
        ancillary_device_description
            << comment("CUDA device descriptor")
            << "static nanos_smp_args_t "
            << device_outline_name << "_args = { (void(*)(void*))" << device_outline_name << "};"
            ;
    }
    else
    {
        internal_error("Unsupported Nanos version.", 0);
    }

    device_descriptor << "{ &nanos_gpu_factory, &" << device_outline_name << "_args }";
}

bool DeviceCUDA::remove_function_task_from_original_source() const
{
    return true;
}

void DeviceCUDA::add_included_cuda_files(FILE* file)
{
    ObjectList<IncludeLine> lines = CurrentFile::get_top_level_included_files();
    std::string cuda_file_ext(".cu\"");
    std::string cuda_header_ext(".cuh\"");

    for (ObjectList<IncludeLine>::iterator it = lines.begin(); it != lines.end(); it++)
    {
        std::string line = (*it).get_preprocessor_line();
        std::string extension = line.substr(line.find_last_of("."));

        if (extension == cuda_file_ext || extension == cuda_header_ext)
        {
            int output = fprintf(file, "%s\n", line.c_str());
            if (output < 0)
                internal_error("Error trying to write the intermediate cuda file\n", 0);
        }
    }
}

bool DeviceCUDA::allow_mandatory_creation()
{
    return true;
}

void DeviceCUDA::copy_stuff_to_device_file(const TL::ObjectList<Nodecl::NodeclBase>& stuff_to_be_copied)
{
    for (TL::ObjectList<Nodecl::NodeclBase>::const_iterator it = stuff_to_be_copied.begin();
            it != stuff_to_be_copied.end();
            ++it)
    {
        _cuda_file_code.append(Nodecl::Utils::deep_copy(*it, *it));
    }
}

void DeviceCUDA::phase_cleanup(DTO& data_flow)
{
    if (!_cuda_file_code.is_null())
    {
        std::string original_filename = TL::CompilationProcess::get_current_file().get_filename();
        std::string new_filename = "cudacc_" + original_filename.substr(0, original_filename.find("."))  + ".cu";

        FILE* ancillary_file = fopen(new_filename.c_str(), "w");
        if (ancillary_file == NULL)
        {
            running_error("%s: error: cannot open file '%s'. %s\n",
                    original_filename.c_str(),
                    new_filename.c_str(),
                    strerror(errno));
        }

        CXX_LANGUAGE()
        {
            // Add to the new intermediate file the *.cu, *.cuh included files.
            // It must be done only in C++ language because the C++ codegen do
            // not deduce the set of used symbols
            add_included_cuda_files(ancillary_file);
        }

        compilation_configuration_t* configuration = ::get_compilation_configuration("cuda");
        ERROR_CONDITION (configuration == NULL, "cuda profile is mandatory when using mnvcc/mnvcxx", 0);

        // Make sure phases are loaded (this is needed for codegen)
        load_compiler_phases(configuration);

        TL::CompilationProcess::add_file(new_filename, "cuda");

        //Remove the intermediate source file
        ::mark_file_for_cleanup(new_filename.c_str());

        Codegen::CudaGPU* phase = reinterpret_cast<Codegen::CudaGPU*>(configuration->codegen_phase);

        phase->codegen_top_level(_cuda_file_code, ancillary_file);

        fclose(ancillary_file);

        // Do not forget the clear the code for next files
        _cuda_file_code.get_internal_nodecl() = nodecl_null();
    }
}

void DeviceCUDA::pre_run(DTO& dto)
{
}

void DeviceCUDA::run(DTO& dto)
{
}

EXPORT_PHASE(TL::Nanox::DeviceCUDA);
