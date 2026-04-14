#ifndef C_SPAN_H
#define C_SPAN_H

/* 
   --- C ABI LAYER (POD Structs) --- 
*/
typedef struct cSpan {
    void*  pData;
    size_t size;
} Span_t;

typedef int (*fCompareItems)(const void* pA, const void* pB);

/**
 * Compares two spans for equality and order.
 * Returns 0 if equal, -1 if left < right, 1 if left > right.
 */
inline int span_compare(Span_t left, Span_t right, uint32_t itemSize, fCompareItems pfnCompare) {
  if (left.size < right.size) return -1;
  if (left.size > right.size) return 1;

  unsigned char* pL = (unsigned char*)left.pData;
  unsigned char* pR = (unsigned char*)right.pData;

  for (uint32_t i = 0; i < left.size; ++i) {
    int res = pfnCompare(pL, pR);
    if (res != 0) return res;
    pL += itemSize;
    pR += itemSize;
  }
  return 0;
}

#endif // C_SPAN_H
