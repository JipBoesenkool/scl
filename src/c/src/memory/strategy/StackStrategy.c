#include "cLib/memory/strategy/StackStrategy.h"
#include "cLib/memory/zone/MemBlock.h"
#include "cLib/memory/allocator/StackAllocator.h"
#include "cLib/Log.h"
#include <string.h>

/* ============================================================
 * TYPE_STACK strategy — payload is a MemStack_t linear allocator.
 * Stubs: implement when nested stacks are in use.
 * ============================================================ */
void StackStrategy_Init(MemBlock_t* pBlock, uint32_t capacity, uint32_t tag, const char* name)
{
  if (!pBlock) return;
  MemStack_t* pStack = (MemStack_t*)GetBlockData(pBlock);
  memset(pStack, 0, capacity);
  // 1. Initialize the internal MemBlock_t (the 'first' member)
  // This allows the stack itself to look like a valid block if scanned.
  pStack->data.magic  = CHECK_SUM;
  pStack->data.size   = capacity;
  pStack->data.tag    = tag;
  pStack->data.type   = TYPE_STACK;
  pStack->data.ppUser = NULL; //TODO: we could store our address here?
  pStack->data.next   = 0;
  pStack->data.prev   = 0;
  // 2. Setup Stack Metadata
  pStack->used     = 0;
  pStack->capacity = capacity - kMEM_STACK_SIZE;
  if (name) {
    pStack->name = name_from_cstr(name);
  }
}

void StackStrategy_Clear(MemBlock_t* pBlock)
{
  MemStack_t* pStack = (MemStack_t*)GetBlockData(pBlock);
  Stack_Clear( pStack );
}

void StackStrategy_Purge(MemBlock_t* pBlock)
{
  MemStack_t* pStack = (MemStack_t*)GetBlockData(pBlock);
  Stack_Clear( pStack );
}

bool StackStrategy_Check(MemBlock_t* pBlock) {
  MemStack_t* pStack = (MemStack_t*)GetBlockData(pBlock);
  return (pStack->used <= pStack->capacity);
}

void StackStrategy_Remap(MemBlock_t* pContext) {
  // Stacks use relative offsets for used/capacity; no absolute pointers to fix.
  (void)pContext;
}

void StackStrategy_Save(MemBlock_t* pBlock, const char* filename)
{
  (void)pBlock; (void)filename;
  MemStack_t* pStack = (MemStack_t*)pBlock;
  pStack->version++; //Update this before we save it?
  //TODO: similar to Save_block?
  Log_Add("[TODO] StackStrategy_Save: not implemented.");
}

MemBlock_t* StackStrategy_Load(const char* filename)
{
  (void)filename;
  Log_Add("[TODO] StackStrategy_Load: not implemented.");
  return NULL;
}

Allocator_t StackStrategy_GetAllocator(MemBlock_t* pBlock)
{
  return Stack_GetAllocator( (MemStack_t*)GetBlockData(pBlock) );
}

const char* StackStrategy_GetBlockName(MemBlock_t* pBlock)
{
  return ((MemStack_t*)GetBlockData(pBlock))->name.name;
}

const MemBlock_t* StackStrategy_FindBlock(MemBlock_t* b, Name_t n) { (void)b; (void)n; return &kInvalidBlock; }

/* ============================================================
 * Global StackStrategy table
 * ============================================================ */
const MemStrategy_t kStackStrategy = {
  StackStrategy_Init,
  StackStrategy_Clear,
  StackStrategy_Purge,
  StackStrategy_Check,
  StackStrategy_Remap,
  StackStrategy_Save,
  StackStrategy_Load,
  StackStrategy_GetAllocator,
  StackStrategy_GetBlockName,
  StackStrategy_FindBlock,
};
