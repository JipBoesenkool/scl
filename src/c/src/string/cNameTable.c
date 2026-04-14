#include <cLib/string/cNameTable.h>
#include <string.h>
#include <stdio.h>
#include <cassert>
#include <stdint.h>

static NameTable_t g_GlobalTable;

void name_table_init(NameTable_t* pTable, NameTableStorage_t storage) {
  assert(pTable != nullptr);
  assert(storage.slotBuffer.pData != nullptr);
  assert(storage.stringBuffer.pData != nullptr);

  size_t elementCount = storage.slotBuffer.size / sizeof(Name_t);

  // Rigid Check: Ensure the number of slots doesn't exceed uint32_t limits
  assert(elementCount <= UINT32_MAX && "Slot buffer is too large for uint32_t indexing!");
  // Slot count MUST be a power of two — the hash probe uses (hash & (count-1)).
  assert(elementCount > 0 && (elementCount & (elementCount - 1)) == 0 &&
         "NameTable slot count must be a power of two!");
  pTable->nameSlotBuffer.pData = (Name_t*)storage.slotBuffer.pData;
  pTable->nameSlotBuffer.count = (uint32_t)elementCount;
  
  pTable->stringBuffer.pData   = (char*)storage.stringBuffer.pData;
  pTable->stringBuffer.size    = storage.stringBuffer.size;

  pTable->activeSlots  = 0;
  pTable->bufferOffset = 0;
  pTable->bIsInitialized = true;

  // Zero out slots so all hashes start as INVALID_HASH (0)
  memset(pTable->nameSlotBuffer.pData, 0, storage.slotBuffer.size);
}

void name_table_init_global(NameTableStorage_t storage) {
  name_table_init(&g_GlobalTable, storage);
}

void name_table_log_stats(const NameTable_t* pTable) {
  const NameTable_t* pTarget = (pTable == GLOBAL_TABLE) ? &g_GlobalTable : pTable;
  if (!pTarget || !pTarget->bIsInitialized) return;

  float slotLoad = (float)pTarget->activeSlots / (float)pTarget->nameSlotBuffer.count;
  float memLoad  = (float)pTarget->bufferOffset / (float)pTarget->stringBuffer.size;

  printf("--- NameTable Stats ---\n");
  printf("Slots: %u/%u (%.2f%% load)\n", pTarget->activeSlots, pTarget->nameSlotBuffer.count, slotLoad * 100.0f);
  printf("Memory: %zu/%zu bytes (%.2f%% load)\n", pTarget->bufferOffset, pTarget->stringBuffer.size, memLoad * 100.0f);
  
  if (slotLoad > 0.70f) {
    printf("[WARNING]: Slot load exceeds 70%%. Performance will degrade!\n");
  }
  printf("-----------------------\n");
}


Name_t name_create_str(NameTable_t* pTable, const char* str) {
  if (str == nullptr || str[0] == '\0') return INVALID_NAME;

  NameTable_t* pActiveTable = (pTable == GLOBAL_TABLE) ? &g_GlobalTable : pTable;
  assert(pActiveTable->bIsInitialized && "NameTable used before init");

  size_t   len  = strlen(str);
  uint32_t hash = fnv_hash(str, len);

  // Slot count MUST be a power of two for this bitwise mask to work.
  uint32_t mask  = pActiveTable->nameSlotBuffer.count - 1;
  uint32_t index = hash & mask;

  // Open addressing with linear probing.
  for (uint32_t i = 0; i < pActiveTable->nameSlotBuffer.count; ++i) {
    uint32_t slotIdx = (index + i) & mask;
    Name_t*  pSlot   = &pActiveTable->nameSlotBuffer.pData[slotIdx];

    // Case 1: Empty slot — intern the string here.
    if (pSlot->hash == INVALID_HASH) {
      size_t required = len + 1;
      assert(pActiveTable->bufferOffset + required <= pActiveTable->stringBuffer.size
             && "NameTable string buffer full!");

      char* pInterned = pActiveTable->stringBuffer.pData + pActiveTable->bufferOffset;
      memcpy(pInterned, str, len);
      pInterned[len] = '\0';

      pSlot->name = pInterned;
      pSlot->hash = hash;

      pActiveTable->bufferOffset += required;
      pActiveTable->activeSlots++;

#ifndef NDEBUG
      float loadFactor = (float)pActiveTable->activeSlots / (float)pActiveTable->nameSlotBuffer.count;
      if (loadFactor > 0.70f && (pActiveTable->activeSlots % 10 == 0))
        name_table_log_stats(pActiveTable);
#endif
      return *pSlot;
    }

    // Case 2: Occupied slot — verify it's actually the same string.
    if (pSlot->hash == hash) {
      if (strncmp(pSlot->name, str, len) == 0 && pSlot->name[len] == '\0')
        return *pSlot;
    }
  }
  assert(false && "NameTable slot buffer full — no empty slots available.");
  return INVALID_NAME;
}


Name_t name_create(NameTable_t* pTable, StringView_t view) {
  if (view.length == 0) return INVALID_NAME;

  NameTable_t* pActiveTable = (pTable == GLOBAL_TABLE) ? &g_GlobalTable : pTable;
  assert(pActiveTable->bIsInitialized && "NameTable used before init");

  uint32_t hash = fnv_hash_string_view(view);
  
  // Requirement: Slot count MUST be a power of two for this bitwise mask to work.
  uint32_t mask = pActiveTable->nameSlotBuffer.count - 1;
  uint32_t index = hash & mask;

  // Strategy: Open Addressing with Linear Probing.
  // We search for an empty slot or a matching interned string.
  for (uint32_t i = 0; i < pActiveTable->nameSlotBuffer.count; ++i) {
    uint32_t slotIdx = (index + i) & mask;
    Name_t* pSlot = &pActiveTable->nameSlotBuffer.pData[slotIdx];

    // Case 1: Empty slot found. Intern the string.
    if (pSlot->hash == INVALID_HASH)
    {
      size_t required = view.length + 1;
      
      assert(pActiveTable->bufferOffset + required <= pActiveTable->stringBuffer.size 
             && "NameTable String Buffer Full!");

      char* pInterned = pActiveTable->stringBuffer.pData + pActiveTable->bufferOffset;
      memcpy(pInterned, view.data, view.length);
      pInterned[view.length] = '\0';

      pSlot->name = pInterned;
      pSlot->hash = hash;

      pActiveTable->bufferOffset += required;
      pActiveTable->activeSlots++;

#ifndef NDEBUG
      // Simple threshold monitoring
      float loadFactor = (float)pActiveTable->activeSlots / (float)pActiveTable->nameSlotBuffer.count;
      if (loadFactor > 0.70f && (pActiveTable->activeSlots % 10 == 0)) {
         name_table_log_stats(pActiveTable);
      }
#endif
      return *pSlot;
    }

    // Case 2: Slot occupied. Check for hash match and string equality.
    if (pSlot->hash == hash)
    {
      if (strncmp(pSlot->name, view.data, view.length) == 0 && pSlot->name[view.length] == '\0') {
        return *pSlot;
      }
    }
  }

  assert(false && "NameTable Slot Buffer Full! No empty slots available.");
  return INVALID_NAME;
}
