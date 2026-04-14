#ifndef C_FORMAT_H
#define C_FORMAT_H

/**
 * --- String_t Macro ---
 * Extracts the raw char* from a String_t for use in variadic functions.
 */
#define STR_VAL(s) string_to_c_str(&(s))

/**
 * --- Format Specifiers ---
 * Use these for consistency and readability across the codebase.
 */
#define FMT_I32  "%d"
#define FMT_U32  "%u"
#define FMT_I64  "%lld"
#define FMT_U64  "%llu"
#define FMT_F32  "%.3f"
#define FMT_HEX  "0x%08X"

#define FMT_STR  "%s"  // To be used with STR_VAL() or const char*
#define FMT_PTR  "%p"
#define FMT_SV   "%.*s"

/**
 * --- Compiler Safety Macros ---
 * Cross-platform annotations to ensure the compiler catches format mismatches.
 */
#if defined(_MSC_VER)
    #include <sal.h>
    #define CLIB_PRINTF_CHECK _Printf_format_string_
    #define CLIB_PRINTF_ATTR(fmt, args) 
#elif defined(__GNUC__) || defined(__clang__)
    #define CLIB_PRINTF_CHECK
    #define CLIB_PRINTF_ATTR(fmt, args) __attribute__ ((format (printf, fmt, args)))
#else
    #define CLIB_PRINTF_CHECK
    #define CLIB_PRINTF_ATTR(fmt, args)
#endif

#endif // C_FORMAT_H
