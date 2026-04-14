#include "cLib/memory/strategy/DataStrategy.h"
#include "cLib/memory/zone/MemBlock.h"
#include "cLib/memory/allocator/NullAllocator.h"

/* ============================================================
 * TYPE_DATA strategy — raw leaf data, no sub-allocator.
 * ============================================================ */
void        DataStrategy_Init        (MemBlock_t* b, uint32_t c, uint32_t t, const char* n) { (void)b;(void)c;(void)t;(void)n; }
void        DataStrategy_Clear       (MemBlock_t* b) { (void)b; }
void        DataStrategy_Purge       (MemBlock_t* b) { (void)b; }
bool        DataStrategy_Check       (MemBlock_t* b) { (void)b; return true; }
void        DataStrategy_Remap       (MemBlock_t* b) { (void)b; }
void        DataStrategy_Save        (MemBlock_t* b, const char* f) { Block_Save(b, f); }
MemBlock_t* DataStrategy_Load        (const char* f) { (void)f; return NULL; }
Allocator_t DataStrategy_GetAllocator(MemBlock_t* b) { (void)b; return Null_GetAllocator(NULL); }
const char* DataStrategy_GetBlockName (MemBlock_t* b) { (void)b; return NULL; }
const MemBlock_t* DataStrategy_FindBlock(MemBlock_t* b, Name_t n) { (void)b; (void)n; return &kInvalidBlock; }

const MemStrategy_t kDataStrategy = {
  DataStrategy_Init,
  DataStrategy_Clear,
  DataStrategy_Purge,
  DataStrategy_Check,
  DataStrategy_Remap,
  DataStrategy_Save,
  DataStrategy_Load,
  DataStrategy_GetAllocator,
  DataStrategy_GetBlockName,
  DataStrategy_FindBlock,
};
