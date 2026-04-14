#ifndef DEFAULT_ALLOCATOR_H
#define DEFAULT_ALLOCATOR_H
#ifdef __cplusplus
extern "C" {
#endif

#include <cLib/memory/allocator/Allocator.h>

/*
 * DefaultAllocator — a stateless Allocator_t that wraps malloc/realloc/free.
 *
 * ppUser is ignored (no defrag support). Handles are assigned sequential IDs.
 * Use this as the fallback allocator when no Zone or Stack is available.
 *
 * Future note: when patching/defrag allocators exist, callers that registered
 * a ppUser with a DefaultAllocator will need to be updated.
 */

/* --- Interface --- */
void*     Default_GetData(void* pContext, Handle_t* pHandle);
void      Default_Clear(void* pContext);
Handle_t  Default_Malloc(void* pContext, uint32_t size, void* pUser);
Handle_t  Default_Realloc(void* pContext, Handle_t* pHandle, uint32_t newSize);
void      Default_Free(void* pContext, Handle_t* pHandle);

/* Returns a stateless Allocator_t wrapping malloc/realloc/free.
 * pContext is unused — pass NULL. */
Allocator_t Default_GetAllocator(void);

#ifdef __cplusplus
} // extern "C"
#endif
#endif // DEFAULT_ALLOCATOR_H
