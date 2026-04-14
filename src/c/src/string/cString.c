#include <cLib/string/cString.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <cLib/memory/allocator/DefaultAllocator.h>

// --- Internal Helpers ---
// Internal constant for clean erasure logic
static const StringView_t EMPTY_SV = { .data = "", .length = 0 };

static inline bool string_is_on_heap(const String_t* s) {
  return s && s->capacity >= CSTRING_STACK_SIZE;
}

// Allocates and sets capacity/heap metadata, returns pointer to writable buffer.
// pAlloc == NULL -> malloc. pAlloc != NULL -> allocate through pAlloc.
static char* string_internal_alloc(String_t* out, size_t length, Allocator_t* pAlloc) {
  *out = String_t{ .stack = {0}, .capacity = 0 };
  if (length == 0) return out->stack;

  if (length < CSTRING_STACK_SIZE) {
    out->capacity = length;
    return out->stack;
  } else {
    char* data;
    if (pAlloc) {
      Handle_t h = pAlloc->Malloc(pAlloc->pContext, (uint32_t)(length + 1), &out->heap.pData);
      data = (char*)h.pData;
    } else {
      data = (char*)malloc(length + 1);
    }
    if (!data) return nullptr;
    out->heap = { .pData = data, .size = length };
    out->capacity = length + 1;
    return data;
  }
}

// Internal padding helper hidden from API
static String_t string_internal_pad(const String_t* in, char pad, size_t length, bool bAtStart) {
  StringView_t view = string_to_sv(in);
  if (length <= view.length) return string_copy(in);

  String_t result;
  char* buffer = string_internal_alloc(&result, length, nullptr);
  if (!buffer) return INVALID_STRING;

  size_t diff = length - view.length;
  if (bAtStart) {
    memset(buffer, pad, diff);
    memcpy(buffer + diff, view.data, view.length);
  } else {
    memcpy(buffer, view.data, view.length);
    memset(buffer + view.length, pad, diff);
  }
  buffer[length] = '\0';
  return result;
}

// --- Lifecycle ---

String_t string_create(const char* init) {
  if (!init) return INVALID_STRING;
  return string_create_from_sv(StringView_t{ .data = init, .length = strlen(init) });
}

String_t string_create_from_sv(StringView_t view) {
  String_t out;
  char* buffer = string_internal_alloc(&out, view.length, nullptr);
  if (buffer && view.length > 0) {
    memcpy(buffer, view.data, view.length);
    buffer[view.length] = '\0';
  }
  return out;
}

String_t string_copy(const String_t* source) {
  if (!source) return INVALID_STRING;
  return string_create_from_sv(string_to_sv(source));
}

void string_destroy(String_t* s) {
  if (string_is_on_heap(s)) {
    if (s->heap.pData) free(s->heap.pData);
  }
  *s = INVALID_STRING;
}

// --- Inspection ---
size_t string_length(const String_t* s) {
  if (!s) return 0;
  return string_is_on_heap(s) ? s->heap.size : s->capacity;
}

bool string_is_empty(const String_t* s) {
  return string_length(s) == 0;
}

char string_at(const String_t* pStr, size_t index) {
  if (!pStr || index >= string_length(pStr))
  {
    return '\0';
  }
  // If capacity is 0-23, it's SSO. Otherwise, it's Heap.
  if (pStr->capacity <= (CSTRING_STACK_SIZE - 1) )
  {
    return pStr->stack[index];
  } else {
    return pStr->heap.pData[index];
  }
}

int string_compare(const String_t* a, const String_t* b) {
  size_t lenA = string_length(a);
  size_t lenB = string_length(b);
  if (lenA != lenB) return (lenA > lenB) ? 1 : -1;
  return memcmp(string_to_c_str(a), string_to_c_str(b), lenA);
}

// --- Conversion ---
const char* string_to_c_str(const String_t* s) {
  if (!s) return "";
  return string_is_on_heap(s) ? (s->heap.pData ? s->heap.pData : "") : s->stack;
}

StringView_t string_to_sv(const String_t* s) {
  if (string_is_empty(s)) {
    return INVALID_SV;
  }
  
  if (string_is_on_heap(s)) {
    return StringView_t{ .data = s->heap.pData, .length = s->heap.size };
  }
  
  return StringView_t{ .data = s->stack, .length = s->capacity };
}

// --- Transformations ---
String_t string_concat(const StringView_t a, const StringView_t b) {
  size_t total = a.length + b.length;
  String_t out;
  char* buffer = string_internal_alloc(&out, total, nullptr);
  if (buffer && total > 0) {
    if (a.length > 0) memcpy(buffer, a.data, a.length);
    if (b.length > 0) memcpy(buffer + a.length, b.data, b.length);
    buffer[total] = '\0';
  }
  return out;
}

String_t string_erase(const String_t* pSrc, size_t start, size_t count) {
  size_t len = string_length(pSrc);
  if (start >= len || count == 0) {
    return string_copy(pSrc);
  }

  if (start + count > len) {
    count = len - start;
  }

  StringView_t self = string_to_sv(pSrc);
  StringView_t v1 = { self.data, start };
  StringView_t v2 = { self.data + start + count, len - (start + count) };

  // This is safe because string_concat handles SSO vs Heap transitions
  return string_concat(v1, v2);
}

String_t string_erase_sv(const String_t* src, const StringView_t view) {
  if (!src || string_is_empty(src) || view.length == 0) {
    return string_copy(src);
  }

  // Routing through replace ensures a single, well-tested path for 
  // multi-occurrence string manipulation.
  return string_replace(src, view, EMPTY_SV);
}

