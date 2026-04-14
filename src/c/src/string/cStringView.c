#include <cLib/string/cStringView.h>
#include <string.h>
#include <ctype.h>

// --- Conversion ---
StringView_t create_sv(const char* str) {
  if (!str) return INVALID_SV;
  return StringView_t{ .data = str, .length = strlen(str) };
}

// --- Inspection ---
int sv_compare(StringView_t a, StringView_t b) {
  // Guard: treat null data as empty regardless of length field
  if (!a.data && !b.data) return 0;
  if (!a.data) return -1;
  if (!b.data) return  1;

  size_t minLen = (a.length < b.length) ? a.length : b.length;

  // memcmp returns <0, 0, or >0 based on the first differing byte
  int res = memcmp(a.data, b.data, minLen);
  
  // If the common prefix is identical, the shorter string is "less than"
  if (res == 0) {
    if (a.length < b.length) return -1;
    if (a.length > b.length) return 1;
    return 0;
  }
  
  return res;
}

bool sv_is_empty(StringView_t view) {
  return view.data == nullptr || view.length == 0;
}

bool sv_starts_with(StringView_t view, StringView_t prefix) {
  if (prefix.length > view.length) return false;
  return memcmp(view.data, prefix.data, prefix.length) == 0;
}

bool sv_ends_with(StringView_t view, StringView_t suffix) {
  if (suffix.length > view.length) return false;
  const char* start = view.data + (view.length - suffix.length);
  return memcmp(start, suffix.data, suffix.length) == 0;
}

bool sv_contains(StringView_t view, StringView_t search) {
  return sv_find(view, search) != INVALID_INDEX;
}

// --- Search ---
size_t sv_find(StringView_t view, StringView_t search) {
  if (search.length == 0) return 0;
  if (search.length > view.length) return INVALID_INDEX;
  
  size_t maxLimit = view.length - search.length;
  for (size_t i = 0; i <= maxLimit; ++i) {
    if (memcmp(view.data + i, search.data, search.length) == 0) return i;
  }
  return INVALID_INDEX;
}

size_t sv_rfind(StringView_t view, StringView_t search) {
  if (search.length == 0) return view.length;
  if (search.length > view.length) return INVALID_INDEX;
  
  for (size_t i = view.length - search.length; ; --i) {
    if (memcmp(view.data + i, search.data, search.length) == 0) return i;
    if (i == 0) break;
  }
  return INVALID_INDEX;
}

size_t sv_find_first_of(StringView_t view, StringView_t search) {
  if (sv_is_empty(view) || sv_is_empty(search)) return INVALID_INDEX;

  for (size_t i = 0; i < view.length; ++i) {
    for (size_t j = 0; j < search.length; ++j) {
      if (view.data[i] == search.data[j]) return i;
    }
  }
  return INVALID_INDEX;
}

size_t sv_find_last_of(StringView_t view, StringView_t search) {
  if (sv_is_empty(view) || sv_is_empty(search)) return INVALID_INDEX;

  for (size_t i = view.length - 1; ; --i) {
    for (size_t j = 0; j < search.length; ++j) {
      if (view.data[i] == search.data[j]) return i;
    }
    if (i == 0) break;
  }
  return INVALID_INDEX;
}

// --- Slicing & Trimming ---
StringView_t sv_slice(StringView_t view, size_t start, size_t end) {
  if (sv_is_empty(view) || start >= view.length || end < start) {
    return INVALID_SV;
  }

  size_t actualEnd = (end > view.length) ? view.length : end;
  size_t newLen = actualEnd - start;

  if (newLen == 0) return INVALID_SV;

  return StringView_t{ 
    .data = view.data + start, 
    .length = newLen 
  };
}

StringView_t sv_ltrim(StringView_t view) {
  if (sv_is_empty(view)) return INVALID_SV;

  size_t start = 0;
  while (start < view.length && isspace((unsigned char)view.data[start])) {
    start++;
  }

  if (start == view.length) return INVALID_SV;

  return StringView_t{ 
    .data = view.data + start, 
    .length = view.length - start 
  };
}

StringView_t sv_rtrim(StringView_t view) {
  if (sv_is_empty(view)) return INVALID_SV;

  size_t end = view.length;
  while (end > 0 && isspace((unsigned char)view.data[end - 1])) {
    end--;
  }

  if (end == 0) return INVALID_SV;

  return StringView_t{ 
    .data = view.data, 
    .length = end 
  };
}

StringView_t sv_trim(StringView_t view) {
  // Pass through rtrim first, then ltrim the result
  return sv_ltrim(sv_rtrim(view));
}
