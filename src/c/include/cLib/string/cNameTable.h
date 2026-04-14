#ifndef C_NAME_TABLE_H
#define C_NAME_TABLE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <cLib/string/cName.h>
#include <cLib/string/cStringView.h>
#include <cLib/memory/cBuffer.h>

#define GLOBAL_TABLE nullptr

typedef struct cNameTableStorage {
  Buffer_t slotBuffer;   
  Buffer_t stringBuffer; 
} NameTableStorage_t;

typedef struct cSlotBuffer {
  Name_t* pData = nullptr;
  uint32_t count = 0; // Number of available Name_t slots
} NameSlotBuffer_t;

typedef struct cNameTable {
  NameSlotBuffer_t nameSlotBuffer; 
  TextBuffer_t     stringBuffer;   
  uint32_t         activeSlots    = 0;
  size_t           bufferOffset   = 0;
  bool             bIsInitialized = false;
} NameTable_t;

void name_table_init_global(NameTableStorage_t storage);
void name_table_init(NameTable_t* pTable, NameTableStorage_t storage);

// Intern a string: returns an existing Name_t if already interned, otherwise
// copies str into the string arena and inserts a new slot.
Name_t name_create_str(NameTable_t* pTable, const char* str);

Name_t name_create(NameTable_t* pTable, StringView_t view);
void   name_table_log_stats(const NameTable_t* pTable);

#ifdef __cplusplus
} // extern "C"
#endif
#endif // C_NAME_TABLE_H
