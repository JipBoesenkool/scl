#ifndef C_ARRAY_H
#define C_ARRAY_H

#include <cLib/cTypes.h>

struct cSpan;
typedef struct cSpan Span_t;

struct cRange;
typedef struct cRange Range_t;

/**
 * Strategy for dynamic array growth.
 * Uses NSDMI for sensible default values.
 */
typedef struct cStrategy {
  uint32_t initialCapacity = 32;
  float growthFactor = 2.0f;
} Strategy_t;

/**
 * Core Dynamic Array structure.
 * pData points to the heap-allocated memory.
 */
typedef struct cArray {
  void* pData = NULL;
  uint32_t size = 0;
  uint32_t capacity = 0;
} Array_t;

// --- Lifecycle ---
bool da_create(Array_t* pArray, uint32_t itemSize);
bool da_init(Array_t* pArray, uint32_t itemSize, uint32_t capacity);
void da_destroy(Array_t* pArray);

// --- Iterators ---
void* da_begin(Array_t* pArray);
void* da_end(Array_t* pArray, uint32_t itemSize);

// --- Element Access ---
void* da_at(Array_t* pArray, uint32_t itemSize, uint32_t index);

// --- Capacity ---
bool da_isEmpty(Array_t* pArray);
bool da_reserve(Array_t* pArray, uint32_t itemSize, uint32_t newCapacity);
bool da_shrink_to_fit(Array_t* pArray, uint32_t itemSize);

// --- Modifiers ---
void* da_append(Array_t* pArray, uint32_t itemSize, const void* pItem);
bool  da_append_span(Array_t* pArray, uint32_t itemSize, Span_t span);
bool  da_append_range(Array_t* pArray, uint32_t itemSize, Range_t range);
void* da_insert(Array_t* pArray, uint32_t itemSize, uint32_t index, const void* pItem);
bool  da_insert_span(Array_t* pArray, uint32_t itemSize, uint32_t index, Span_t span);
bool  da_insert_range(Array_t* pArray, uint32_t itemSize, uint32_t index, Range_t range);
void  da_pop_back(Array_t* pArray);
void  da_pop_back_n(Array_t* pArray, uint32_t n);
void  da_erase(Array_t* pArray, uint32_t itemSize, uint32_t index);
void  da_clear(Array_t* pArray);
void  da_fill(Array_t* pArray, uint32_t itemSize, const void* pItem);

void  da_assign(Array_t* pArray, uint32_t itemSize, uint32_t count, const void* pItem);
bool  da_assign_span(Array_t* pArray, uint32_t itemSize, Span_t span);
bool  da_assign_range(Array_t* pArray, uint32_t itemSize, Range_t range);
bool  da_resize(Array_t* pArray, uint32_t itemSize, uint32_t newSize);
void  da_swap(Array_t* pLeft, Array_t* pRight);

// --- Extended Modifiers ---
void* da_append_ex(Array_t* pArray, uint32_t itemSize, const void* pItem, const Strategy_t* pStrategy);
bool  da_append_span_ex(Array_t* pArray, uint32_t itemSize, Span_t span, const Strategy_t* pStrategy);
bool  da_append_range_ex(Array_t* pArray, uint32_t itemSize, Range_t range, const Strategy_t* pStrategy);
void* da_insert_ex(Array_t* pArray, uint32_t itemSize, uint32_t index, const void* pItem, const Strategy_t* pStrategy);
bool  da_insert_span_ex(Array_t* pArray, uint32_t itemSize, uint32_t index, Span_t span, const Strategy_t* pStrategy);
bool  da_insert_range_ex(Array_t* pArray, uint32_t itemSize, uint32_t index, Range_t range, const Strategy_t* pStrategy);

#endif // C_ARRAY_H