String_t string_substring(const String_t* in, size_t start, size_t end) {
  StringView_t sub = sv_slice(string_to_sv(in), start, end);
  return sv_is_empty(sub) ? INVALID_STRING : string_create_from_sv(sub);
}

String_t string_to_upper(const String_t* in) {
  StringView_t view = string_to_sv(in);
  if (view.length == 0) return INVALID_STRING;
  String_t out;
  char* buffer = string_internal_alloc(&out, view.length, nullptr);
  if (buffer) {
    for (size_t i = 0; i < view.length; ++i) buffer[i] = (char)toupper((unsigned char)view.data[i]);
    buffer[view.length] = '\0';
  }
  return out;
}

String_t string_to_lower(const String_t* in) {
  StringView_t view = string_to_sv(in);
  if (view.length == 0) return INVALID_STRING;
  String_t out;
  char* buffer = string_internal_alloc(&out, view.length, nullptr);
  if (buffer) {
    for (size_t i = 0; i < view.length; ++i) buffer[i] = (char)tolower((unsigned char)view.data[i]);
    buffer[view.length] = '\0';
  }
  return out;
}

String_t string_replace(const String_t* in, const StringView_t oldView, const StringView_t newView) {
  if (string_is_empty(in) || sv_is_empty(oldView)) return string_copy(in);
  
  StringView_t source = string_to_sv(in);
  size_t count = 0;
  size_t pos = 0;
  StringView_t searchArea = source;

  // Pass 1: Count occurrences using sv_find (safe for non-null-terminated views)
  while ((pos = sv_find(searchArea, oldView)) != INVALID_INDEX) {
    count++;
    searchArea.data += pos + oldView.length;
    searchArea.length -= pos + oldView.length;
  }

  if (count == 0) return string_copy(in);

  // Pass 2: Single Allocation
  size_t finalLen = source.length + (count * (newView.length - oldView.length));
  String_t out;
  char* buffer = string_internal_alloc(&out, finalLen, nullptr);
  if (!buffer) return INVALID_STRING;

  char* dest = buffer;
  const char* srcPtr = source.data;
  searchArea = source; // Reset search area

  while ((pos = sv_find(searchArea, oldView)) != INVALID_INDEX) {
    // Copy the part before the match
    memcpy(dest, searchArea.data, pos);
    dest += pos;
    // Copy the replacement
    memcpy(dest, newView.data, newView.length);
    dest += newView.length;
    // Advance search area
    searchArea.data += pos + oldView.length;
    searchArea.length -= pos + oldView.length;
  }
  
  // Copy remaining tail
  if (searchArea.length > 0) {
    memcpy(dest, searchArea.data, searchArea.length);
    dest += searchArea.length;
  }
  
  *dest = '\0';
  return out;
}

String_t string_replace_char(const String_t* in, char oldChar, char newChar) {
  StringView_t view = string_to_sv(in);
  String_t out;
  char* buffer = string_internal_alloc(&out, view.length, nullptr);
  if (buffer) {
    for (size_t i = 0; i < view.length; ++i) {
      buffer[i] = (view.data[i] == oldChar) ? newChar : view.data[i];
    }
    buffer[view.length] = '\0';
  }
  return out;
}

String_t string_repeat(const String_t* in, uint32_t count) {
  StringView_t view = string_to_sv(in);
  size_t total = view.length * count;
  if (total == 0) return INVALID_STRING;
  String_t out;
  char* buffer = string_internal_alloc(&out, total, nullptr);
  if (buffer) {
    for (uint32_t i = 0; i < count; ++i) {
      memcpy(buffer + (i * view.length), view.data, view.length);
    }
    buffer[total] = '\0';
  }
  return out;
}

// --- Padding ---
String_t string_lpad(const String_t* in, char pad, size_t length) {
  return string_internal_pad(in, pad, length, true);
}

String_t string_rpad(const String_t* in, char pad, size_t length) {
  return string_internal_pad(in, pad, length, false);
}

// --- Allocator-aware Lifecycle ---
String_t string_create_alloc(const char* init, Allocator_t* pAlloc) {
  if (!init) return INVALID_STRING;
  return string_create_from_sv_alloc(StringView_t{ .data = init, .length = strlen(init) }, pAlloc);
}

String_t string_create_from_sv_alloc(StringView_t view, Allocator_t* pAlloc) {
  String_t out;
  char* buffer = string_internal_alloc(&out, view.length, pAlloc);
  if (buffer && view.length > 0) {
    memcpy(buffer, view.data, view.length);
    buffer[view.length] = '\0';
  }
  return out;
}

String_t string_copy_alloc(const String_t* source, Allocator_t* pAlloc) {
  if (!source) return INVALID_STRING;
  return string_create_from_sv_alloc(string_to_sv(source), pAlloc);
}

void string_destroy_alloc(String_t* s, Allocator_t* pAlloc) {
  if (!string_is_on_heap(s)) {
    // SSO string — nothing was allocated through pAlloc, just reset.
    *s = INVALID_STRING;
    return;
  }
  if (pAlloc && s->heap.pData) {
    Handle_t h = { 0, 0, s->heap.pData };
    pAlloc->Free(pAlloc->pContext, &h);
  } else if (s->heap.pData) {
    free(s->heap.pData);
  }
  *s = INVALID_STRING;
}
