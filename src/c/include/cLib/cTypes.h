#ifndef CTYPES_H
#define CTYPES_H

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // Defines size_t

// --- Forward Declarations for Engine Types ---
// This prevents circular dependencies while allowing these 
// types to be used as pointers in other headers.
struct cString;
typedef struct cString string_t;

struct cName;
typedef struct cName Name_t;

// --- Standard Integer Types (mirrors cLib/ts/ctypes.d.ts) ---
// typedef int8_t   int8_t;
// typedef uint8_t  uint8_t;
// typedef int16_t  int16_t;
// typedef uint16_t uint16_t;
// typedef int32_t  int32_t;
// typedef uint32_t uint32_t;
// typedef int64_t  int64_t;
// typedef uint64_t uint64_t;

// --- Standard Floating Point (mirrors cLib/ts/ctypes.d.ts) ---
// typedef float    float;
// typedef double   double;
// typedef size_t   size_t;

// Empty Component Decorator (No-op for the C/C++ compiler)
#define Component(...) 

#endif
