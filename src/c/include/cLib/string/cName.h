#ifndef C_NAME_H
#define C_NAME_H
#ifdef __cplusplus
extern "C" {
#endif

#include <cLib/cTypes.h>
#include <cLib/string/cString.h>
#include <cLib/string/cStringView.h>

#define INVALID_HASH 0x00000000u
// The struct name must match the forward declaration in cTypes.h
typedef struct cName {
  const char* name = INVALID_CSTR; // 8 bytes
  uint32_t    hash = INVALID_HASH; // 4 bytes
  uint32_t    padding;             // 4 bytes
} Name_t; // 16 bytes

// Standard Null Object
static const Name_t INVALID_NAME = { INVALID_CSTR, INVALID_HASH };

// --- Hashing Constants ---
#define FNV_OFFSET_BASIS 2166136261u
#define FNV_PRIME 16777619u

// --- Core Hashing Logic ---
// Kept inline so the NAME macro can be evaluated at compile-time.
static inline uint32_t fnv_hash(const char* str, size_t len) {
  if (len == 0 || str == nullptr) return INVALID_HASH;

  uint32_t hash = FNV_OFFSET_BASIS;
  for (size_t i = 0; i < len; ++i) {
    hash ^= (uint8_t)str[i];
    hash *= FNV_PRIME;
  }
  return hash;
}

// We keep this inline so the NAME macro can be evaluated at compile-time.
static inline uint32_t fnv_hash_string_view(StringView_t view) {
  if (view.length == 0) return INVALID_HASH;
  
  uint32_t hash = FNV_OFFSET_BASIS;
  for (size_t i = 0; i < view.length; ++i) {
    hash ^= (uint8_t)view.data[i];
    hash *= FNV_PRIME;
  }
  return hash;
}

// Runtime: wraps a const char* (caller guarantees the pointer outlives the Name_t).
// Use for zone/stack names that are always string literals.
static inline Name_t name_from_cstr(const char* str) {
  if (str == nullptr || str[0] == '\0') return INVALID_NAME;
  size_t len = 0;
  while (str[len]) ++len; // avoid strlen dependency in C
  return Name_t{ str, fnv_hash(str, len) };
}


// Enforces literal usage via "" lit concatenation.
#define NAME(lit) (Name_t{ "" lit, fnv_hash_string_view({ "" lit, sizeof(lit) - 1 }) })

// --- Comparison ---
bool name_equals(Name_t a, Name_t b);

#ifdef __cplusplus
} // extern "C"
#endif
#endif // C_NAME_H
