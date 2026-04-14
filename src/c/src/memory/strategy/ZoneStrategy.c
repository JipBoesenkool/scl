#include "cLib/memory/strategy/ZoneStrategy.h"
#include "cLib/memory/strategy/MemStrategy.h"
#include "cLib/memory/allocator/ZoneAllocator.h"
#include "cLib/memory/zone/Zone.h"
#include "cLib/memory/zone/ZoneStats.h"
#include "cLib/string/cName.h"
#include "cLib/Log.h"

/* ============================================================
 * TYPE_ZONE strategy — payload is a MemZone_t sub-zone.
 * ============================================================ */
//TODO: fix Block_AsZone
void ZoneStrategy_Init(MemBlock_t* pBlock, uint32_t capacity, uint32_t tag, const char* name)
{
  (void)capacity; /* sub-zone payload = pBlock->size - header; ignore caller-supplied value */
  Z_Init( Block_AsSubZone(pBlock), pBlock->size - kMEM_BLOCK_SIZE, tag, name);
}

void ZoneStrategy_Clear(MemBlock_t* pBlock)
{
  Z_Clear( Z_BlockAsZone(pBlock) );
}

void ZoneStrategy_Purge(MemBlock_t* pBlock)
{
  Z_Compact( Z_BlockAsZone(pBlock) );
}

bool ZoneStrategy_Check(MemBlock_t* pBlock)
{
  return Z_Check( Z_BlockAsZone(pBlock) );
}

void ZoneStrategy_Remap(MemBlock_t* pBlock)
{
  Z_Remap( Z_BlockAsZone(pBlock) );
}

void ZoneStrategy_Save(MemBlock_t* pBlock, const char* filename)
{
  (void)pBlock; (void)filename;
  Log_Add("[TODO] ZoneStrategy_Save: not implemented.");
}

MemBlock_t* ZoneStrategy_Load(const char* filename)
{
  (void)filename;
  Log_Add("[TODO] ZoneStrategy_Load: not implemented.");
  return NULL;
}

Allocator_t ZoneStrategy_GetAllocator(MemBlock_t* pBlock)
{
  return Zone_GetAllocator( (MemZone_t*)pBlock );
}

const char* ZoneStrategy_GetBlockName(MemBlock_t* pBlock)
{
  return ((MemZone_t*)GetBlockData(pBlock))->name.name;
}

const MemBlock_t* ZoneStrategy_FindBlock(MemBlock_t* pBlock, Name_t name)
{
  MemZone_t* pZone    = Block_AsSubZone(pBlock);
  uint32_t anchorOff  = Z_GetOffset(pZone, &pZone->blocklist);
  uint32_t currOff    = pZone->blocklist.next;

  while (currOff != anchorOff) {
    MemBlock_t* pChild = Z_GetBlock(pZone, currOff);
    if (pChild->tag != PU_FREE) {
      const char* childName = MEM_STRATEGY(pChild).GetBlockName(pChild);
      if (childName != NULL && name_equals(name_from_cstr(childName), name)) {
        return pChild;
      }
      // Recurse via strategy dispatch — no direct type check
      const MemBlock_t* pFound = MEM_STRATEGY(pChild).FindBlock(pChild, name);
      if (Block_IsValid(pFound)) return pFound;
    }
    currOff = pChild->next;
  }
  return &kInvalidBlock;
}

const MemStrategy_t kZoneStrategy = {
  ZoneStrategy_Init,
  ZoneStrategy_Clear,
  ZoneStrategy_Purge,
  ZoneStrategy_Check,
  ZoneStrategy_Remap,
  ZoneStrategy_Save,
  ZoneStrategy_Load,
  ZoneStrategy_GetAllocator,
  ZoneStrategy_GetBlockName,
  ZoneStrategy_FindBlock,
};
