#ifndef MEMORY_TOOL_HPP
#define MEMORY_TOOL_HPP

#include <stdint.h>
#include <cLib/memory/zone/Zone.h>

/**
 * Global UI State for the Debugger.
 */
typedef struct cMemoryGuiState {
  /* Map / filter */
  uintptr_t selectedBlockAddr = 0;
  uint32_t  activeTagFilter   = 0xFFFFFFFF;
  bool      bShowSubZones     = true;
  /* Block actions */
  char      saveFile[64]      = "block_dump.bin";
  /* Zone actions */
  int32_t   allocSize         = 1024; // *1024
  int32_t   allocTag          = 50; // PU_LEVEL
  // make the view scrollable
  bool bAutoZoom = true;
  float zoomFactor   = 10.0f;   // 1.0 = 100% view, 10.0 = 10x zoom
  float scrollOffset = 0.0f; // Normalized 0.0 to 1.0
} MemoryGuiState_t;

/**
 * Renders the full DoomZone Debugger window.
 * @param pHeap The base pointer to the master MemZone_t.
 */
void GUI_Update(MemZone_t* pZone);

#endif
