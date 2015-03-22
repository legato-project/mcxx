/*--------------------------------------------------------------------
  (C) Copyright 2006-2014 Barcelona Supercomputing Center
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




#ifndef CXX_LEXER_H
#define CXX_LEXER_H

#include <stdio.h>
#include "libmcxx-common.h"
#include "cxx-driver-decls.h"
#include "cxx-macros.h"

MCXX_BEGIN_DECLS

typedef 
struct token_atrib_tag 
{
    const char* token_text;
} token_atrib_t;

typedef struct parser_location_tag
{
    const char* first_filename;
    int first_line;
    int first_column;
} parser_location_t;

# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_filename = YYRHSLOC (Rhs, 1).first_filename;  \
          (Current).first_line     = YYRHSLOC (Rhs, 1).first_line;      \
          (Current).first_column   = YYRHSLOC (Rhs, 1).first_column;    \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_filename   =                                  \
            YYRHSLOC (Rhs, 0).first_filename;                           \
          (Current).first_line =                                        \
            YYRHSLOC (Rhs, 0).first_line;                               \
          (Current).first_column =                                      \
            YYRHSLOC (Rhs, 0).first_column;                             \
        }                                                               \
    while (0)

LIBMCXX_EXTERN void update_parser_location(const char* current_text, parser_location_t* loc);

struct scan_file_descriptor 
{
    // This is the (physical) filename being scanned
    const char* filename;

    // This is the logical filename that we are scanning.
    // current_filename != filename only in Fortran fixed-form because we scan
    // the output of prescanner
    const char* current_filename;

    union {
        // file descriptor + flex buffer
        struct {
            FILE* file_descriptor;
            struct yy_buffer_state* scanning_buffer;
        };

        // memory buffer/mmap
        struct {
            const char *current_pos; // position in the buffer

            const char *buffer; // scanned buffer
            size_t buffer_size; // number of characters in buffer relevant for scanning

            int fd; // if fd >= 0 this is a mmap
        };
    };

    // Line of current token
    unsigned int line_number;
    // Column where the current token starts
    unsigned column_number;
    // Fortran: After a joined line we have to move to this line if new_line > 0 
    unsigned int new_line; 
    // Fortran: Number of joined lines so far
    unsigned int joined_lines;
};

#define YYLTYPE parser_location_t

LIBMCXX_EXTERN struct scan_file_descriptor scanning_now;

LIBMCXX_EXTERN int mcxx_open_file_for_scanning(const char* scanned_filename, const char* input_filename);
LIBMCXX_EXTERN int mc99_open_file_for_scanning(const char* scanned_filename, const char* input_filename);

LIBMCXX_EXTERN int mcxx_prepare_string_for_scanning(const char* str);
LIBMCXX_EXTERN int mc99_prepare_string_for_scanning(const char* str);

LIBMCXX_EXTERN void register_new_directive(
        compilation_configuration_t* configuration,
        const char* prefix, const char* directive, char is_construct, 
        /* This flag is only meaningful in Fortran */
        char bound_to_single_stmt);

LIBMCXX_EXTERN pragma_directive_kind_t lookup_pragma_directive(const char* prefix, const char* directive);

LIBMCXX_EXTERN int mc99_flex_debug;
LIBMCXX_EXTERN int mcxx_flex_debug;

LIBMCXX_EXTERN int mcxxdebug;
LIBMCXX_EXTERN int mc99debug;

LIBMCXX_EXTERN void close_scanned_file(void); 

MCXX_END_DECLS

#endif // CXX_LEXER_H
