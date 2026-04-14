#include <cLib/memory/allocator/DefaultAllocator.h>
#include <stdlib.h>
#include <stdint.h>

/* Sequential ID counter — wraps at UINT32_MAX. Not thread-safe. */
static uint32_t s_nextId = 1;

void* Default_GetData(void* pContext, Handle_t* pHandle) {
  (void)pContext;
  if (!pHandle) return NULL;
  return pHandle->pData;
}

void Default_Clear(void* pContext) {
  (void)pContext;
  /* Stateless — nothing to clear. */
}

Handle_t Default_Malloc(void* pContext, uint32_t size, void* pUser) {
  (void)pContext;
  Handle_t h = { s_nextId++, 0, NULL };
  if (size == 0) return h;

  h.pData = malloc(size);
  if (pUser) {
    *(void**)pUser = h.pData;
  }
  return h;
}

Handle_t Default_Realloc(void* pContext, Handle_t* pHandle, uint32_t newSize) {
  (void)pContext;
  if (!pHandle) {
    Handle_t h = { s_nextId++, 0, NULL };
    h.pData = newSize ? malloc(newSize) : NULL;
    return h;
  }
  void* p = realloc(pHandle->pData, newSize);
  pHandle->pData = p;
  return *pHandle;
}

void Default_Free(void* pContext, Handle_t* pHandle) {
  (void)pContext;
  if (!pHandle || !pHandle->pData) return;
  free(pHandle->pData);
  pHandle->pData = NULL;
}

Allocator_t Default_GetAllocator(void) {
  Allocator_t alloc = {
    .pContext = NULL,
    .GetData  = Default_GetData,
    .Clear    = Default_Clear,
    .Malloc   = Default_Malloc,
    .Realloc  = Default_Realloc,
    .Free     = Default_Free
  };
  return alloc;
}
