#ifndef C_STRING_H
#define C_STRING_H
#ifdef __cplusplus
extern "C" {
#endif

#include <cLib/cTypes.h>
#include <cLib/string/cStringView.h>
#include <cLib/memory/cBuffer.h>

// Forward declaration — avoids circular include chain:
// cString.h -> Allocator.h -> cName.h -> cString.h
typedef struct cAllocator Allocator_t;

#define CSTRING_STACK_SIZE 24
#define INVALID_CSTR "INVALID"

typedef struct cString {
  union {
    char          stack[CSTRING_STACK_SIZE] = {0}; 
    TextBuffer_t  heap;
  };
  size_t capacity = 0; 
} String_t;

// Both represent a zero-length, stack-allocated string
#define INVALID_STRING (String_t{ .stack = {0}, .capacity = 0 })

static_assert(sizeof(TextBuffer_t) <= CSTRING_STACK_SIZE, "SSO Error: TextBuffer must fit within CSTRING_STACK_SIZE");

// --- Lifecycle ---
String_t      string_create(const char* init);
String_t      string_create_from_sv(StringView_t view);
String_t      string_copy(const String_t* source);
void          string_destroy(String_t* s);

// --- Allocator-aware Lifecycle ---
// pAlloc == NULL falls back to malloc (identical to the non-_alloc variants).
// When pAlloc != NULL the string's heap block is allocated through pAlloc so
// that the owning zone can defragment it later. SSO strings never touch pAlloc.
String_t      string_create_alloc(const char* init, Allocator_t* pAlloc);
String_t      string_create_from_sv_alloc(StringView_t view, Allocator_t* pAlloc);
String_t      string_copy_alloc(const String_t* source, Allocator_t* pAlloc);
void          string_destroy_alloc(String_t* s, Allocator_t* pAlloc);

// --- Conversion ---
const char* string_to_c_str(const String_t* s);
StringView_t  string_to_sv(const String_t* s);

// --- Inspection ---
char          string_at(const String_t* src, size_t index);
int           string_compare(const String_t* a, const String_t* b);
size_t        string_length(const String_t* s);
bool          string_is_empty(const String_t* s);

// --- Transformation ---
String_t      string_concat(const StringView_t a, const StringView_t b);
String_t      string_erase(const String_t* src, size_t start, size_t count);
String_t      string_erase_sv(const String_t* src, const StringView_t view);
String_t      string_substring(const String_t* in, size_t start, size_t end);
String_t      string_to_upper(const String_t* in);
String_t      string_to_lower(const String_t* in);
String_t      string_repeat(const String_t* in, uint32_t count);
String_t      string_replace(const String_t* in, const StringView_t oldView, const StringView_t newView);
String_t      string_replace_char(const String_t* in, char oldChar, char newChar);
String_t      string_lpad(const String_t* in, char pad, size_t length);
String_t      string_rpad(const String_t* in, char pad, size_t length);

#ifdef __cplusplus
} // extern "C"
#endif
#endif // C_STRING_H
