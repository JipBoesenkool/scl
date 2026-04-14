#ifndef C_RANGE_H
#define C_RANGE_H

/* 
   --- C ABI LAYER (POD Structs) --- 
*/
typedef struct cRange {
    void*  pBegin;
    void*  pSentinel;
} Range_t;

#endif // C_RANGE_H
