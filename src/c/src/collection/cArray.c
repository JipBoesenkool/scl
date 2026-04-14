#include <cLib/collection/cArray.h>
#include <cLib/collection/cSpan.h>
#include <cLib/collection/cRange.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/**
 * Default growth strategy used when no explicit strategy is provided.
 */
static const Strategy_t kDEFAULT_STRATEGY = {
  .initialCapacity = 32,
  .growthFactor = 2.0f
};

// --- Internal Helpers ---
/**
 * Checks if the array needs to grow to accommodate 'required' elements.
 * Uses the provided strategy for growth calculations.
 */
static bool da_maybe_grow_ex(Array_t* pArray, uint32_t itemSize, uint32_t required, const Strategy_t* pStrategy) {
  if (required <= pArray->capacity) return true;

  // Validation: Ensure the growth factor actually increases the initial capacity.
  // This catches cases like: initialCapacity = 1, growthFactor = 1.1f -> (1 * 1.1 = 1).
  assert( (uint32_t)(pStrategy->initialCapacity * pStrategy->growthFactor) > pStrategy->initialCapacity 
         && "Invalid Strategy: growthFactor is too low for the given initialCapacity.");

  // 1. Pick starting point
  uint32_t newCap = pArray->capacity;
  if (newCap < pStrategy->initialCapacity) {
    newCap = pStrategy->initialCapacity;
  }

  // 2. Geometric Growth
  while (newCap < required) {
    newCap = (uint32_t)(newCap * pStrategy->growthFactor);
  }

  return da_reserve(pArray, itemSize, newCap);
}

/**
 * Calculates the number of items between two pointers based on itemSize.
 */
static uint32_t da_calculate_range_count(Range_t range, uint32_t itemSize) {
  if (!range.pBegin || !range.pSentinel) return 0;
  return (uint32_t)(((unsigned char*)range.pSentinel - (unsigned char*)range.pBegin) / itemSize);
}

// --- Lifecycle ---
bool da_create(Array_t* pArray, uint32_t itemSize) {
  assert(pArray);
  return da_reserve(pArray, itemSize, kDEFAULT_STRATEGY.initialCapacity);
}


bool da_init(Array_t* pArray, uint32_t itemSize, uint32_t capacity) {
  assert(pArray);
  assert(!pArray->pData);
  return da_reserve(pArray, itemSize, capacity);
}

void da_destroy(Array_t* pArray) {
  if (pArray && pArray->pData) {
    free(pArray->pData);
  }
  pArray->pData = NULL;
  pArray->size = 0;
  pArray->capacity = 0;
}

// --- Iterators ---

void* da_begin(Array_t* pArray) { 
  return pArray->pData; 
}

void* da_end(Array_t* pArray, uint32_t itemSize) {
  return (unsigned char*)pArray->pData + (pArray->size * itemSize);
}

// --- Element Access ---
void* da_at(Array_t* pArray, uint32_t itemSize, uint32_t index) {
  assert(pArray && index < pArray->capacity);
  uint8_t* pData = (uint8_t*)pArray->pData + (index * itemSize);
  if (*pData == 0) pArray->size++;
  return pData;
}

// --- Capacity ---
bool da_isEmpty(Array_t* pArray) { 
  return pArray == NULL || pArray->size == 0; 
}

bool da_reserve(Array_t* pArray, uint32_t itemSize, uint32_t newCapacity) {
  if (newCapacity <= pArray->capacity) return true;
  const size_t totalSize = (size_t)newCapacity * itemSize;
  void* pNew = realloc(pArray->pData, totalSize);
  if (!pNew) return false;
  memset(pNew, 0, totalSize);
  
  pArray->pData = pNew;
  pArray->capacity = newCapacity;
  return true;
}

bool da_shrink_to_fit(Array_t* pArray, uint32_t itemSize) {
  if (pArray->size == pArray->capacity) return true;
  
  if (pArray->size == 0) {
    free(pArray->pData);
    pArray->pData = NULL;
    pArray->capacity = 0;
    return true;
  }
  
  void* pNew = realloc(pArray->pData, (size_t)pArray->size * itemSize);
  if (!pNew) return false;
  
  pArray->pData = pNew;
  pArray->capacity = pArray->size;
  return true;
}

