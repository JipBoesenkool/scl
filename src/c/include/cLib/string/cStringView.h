#ifndef C_STRING_VIEW_H
#define C_STRING_VIEW_H
#ifdef __cplusplus
extern "C" {
#endif

#include <cLib/cTypes.h>

//TODO: settle on the interface, currently a mix of c++ and javascript
//TODO: return array of views
#define INVALID_INDEX ((size_t)-1)
#define INVALID_SV StringView_t{ nullptr, 0 }

/*
 * StringView_t — zero-copy, non-owning view into a string buffer.
 *
 * SAFE POINT CONTRACT: A StringView is only valid as long as the underlying
 * buffer is not moved or freed. In particular, if the owning String_t was
 * allocated through a defrag-capable allocator, any defrag/compact operation
 * invalidates all outstanding StringViews into that buffer.
 *
 * Do not store StringViews across allocator operations. Obtain a fresh view
 * after each potential defrag event.
 */
typedef struct cStringView {
  const char* data   = nullptr;
  size_t      length = 0;
} StringView_t;

#define SV(lit) ( StringView_t{ "" lit, sizeof(lit) - 1 } )

// --- Conversion ---
StringView_t create_sv(const char* str);

// --- Inspection ---
int     sv_compare(StringView_t a, StringView_t b);
bool    sv_is_empty(StringView_t view);
bool    sv_starts_with(StringView_t view, StringView_t prefix);
bool    sv_ends_with(StringView_t view, StringView_t suffix);
bool    sv_contains(StringView_t view, StringView_t search);

size_t  sv_find(StringView_t view, StringView_t search);
size_t  sv_rfind(StringView_t view, StringView_t search);
size_t  sv_find_first_of(StringView_t view, StringView_t search);
size_t  sv_find_last_of(StringView_t view, StringView_t search);
//size_t  sv_find_all(StringView_t view, StringView_t search);
//size_t  sv_rfind_all(StringView_t view, StringView_t search);

// --- Slicing & Trimming (Zero Allocation) ---;
StringView_t sv_trim(StringView_t view);
StringView_t sv_ltrim(StringView_t view);
StringView_t sv_rtrim(StringView_t view);
StringView_t sv_slice(StringView_t view, size_t start, size_t end);
//StringView_t sv_split(StringView_t view, char delimiter);

#ifdef __cplusplus
} // extern "C"
#endif
#endif // C_STRING_VIEW_H
