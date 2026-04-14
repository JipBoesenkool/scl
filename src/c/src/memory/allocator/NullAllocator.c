#include "cLib/memory/allocator/NullAllocator.h"
#include "cLib/Log.h"

/* --- Interface Implementation --- */
void* Null_GetData(void* pNull, Handle_t* pHandle) {
  (void)pNull;
  Log_Add("[WARNING] Null_GetData");
  *pHandle = kInvalidHandle; 
  return NULL;
}

void Null_Clear(void* pNull) {
  (void)pNull;
  Log_Add("[WARNING] Null_Clear");
}

Handle_t Null_Malloc(void* pNull, uint32_t size, void* pUser) {
  (void)pNull; (void)size; (void)pUser;
  Log_Add("[WARNING] Null_Malloc");
  if (pUser != NULL) {
    *(void**)pUser = NULL;
  }
  return kInvalidHandle;
}

Handle_t Null_Realloc(void* pNull, Handle_t* pHandle, uint32_t newSize) {
  (void)pNull; (void)newSize;
  Log_Add("[WARNING] Null_Realloc");
  // Null allocator always fails to reallocate
  if (pHandle) {
    *pHandle = kInvalidHandle;
  }
  return kInvalidHandle;
}

void Null_Free(void* pNull, Handle_t* pHandle) {
  (void)pNull;
  Log_Add("[WARNING] Null_Free");
  if (pHandle) {
    *pHandle = kInvalidHandle;
  }
}

Allocator_t Null_GetAllocator(void* pContext) {
  Allocator_t alloc = {
    .pContext = pContext,
    .GetData  = Null_GetData,
    .Clear    = Null_Clear,
    .Malloc   = Null_Malloc,
    .Realloc  = Null_Realloc,
    .Free     = Null_Free
  };
  return alloc;
}

const Allocator_t INVALID_ALLOCATOR = {
  .pContext = NULL,
  .GetData  = Null_GetData,
  .Clear    = Null_Clear,
  .Malloc   = Null_Malloc,
  .Realloc  = Null_Realloc,
  .Free     = Null_Free
};
