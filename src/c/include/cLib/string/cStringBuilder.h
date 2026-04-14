#ifndef C_STRING_BUILDER_H
#define C_STRING_BUILDER_H
#ifdef __cplusplus
extern "C" {
#endif

#include <cLib/string/cString.h>
#include <cLib/string/cFormat.h>
#include <cLib/memory/cBuffer.h>

#include <stdarg.h>
#include <stdint.h>
// Allocator_t is forward-declared via cString.h -> cString.h forward decl

typedef struct cStringBuilder
{
  TextBuffer_t buffer;
  size_t length   = 0;
  size_t capacity = 0;
} StringBuilder_t;

// --- Lifecycle ---
void sb_init(StringBuilder_t* sb, Buffer_t buffer);
void sb_clear(StringBuilder_t* sb);

// --- Appending ---
void sb_append_sv(StringBuilder_t* sb, StringView_t view);
void sb_append_char(StringBuilder_t* sb, char ch);
void sb_append_cstr(StringBuilder_t* sb, const char* str);
void sb_append_string(StringBuilder_t* sb, const String_t* s);
void sb_append_i32(StringBuilder_t* sb, int32_t value);
void sb_append_u32(StringBuilder_t* sb, uint32_t value);
void sb_append_f32(StringBuilder_t* sb, float value);
void sb_append_bool(StringBuilder_t* sb, bool value);
void sb_append_format(StringBuilder_t* sb, CLIB_PRINTF_CHECK const char* format, ...)
     CLIB_PRINTF_ATTR(2, 3);

// --- Conversion & Views ---
// Returns a view of the current string without null terminator
StringView_t sb_to_sv(const StringBuilder_t* sb);
// Deep copy of the builder result into an owned String_t (malloc fallback)
String_t sb_to_string(const StringBuilder_t* sb);
// Deep copy using pAlloc; NULL falls back to malloc (same as sb_to_string)
String_t sb_to_string_alloc(const StringBuilder_t* sb, Allocator_t* pAlloc);

#ifdef __cplusplus
} // extern "C"
#endif
#endif // C_STRING_BUILDER_H