// --- Modifiers ---

void* da_append(Array_t* pArray, uint32_t itemSize, const void* pItem) {
  return da_append_ex(pArray, itemSize, pItem, &kDEFAULT_STRATEGY);
}

bool da_append_span(Array_t* pArray, uint32_t itemSize, Span_t span) {
  return da_append_span_ex(pArray, itemSize, span, &kDEFAULT_STRATEGY);
}

bool da_append_range(Array_t* pArray, uint32_t itemSize, Range_t range) {
  return da_append_range_ex(pArray, itemSize, range, &kDEFAULT_STRATEGY);
}

void* da_insert(Array_t* pArray, uint32_t itemSize, uint32_t index, const void* pItem) {
  return da_insert_ex(pArray, itemSize, index, pItem, &kDEFAULT_STRATEGY);
}

bool da_insert_span(Array_t* pArray, uint32_t itemSize, uint32_t index, Span_t span) {
  return da_insert_span_ex(pArray, itemSize, index, span, &kDEFAULT_STRATEGY);
}

bool da_insert_range(Array_t* pArray, uint32_t itemSize, uint32_t index, Range_t range) {
  return da_insert_range_ex(pArray, itemSize, index, range, &kDEFAULT_STRATEGY);
}

void da_pop_back(Array_t* pArray) { 
  if (pArray->size > 0) pArray->size--; 
}

void da_pop_back_n(Array_t* pArray, uint32_t n) {
  pArray->size = (n >= pArray->size) ? 0 : pArray->size - n;
}

void da_erase(Array_t* pArray, uint32_t itemSize, uint32_t index) {
  assert(index < pArray->size);
  unsigned char* pBase = (unsigned char*)pArray->pData;
  if (index < pArray->size - 1) {
    memmove(pBase + index * itemSize, 
            pBase + (index + 1) * itemSize, 
            (size_t)(pArray->size - index - 1) * itemSize);
  }
  pArray->size--;
}

void da_clear(Array_t* pArray) { 
  pArray->size = 0; 
}

void da_fill(Array_t* pArray, uint32_t itemSize, const void* pItem) {
  assert(pArray && pItem);
  unsigned char* pCursor = (unsigned char*)pArray->pData;
  for (uint32_t i = 0; i < pArray->size; ++i) {
    memcpy(pCursor, pItem, itemSize);
    pCursor += itemSize;
  }
}

void da_assign(Array_t* pArray, uint32_t itemSize, uint32_t count, const void* pItem) {
  da_clear(pArray);
  if (!da_reserve(pArray, itemSize, count)) return;
  pArray->size = count;
  da_fill(pArray, itemSize, pItem);
}

bool da_assign_span(Array_t* pArray, uint32_t itemSize, Span_t span) {
  da_clear(pArray);
  return da_append_span(pArray, itemSize, span);
}

bool da_assign_range(Array_t* pArray, uint32_t itemSize, Range_t range) {
  da_clear(pArray);
  return da_append_range(pArray, itemSize, range);
}

bool da_resize(Array_t* pArray, uint32_t itemSize, uint32_t newSize) {
  if (newSize < pArray->size) {
    pArray->size = newSize;
    return true;
  }
  if (newSize > pArray->size) {
    uint32_t diff = newSize - pArray->size;
    if (!da_reserve(pArray, itemSize, newSize)) return false;
    memset((unsigned char*)pArray->pData + (pArray->size * itemSize), 0, (size_t)diff * itemSize);
    pArray->size = newSize;
  }
  return true;
}

void da_swap(Array_t* pLeft, Array_t* pRight) {
  Array_t tmp = *pLeft;
  *pLeft = *pRight;
  *pRight = tmp;
}

// --- Extended Modifiers ---

