#ifndef NULL_ALLOCATOR_H
#define NULL_ALLOCATOR_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cLib/memory/allocator/Allocator.h"

typedef struct cMemNull {
  MemBlock_t  data;           // 32 bytes: Describes the entire footprint
  uint32_t    capacity = 0;   // 4 bytes
  uint32_t    padding1;       // 4 bytes — reserved
  uint32_t    padding2;       // 4 bytes — reserved
  uint32_t    padding3;       // 4 bytes — reserved
  Name_t      name;           // 16 bytes (const char* ptr + FNV hash)
} MemNull_t; // 64 bytes
static_assert(sizeof(MemNull_t) == 64, "MemNull_t must be exactly 64 bytes");

static const uint32_t kMEM_NULL_SIZE  = (sizeof(MemNull_t) + ALIGN_MASK) & ~ALIGN_MASK;

// --- Interface --- 
void*     Null_GetData(void* pContext, Handle_t* pHandle);
void      Null_Clear(void* pContext);
Handle_t  Null_Malloc(void* pContext, uint32_t size, void* pUser);
Handle_t  Null_Realloc(void* pContext, Handle_t* pHandle, uint32_t newSize);
void      Null_Free(void* pContext, Handle_t* pHandle);

// An allocator which returns invalid handles and logs [WARNING]
Allocator_t Null_GetAllocator(void* pContext);

// NullObject sentinel for "allocator not found". Safe to call — logs [WARNING].
// Equivalent to Null_GetAllocator(NULL). Use Allocator_IsValid to test.
extern const Allocator_t INVALID_ALLOCATOR;

#ifdef __cplusplus
} // extern "C"
#endif
#endif // NULL_ALLOCATOR_H
