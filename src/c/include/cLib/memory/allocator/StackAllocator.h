#ifndef STACK_ALLOCATOR_H
#define STACK_ALLOCATOR_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cLib/memory/allocator/Allocator.h"

typedef struct cMemStack {
  MemBlock_t  data;           // 32 bytes: Describes the entire footprint
  uint32_t    used      = 0;  //  4 bytes
  uint32_t    capacity  = 0;  //  4 bytes
  uint32_t    version   = 0;  //  4 bytes +1 on move/reload
  uint32_t    padding1;       //  4 bytes — reserved
  Name_t      name;           // 16 bytes (const char* ptr + FNV hash)
} MemStack_t; // 64 bytes
static_assert(sizeof(MemStack_t) == 64, "MemStack_t must be exactly 64 bytes");

static const uint32_t kMEM_STACK_SIZE  = (sizeof(MemStack_t) + ALIGN_MASK) & ~ALIGN_MASK;

// Reinterpret pBlock as the containing MemStack_t (same address — data is first member).
static inline MemStack_t* Block_AsStack(MemBlock_t* pBlock) {
  return (MemStack_t*)pBlock;
}

/* --- Interface --- */
void*     Stack_GetData(MemStack_t* pStack, Handle_t* pHandle);
void      Stack_Clear(MemStack_t* pStack);
Handle_t  Stack_Malloc(MemStack_t* pStack, uint32_t size, void* pUser);
Handle_t  Stack_Realloc(MemStack_t* pStack, Handle_t* pHandle, uint32_t newSize);
void      Stack_Free(MemStack_t* pStack, Handle_t* pHandle);

// Initializes a stack at the provided memory location
Allocator_t Stack_GetAllocator(MemStack_t* pStack);

#ifdef __cplusplus
} // extern "C"
#endif
#endif // STACK_ALLOCATOR_H
