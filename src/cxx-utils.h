#ifndef CXX_UTILS_H
#define CXX_UTILS_H

#include <stdlib.h>

// Some useful macros
#ifndef __GNUC__
    #define __attribute__(x)
#endif

#ifdef __GNUC__
  #if __GNUC__ >= 3 
     #define NORETURN __attribute__((noreturn))

     #if __GNUC_MINOR__ >= 4
         #define WARN_UNUSED __attribute__((warn_unused_result))
     #else
         #define WARN_UNUSED
     #endif
  #elif __GNUC__ == 2
     #error "This code will not compile with GCC 2"
  #endif
#endif

void running_error(char* message, ...) NORETURN;

#define internal_error(message, ...) \
   { debug_message(message, "Internal compiler error. Please report bug:\n", __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__ ); exit(EXIT_FAILURE);  }

void debug_message(const char* message, const char* kind, const char* source_file, int line, const char* function_name, ...);

#define DEBUG_MESSAGE(message, ...) \
   { if (compilation_options.debugging) debug_message(message, "Debug: ", __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__); }

#define WARNING_MESSAGE(message, ...) \
   { debug_message(message, "Warning: ", __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__); }

#define ASSERT_MESSAGE(cond, message, ...) \
   { if (!(cond)) \
       { debug_message(message, "Assert failed (" #cond ")\n\t", __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__); } \
   }

#define HASH_SIZE 23
int prime_hash(char* key, int hash_size);

// Routine to ease adding pointers to a pointer list
//   list is a T**
//   size is an int
//   elem is a T*
#define P_LIST_ADD(list, size, elem)  \
do { \
	(size)++; \
	(list) = realloc((list), sizeof(*(list))*(size)); \
	(list)[((size)-1)] = (elem); \
} while(0)

#endif // CXX_UTILS_H
