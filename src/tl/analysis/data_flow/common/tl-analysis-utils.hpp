/*--------------------------------------------------------------------
 ( C) Copyright 2006-2012 Barcelona Supercomputing Center             **
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

#ifndef TL_ANALYSIS_UTILS_HPP
#define TL_ANALYSIS_UTILS_HPP

#include <sstream>

#include "tl-extended-symbol.hpp"
#include "tl-nodecl-visitor.hpp"

namespace TL {
namespace Analysis {
namespace Utils {    
    
    // *************************************************************** //
    // ************ Common methods with analysis purposes ************ //
    
    //! Returns a hashed string depending on \ast
    std::string generate_hashed_name( Nodecl::NodeclBase ast );
    
    //! Returns the Nodecl containing the main in \ast, if it exists. 
    //! Returns a null Nodecl otherwise
    Nodecl::NodeclBase find_main_function( Nodecl::NodeclBase ast );
    
    // ********** End common methods with analysis purposes ********** //
    // *************************************************************** //
    

    
    // *************************************************************** //
    // ************************** Visitors *************************** //
    
    //!Visitor to get the l-values contained in a given nodecl that are modified
    class LIBTL_CLASS AssignedExtSymVisitor : public Nodecl::ExhaustiveVisitor<void>
    {
    private:
        
        // ******* Class attributes ******* //
        
        //! List of extended symbols found during the traversal
        ObjectList<ExtendedSymbol> _assigned_ext_syms;   
        
        //! Temporary value used to know whether we are traversing the left-hand side of an assignment
        bool _is_lhs;
        
        
        // ******* Private methods ******** //
        
        void visit_assignment(Nodecl::NodeclBase ass_lhs, Nodecl::NodeclBase ass_rhs);
        
        void visit_xx_crements(Nodecl::NodeclBase n);
        
    public:
        //! Constructor
        AssignedExtSymVisitor( );
        
        //! Getters and setters
        ObjectList<ExtendedSymbol> get_assigned_ext_syms( );
        
        //! Visiting methods
        Ret visit( const Nodecl::AddAssignment& n );
        Ret visit( const Nodecl::ArithmeticShrAssignment& n );
        Ret visit( const Nodecl::ArraySubscript& n );
        Ret visit( const Nodecl::Assignment& n );
        Ret visit( const Nodecl::BitwiseAndAssignment& n );
        Ret visit( const Nodecl::BitwiseOrAssignment& n );
        Ret visit( const Nodecl::BitwiseShlAssignment& n );
        Ret visit( const Nodecl::BitwiseShrAssignment& n );
        Ret visit( const Nodecl::BitwiseXorAssignment& n );
        Ret visit( const Nodecl::ClassMemberAccess& n );
        Ret visit( const Nodecl::DivAssignment& n );
        Ret visit( const Nodecl::MinusAssignment& n );
        Ret visit( const Nodecl::ModAssignment& n );
        Ret visit( const Nodecl::MulAssignment& n );
        Ret visit( const Nodecl::Postdecrement& n );
        Ret visit( const Nodecl::Postincrement& n );
        Ret visit( const Nodecl::Predecrement& n );
        Ret visit( const Nodecl::Preincrement& n );
        Ret visit( const Nodecl::Symbol& n );
    };
    
    //! Visitor to get the main function of a program, if it exists
    class LIBTL_CLASS MainFunctionVisitor : public Nodecl::ExhaustiveVisitor<void>
    {
    private:
        // ******* Class attributes ******* //
        Nodecl::NodeclBase _main;
        
    public:
        // ********* Constructors ********* //
        //! Constructor
        MainFunctionVisitor( );
        
        // ****** Getters and setters ****** //
        Nodecl::NodeclBase get_main( );
        
        // ******** Visiting methods ******* //
        //! Visiting methods
        /*!We re-implement all TopLevel nodecl to avoid continue visiting in case
         * the main function has already been found.
         * TopLevel
         *      FunctionCode
         *      ObjectInit
         *      CxxDecl             : CxxDecl, CxxDef, CxxExplicitInstantiation
         *                            CxxExternExplicitInstantiation, CxxUsingNamespace, CxxUsingDecl
         *      PragmaDirective     : PragmaCustomDirective, PragmaCustomDeclaration
         *      Compatibility       : UnknownPragma, SourceComment, PreprocessorLine, Verbatim
         *                            AsmDefinition, GccAsmDefinition, GccAsmSpec, UpcSyncStatement
         *                            GccBuiltinVaArg, GxxTrait, Text
         */
        Ret visit( const Nodecl::AsmDefinition& n );
        Ret visit( const Nodecl::GccAsmDefinition& n );
        Ret visit( const Nodecl::GccAsmSpec& n );
        Ret visit( const Nodecl::GccBuiltinVaArg& n );
        Ret visit( const Nodecl::CxxDecl& n );
        Ret visit( const Nodecl::CxxDef& n );
        Ret visit( const Nodecl::CxxExplicitInstantiation& n );
        Ret visit( const Nodecl::CxxExternExplicitInstantiation& n );
        Ret visit( const Nodecl::CxxUsingNamespace& n );
        Ret visit( const Nodecl::CxxUsingDecl& n );
        Ret visit( const Nodecl::FunctionCode& n );
        Ret visit( const Nodecl::GxxTrait& n );
        Ret visit( const Nodecl::ObjectInit& n );
        Ret visit( const Nodecl::PragmaCustomDeclaration& n );
        Ret visit( const Nodecl::PragmaCustomDirective& n );
        Ret visit( const Nodecl::PreprocessorLine& n );
        Ret visit( const Nodecl::SourceComment& n );
        Ret visit( const Nodecl::Text& n );
        Ret visit( const Nodecl::UnknownPragma& n );
        Ret visit( const Nodecl::UpcSyncStatement& n );
        Ret visit( const Nodecl::Verbatim& n );
    };
    
    // ************************* End visitors ************************ //
    // *************************************************************** //
}
}
}

#endif          // TL_ANALYSIS_UTILS_HPP