void* da_append_ex(Array_t* pArray, uint32_t itemSize, const void* pItem, const Strategy_t* pStrategy) {
  if (!da_maybe_grow_ex(pArray, itemSize, pArray->size + 1, pStrategy)) return NULL;
  unsigned char* pTarget = (unsigned char*)pArray->pData + (pArray->size * itemSize);
  if (pItem) memcpy(pTarget, pItem, itemSize);
  else memset(pTarget, 0, itemSize);
  pArray->size++;
  return pTarget;
}

bool da_append_span_ex(Array_t* pArray, uint32_t itemSize, Span_t span, const Strategy_t* pStrategy) {
  uint32_t count = span.size;
  if (count == 0) return true;
  if (!da_maybe_grow_ex(pArray, itemSize, pArray->size + count, pStrategy)) return false;
  unsigned char* pTarget = (unsigned char*)pArray->pData + (pArray->size * itemSize);
  memcpy(pTarget, span.pData, (size_t)count * itemSize);
  pArray->size += count;
  return true;
}

bool da_append_range_ex(Array_t* pArray, uint32_t itemSize, Range_t range, const Strategy_t* pStrategy) {
  uint32_t count = da_calculate_range_count(range, itemSize);
  if (count == 0) return true;
  if (!da_maybe_grow_ex(pArray, itemSize, pArray->size + count, pStrategy)) return false;

  unsigned char* pCursor = (unsigned char*)pArray->pData + (pArray->size * itemSize);
  unsigned char* pSrc = (unsigned char*)range.pBegin;
  for (uint32_t i = 0; i < count; ++i) {
    memcpy(pCursor, pSrc, itemSize);
    pCursor += itemSize;
    pSrc += itemSize;
  }
  pArray->size += count;
  return true;
}

void* da_insert_ex(Array_t* pArray, uint32_t itemSize, uint32_t index, const void* pItem, const Strategy_t* pStrategy) {
  assert(index <= pArray->size);
  if (!da_maybe_grow_ex(pArray, itemSize, pArray->size + 1, pStrategy)) return NULL;
  unsigned char* pBase = (unsigned char*)pArray->pData;
  if (index < pArray->size) {
    memmove(pBase + (index + 1) * itemSize, pBase + index * itemSize, (size_t)(pArray->size - index) * itemSize);
  }
  void* pTarget = pBase + (index * itemSize);
  if (pItem) memcpy(pTarget, pItem, itemSize);
  else memset(pTarget, 0, itemSize);
  pArray->size++;
  return pTarget;
}

bool da_insert_span_ex(Array_t* pArray, uint32_t itemSize, uint32_t index, Span_t span, const Strategy_t* pStrategy) {
  assert(index <= pArray->size);
  uint32_t count = span.size;
  if (count == 0) return true;
  if (!da_maybe_grow_ex(pArray, itemSize, pArray->size + count, pStrategy)) return false;
  unsigned char* pBase = (unsigned char*)pArray->pData;
  if (index < pArray->size) {
    memmove(pBase + (index + count) * itemSize, pBase + index * itemSize, (size_t)(pArray->size - index) * itemSize);
  }
  memcpy(pBase + (index * itemSize), span.pData, (size_t)count * itemSize);
  pArray->size += count;
  return true;
}

bool da_insert_range_ex(Array_t* pArray, uint32_t itemSize, uint32_t index, Range_t range, const Strategy_t* pStrategy) {
  assert(index <= pArray->size);
  uint32_t count = da_calculate_range_count(range, itemSize);
  if (count == 0) return true;
  if (!da_maybe_grow_ex(pArray, itemSize, pArray->size + count, pStrategy)) return false;

  unsigned char* pBase = (unsigned char*)pArray->pData;
  if (index < pArray->size) {
    memmove(pBase + (index + count) * itemSize, pBase + index * itemSize, (size_t)(pArray->size - index) * itemSize);
  }

  unsigned char* pCursor = pBase + (index * itemSize);
  unsigned char* pSrc = (unsigned char*)range.pBegin;
  for (uint32_t i = 0; i < count; ++i) {
    memcpy(pCursor, pSrc, itemSize);
    pCursor += itemSize;
    pSrc += itemSize;
  }
  pArray->size += count;
  return true;
}
