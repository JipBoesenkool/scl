#include "cLib/memory/strategy/NullStrategy.h"
#include "cLib/memory/zone/MemBlock.h"
#include "cLib/memory/allocator/NullAllocator.h"

/* ============================================================
 * TYPE_NONE strategy — no-op guard for untyped/uninitialized blocks.
 * ============================================================ */
void        NullStrategy_Init        (MemBlock_t* b, uint32_t c, uint32_t t, const char* n) { (void)b;(void)c;(void)t;(void)n; }
void        NullStrategy_Clear       (MemBlock_t* b) { (void)b; }
void        NullStrategy_Purge       (MemBlock_t* b) { (void)b; }
bool        NullStrategy_Check       (MemBlock_t* b) { (void)b; return true; }
void        NullStrategy_Remap       (MemBlock_t* b) { (void)b; }
void        NullStrategy_Save        (MemBlock_t* b, const char* f) { (void)b; (void)f; }
MemBlock_t* NullStrategy_Load        (const char* f) { (void)f; return NULL; }
Allocator_t NullStrategy_GetAllocator(MemBlock_t* b) { (void)b; return Null_GetAllocator(NULL); }
const char* NullStrategy_GetBlockName (MemBlock_t* b) { (void)b; return NULL; }
const MemBlock_t* NullStrategy_FindBlock(MemBlock_t* b, Name_t n) { (void)b; (void)n; return &kInvalidBlock; }

const MemStrategy_t kNullStrategy = {
  NullStrategy_Init,
  NullStrategy_Clear,
  NullStrategy_Purge,
  NullStrategy_Check,
  NullStrategy_Remap,
  NullStrategy_Save,
  NullStrategy_Load,
  NullStrategy_GetAllocator,
  NullStrategy_GetBlockName,
  NullStrategy_FindBlock,
};
