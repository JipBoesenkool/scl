/**
 * ZONE ALLOCATOR GUI TOOL (V2.6.1)
 * [2026-04-08] Refactored for human readability: expressive variable names.
 * [2026-04-08] Integrated modular headers: Log.hpp and Tag.hpp.
 * [2026-04-08] Unified block rendering and alpha blending using TagInfo registry.
 */

#include "MemoryGui.hpp"
#include "imgui.h"

#include <cLib/memory/zone/Zone.h>
#include <cLib/memory/zone/ZoneStats.h>
#include <cLib/memory/zone/MemBlock.h>
#include <cLib/memory/allocator/ZoneAllocator.h>
#include <cLib/memory/allocator/StackAllocator.h>
#include <cLib/memory/strategy/MemStrategy.h>
#include <cLib/Log.h>

#include "Log.hpp"
#include "Tag.hpp"

#include <stdio.h>

 // Global UI state for the debugger instance
static MemoryGuiState_t gState;

/**
 * DrawMemoryBlock
 * Visualizes a single memory block, splitting it into:
 * 1. The Header segment (Fixed size, usually gray)
 * 2. The User Data segment (Variable size, colored by tag)
 */
static void DrawMemoryBlock(
  ImDrawList* pDrawList,
  MemZone_t* pZone,
  MemBlock_t* pBlock,
  uint32_t offset,
  ImVec2 pos,
  float totalWidth,
  float height
) {
  TagInfo info = GetTagInfo(pBlock->tag);

  bool bIsFiltered = (gState.activeTagFilter != PU_ALL);
  bool bMatches = (pBlock->tag == gState.activeTagFilter);
  float alphaMult = (bIsFiltered && !bMatches) ? 0.2f : 1.0f;

  auto ApplyAlpha = [](ImU32 col, float alpha) -> ImU32 {
    ImVec4 v = ImGui::ColorConvertU32ToFloat4(col);
    v.w *= alpha;
    return ImGui::ColorConvertFloat4ToU32(v);
    };

  ImU32 headerColor = ApplyAlpha(IM_COL32(100, 100, 105, 255), alphaMult);
  ImU32 bodyColor = ApplyAlpha(info.color, alphaMult);

  // --- Scaled Header Width ---
  // We use a base of 4px plus 1% of the block width, clamped so it never 
  // exceeds 20% of the total block width.
  float headerWidth = 4.0f + (totalWidth * 0.01f);
  if (headerWidth > totalWidth * 0.2f) headerWidth = totalWidth * 0.2f;

  pDrawList->AddRectFilled(pos, ImVec2(pos.x + headerWidth, pos.y + height), headerColor);
  pDrawList->AddRectFilled(ImVec2(pos.x + headerWidth, pos.y), ImVec2(pos.x + totalWidth, pos.y + height), bodyColor);
  pDrawList->AddRect(pos, ImVec2(pos.x + totalWidth, pos.y + height), IM_COL32(0, 0, 0, 180));

  // --- Usage overlay for TYPE_ZONE / TYPE_STACK ---
  // Dark rect covers the unused portion; white line marks the used boundary.
  float usedFrac = -1.0f;
  if (pBlock->tag != PU_FREE) {
    if (pBlock->type == TYPE_ZONE) {
      MemZone_t* pSub = (MemZone_t*)GetBlockData(pBlock);
      if (pSub->capacity > 0) usedFrac = (float)pSub->used / (float)pSub->capacity;
    } else if (pBlock->type == TYPE_STACK) {
      MemStack_t* pStk = (MemStack_t*)GetBlockData(pBlock);
      if (pStk->capacity > 0) usedFrac = (float)pStk->used / (float)pStk->capacity;
    }
  }
  if (usedFrac >= 0.0f) {
    float bodyW   = totalWidth - headerWidth;
    float usedEnd = pos.x + headerWidth + bodyW * usedFrac;
    float rightX  = pos.x + totalWidth;
    if (usedEnd < rightX) {
      pDrawList->AddRectFilled(ImVec2(usedEnd, pos.y), ImVec2(rightX, pos.y + height),
                               IM_COL32(0, 0, 0, 120));
      pDrawList->AddLine(ImVec2(usedEnd, pos.y), ImVec2(usedEnd, pos.y + height),
                         IM_COL32(255, 255, 255, 180), 1.0f);
    }
  }

  if ((uintptr_t)pBlock == gState.selectedBlockAddr) {
    pDrawList->AddRect(ImVec2(pos.x - 1, pos.y - 1), ImVec2(pos.x + totalWidth + 1, pos.y + height + 1), IM_COL32(255, 255, 255, 255), 0, 0, 1.5f);
  }

  ImGui::SetCursorScreenPos(pos);
  char buttonId[32];
  sprintf(buttonId, "##block_%u", offset);

  if (ImGui::InvisibleButton(buttonId, ImVec2(totalWidth > 1.0f ? totalWidth : 1.0f, height))) {
    gState.selectedBlockAddr = (uintptr_t)pBlock;
  }

  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    if (ImGui::GetMousePos().x < pos.x + headerWidth) {
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "[BLOCK HEADER]");
      ImGui::Separator();
      ImGui::Text("Next Offset: %u", pBlock->next);
      ImGui::Text("Prev Offset: %u", pBlock->prev);
      ImGui::Text("Magic: 0x%X", pBlock->magic);
    }
    else {
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "[%s DATA]", info.label);
      ImGui::Separator();
      ImGui::Text("Zone Offset: %u", offset);
      ImGui::Text("Payload: %u bytes", pBlock->size - (uint32_t)sizeof(MemBlock_t));
      ImGui::Text("Tag ID: %u", pBlock->tag);
    }
    ImGui::EndTooltip();
  }
}

/**
 * DrawHeaderStats
 * Renders the top metrics bar including name, free space
 */
static void DrawHeaderStats(MemZone_t* pZone) {
  float usageRatio = (float)pZone->used / (float)pZone->capacity;
  uint32_t freeSize = Z_GetUsedSpace(pZone, PU_FREE);
  uint32_t purgable = Z_GetPurgeableSpace(pZone);

  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.8f, 1.0f), "Zone: %s |", pZone->name.name);
  ImGui::SameLine();

  ImGui::Text("Free: %u KB | Purgeable: %u KB | Total Available: %u KB",
    freeSize / 1024, purgable / 1024, (freeSize + purgable) / 1024);

  // Visual usage progress bar
  char overlayText[64];
  sprintf(overlayText, "%u / %u KB (%.1f%%)", pZone->used / 1024, pZone->capacity / 1024, usageRatio * 100.0f);
  ImGui::ProgressBar(usageRatio, ImVec2(-1, 20.0f), overlayText);
}

/**
 * DrawBlockInspectorTable
 * Displays detailed internal structure fields for the currently selected block.
 */
static void DrawBlockInspectorTable() {
  if (gState.selectedBlockAddr == 0) {
    ImGui::TextDisabled("Select a block in the map to inspect...");
    return;
  }

  MemBlock_t* pBlock = (MemBlock_t*)gState.selectedBlockAddr;
  TagInfo info = GetTagInfo(pBlock->tag);

  if (ImGui::BeginTable("InspectorFields", 2, ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    auto AddRow = [](const char* key, const char* format, ...) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::TextUnformatted(key);
      ImGui::TableNextColumn();
      va_list args; va_start(args, format); ImGui::TextV(format, args); va_end(args);
      };

    AddRow("Address", "0x%p", pBlock);
    AddRow("Tag", "%u (%s)", pBlock->tag, info.label);
    AddRow("Size", "%u bytes", pBlock->size);
    AddRow("Magic", "0x%X %s", pBlock->magic, (pBlock->magic == CHECK_SUM ? "(OK)" : "(CORRUPT)"));
    AddRow("Next", "%u", pBlock->next);
    AddRow("Prev", "%u", pBlock->prev);

    if (pBlock->tag != PU_FREE) {
      if (pBlock->type == TYPE_ZONE) {
        MemZone_t* pSub = (MemZone_t*)GetBlockData(pBlock);
        const char* zName = MEM_STRATEGY(pBlock).GetBlockName(pBlock);
        AddRow("Name",     "%s", zName ? zName : "-");
        AddRow("Used",     "%u bytes", pSub->used);
        AddRow("Capacity", "%u bytes", pSub->capacity);
      } else if (pBlock->type == TYPE_STACK) {
        MemStack_t* pStk = (MemStack_t*)GetBlockData(pBlock);
        const char* sName = MEM_STRATEGY(pBlock).GetBlockName(pBlock);
        AddRow("Name",     "%s", sName ? sName : "-");
        AddRow("Used",     "%u bytes", pStk->used);
        AddRow("Capacity", "%u bytes", pStk->capacity);
      }
    }

    ImGui::EndTable();
  }

}

// Forward declaration — defined after DrawMemoryScaleWidget.
static void DrawZoneBar(MemZone_t* pZone, const char* label, float height);

static void DrawBlockInspectorSubZone() {
  if (gState.selectedBlockAddr == 0) return;
  MemBlock_t* pSel = (MemBlock_t*)gState.selectedBlockAddr;
  if (pSel->tag != PU_FREE && pSel->type == TYPE_ZONE) {
    MemZone_t*  pSub  = (MemZone_t*)GetBlockData(pSel);
    const char* zName = MEM_STRATEGY(pSel).GetBlockName(pSel);
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.6f, 1.f), "Contents of %s", zName ? zName : "?");
    DrawZoneBar(pSub, zName ? zName : "?", 36.0f);
  }
}

/**
 * CalculateActiveBounds
 * Finds the first and last byte offsets that are NOT tagged as PU_FREE.
 */
static void CalculateActiveBounds(MemZone_t* pZone, uint32_t* pOutStart, uint32_t* pOutEnd) {
  uint32_t first = pZone->capacity;
  uint32_t last = 0;
  uint32_t anchor = Z_GetOffset(pZone, &pZone->blocklist);
  uint32_t current = pZone->blocklist.next;

  while (current != anchor) {
    MemBlock_t* pBlock = Z_GetBlock(pZone, current);
    if (pBlock->tag != PU_FREE) {
      if (current < first) first = current;
      if (current + pBlock->size > last) last = current + pBlock->size;
    }
    current = pBlock->next;
  }

  // If no blocks are allocated, default to full range
  if (last == 0) {
    *pOutStart = 0;
    *pOutEnd = pZone->capacity;
  }
  else {
    *pOutStart = first;
    *pOutEnd = last;
  }
}

/**
 * DrawZoneBar
 * Flat, non-zoomable proportional bar for a single zone.
 * Shows all blocks proportionally; click to select.
 */
static void DrawZoneBar(MemZone_t* pZone, const char* label, float height)
{
  if (!pZone || pZone->capacity == 0) return;

  ImGui::Text("%s: %u / %u KB (%.0f%%)", label,
              pZone->used / 1024, pZone->capacity / 1024,
              (float)pZone->used / (float)pZone->capacity * 100.0f);

  ImVec2 pos = ImGui::GetCursorScreenPos();
  float   w   = ImGui::GetContentRegionAvail().x;
  ImDrawList* pDL = ImGui::GetWindowDrawList();
  pDL->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + height), IM_COL32(20, 20, 25, 255));
  pDL->PushClipRect(pos, ImVec2(pos.x + w, pos.y + height), true);

  uint32_t anchor = Z_GetOffset(pZone, &pZone->blocklist);
  uint32_t curr   = pZone->blocklist.next;
  while (curr != anchor) {
    MemBlock_t* pBlock = Z_GetBlock(pZone, curr);
    if (!pBlock) break;
    float bx = pos.x + (float)curr / (float)pZone->capacity * w;
    float bw = (float)pBlock->size / (float)pZone->capacity * w;
    if (bw < 1.0f) bw = 1.0f;

    TagInfo info = GetTagInfo(pBlock->tag);
    pDL->AddRectFilled(ImVec2(bx, pos.y), ImVec2(bx + bw, pos.y + height), info.color);
    pDL->AddRect(ImVec2(bx, pos.y), ImVec2(bx + bw, pos.y + height), IM_COL32(0, 0, 0, 150));

    if ((uintptr_t)pBlock == gState.selectedBlockAddr)
      pDL->AddRect(ImVec2(bx - 1, pos.y - 1), ImVec2(bx + bw + 1, pos.y + height + 1),
                   IM_COL32(255, 255, 255, 255), 0, 0, 1.5f);

    ImGui::SetCursorScreenPos(ImVec2(bx, pos.y));
    char bid[32]; snprintf(bid, sizeof(bid), "##zb_%u", curr);
    if (ImGui::InvisibleButton(bid, ImVec2(bw, height)))
      gState.selectedBlockAddr = (uintptr_t)pBlock;

    curr = pBlock->next;
  }
  pDL->PopClipRect();
  ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + height));
  ImGui::Dummy(ImVec2(w, 6));
}

/**
 * DrawSubZoneMaps
 * For every TYPE_ZONE block in pZone, draws a labeled DrawZoneBar and recurses.
 */
static void DrawSubZoneMaps(MemZone_t* pZone)
{
  uint32_t anchor = Z_GetOffset(pZone, &pZone->blocklist);
  uint32_t curr   = pZone->blocklist.next;
  while (curr != anchor) {
    MemBlock_t* pBlock = Z_GetBlock(pZone, curr);
    if (!pBlock) break;
    if (pBlock->tag != PU_FREE && pBlock->type == TYPE_ZONE) {
      MemZone_t*  pSub  = (MemZone_t*)GetBlockData(pBlock);
      const char* zName = MEM_STRATEGY(pBlock).GetBlockName(pBlock);
      ImGui::Separator();
      DrawZoneBar(pSub, zName ? zName : "?", 40.0f);
      DrawSubZoneMaps(pSub);
    }
    curr = pBlock->next;
  }
}

/**
 * DrawMemoryMap
 * Iterates through the entire zone blocklist and renders the visual representation.
* Renders a zoomable slice of the memory zone.
 */
static void DrawMemoryScaleWidget(MemZone_t* pZone)
{
  // UI Controls for Auto-Zoom
  ImGui::Checkbox("Auto-Zoom", &gState.bAutoZoom);

  if (gState.bAutoZoom) {
    uint32_t activeStart, activeEnd;
    CalculateActiveBounds(pZone, &activeStart, &activeEnd);

    float range = (float)(activeEnd - activeStart);
    float buffer = range * 0.05f; // Add a 5% margin so it doesn't feel cramped

    // 1. Zoom is the ratio of total capacity to the active range
    gState.zoomFactor = (float)pZone->capacity / (range + buffer);
    if (gState.zoomFactor < 1.0f) gState.zoomFactor = 1.0f;

    // 2. Scroll is the normalized start position (minus a bit of buffer)
    gState.scrollOffset = ((float)activeStart - (buffer * 0.5f)) / (float)pZone->capacity;
    if (gState.scrollOffset < 0.0f) gState.scrollOffset = 0.0f;
  }

  // Draw the sliders (disabled if Auto-Zoom is on to show they are being overridden)
  ImGui::BeginDisabled(gState.bAutoZoom);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(200);
  ImGui::SliderFloat("Zoom", &gState.zoomFactor, 1.0f, 10000.0f, "%1.fx");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(200);
  ImGui::SliderFloat("Scroll", &gState.scrollOffset, 0.0f, 1.0f, "");
  ImGui::EndDisabled();
}

/**
 * DrawMemoryMap
 * Iterates through the entire zone blocklist and renders the visual representation.
* Renders a zoomable slice of the memory zone.
 */
static void DrawMemoryMap(MemZone_t* pZone) {
  ImVec2 mapPos = ImGui::GetCursorScreenPos();
  float mapHeight = 60.0f;
  float mapWidth = ImGui::GetContentRegionAvail().x;
  ImDrawList* pDrawList = ImGui::GetWindowDrawList();

  pDrawList->AddRectFilled(mapPos, ImVec2(mapPos.x + mapWidth, mapPos.y + mapHeight), IM_COL32(20, 20, 25, 255));
  pDrawList->PushClipRect(mapPos, ImVec2(mapPos.x + mapWidth, mapPos.y + mapHeight), true);

  uint32_t anchorOffset = Z_GetOffset(pZone, &pZone->blocklist);
  uint32_t current = pZone->blocklist.next;

  while (current != anchorOffset) {
    MemBlock_t* pBlock = Z_GetBlock(pZone, current);

    float normStart = (float)current / (float)pZone->capacity;
    float normWidth = (float)pBlock->size / (float)pZone->capacity;

    float viewX = (normStart - gState.scrollOffset) * gState.zoomFactor * mapWidth;
    float viewW = normWidth * gState.zoomFactor * mapWidth;

    // Render block if it's within the visible viewport
    if (viewX + viewW > 0 && viewX < mapWidth) {
      DrawMemoryBlock(
        pDrawList, pZone, pBlock, current,
        ImVec2(mapPos.x + viewX, mapPos.y), viewW, mapHeight
      );
    }
    current = pBlock->next;
  }
  // 2. Render Rover (Inside Clip Rect, but rendered AFTER blocks to stay on top)
  double normRover = (double)pZone->rover / (double)pZone->capacity;
  float roverX = (float)((normRover - (double)gState.scrollOffset) * (double)gState.zoomFactor * (double)mapWidth);

  if (roverX >= -1.0f && roverX <= mapWidth + 1.0f) {
    float triSize = 12.0f;
    // Shift tip down 15px so it's inside the color bar and not clipped by the top edge
    ImVec2 tip = ImVec2(mapPos.x + roverX, mapPos.y + 15.0f);
    // High-contrast vertical line (Full height)
    pDrawList->AddLine(
      ImVec2(mapPos.x + roverX, mapPos.y),
      ImVec2(mapPos.x + roverX, mapPos.y + mapHeight),
      IM_COL32(255, 0, 255, 255),
      2.0f
    );
    // Magenta Triangle sitting inside the bar
    pDrawList->AddTriangleFilled(
      tip,
      ImVec2(tip.x - triSize, tip.y - 15.0f),
      ImVec2(tip.x + triSize, tip.y - 15.0f),
      IM_COL32(255, 0, 255, 255)
    );

    // Optional: Add a small black outline to the triangle to make it pop
    pDrawList->AddTriangle(
      tip,
      ImVec2(tip.x - triSize, tip.y - 15.0f),
      ImVec2(tip.x + triSize, tip.y - 15.0f),
      IM_COL32(0, 0, 0, 255),
      1.0f
    );
  }
  pDrawList->PopClipRect();

  // Handle Zoom Interaction...
  ImGui::SetCursorScreenPos(mapPos);
  ImGui::InvisibleButton("##MapInteraction", ImVec2(mapWidth, mapHeight));
  if (ImGui::IsItemHovered()) {
    float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0f) {
      gState.zoomFactor += wheel * 0.5f;
      if (gState.zoomFactor < 1.0f) gState.zoomFactor = 1.0f;
    }
  }

  ImGui::Dummy(ImVec2(0, 10));
}

/**
 * DrawBlockActions
 * Buttons that operate on the currently selected block:
 *   Save → serialise to disk | Free → Z_Free | Load → restore from disk.
 * Only shown when a non-free block is selected.
 */
static void DrawBlockActions(MemZone_t* pZone)
{
  if (gState.selectedBlockAddr == 0) return;
  MemBlock_t* pBlock = (MemBlock_t*)gState.selectedBlockAddr;
  if (pBlock->magic != CHECK_SUM) return;

  ImGui::Separator();
  ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.3f, 1.0f), "Block Actions");

  // --- Save / Load filename ---
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 4);
  ImGui::InputText("##savefile", gState.saveFile, sizeof(gState.saveFile));

  bool bIsFree = (pBlock->tag == PU_FREE);

  // Save — only for allocated blocks
  ImGui::BeginDisabled(bIsFree);
  if (ImGui::Button("Save Block", ImVec2(-1, 0))) {
    if (Block_Save(pBlock, gState.saveFile))
      Log_Add("[DISK] Saved block (tag=%u, %u B) → %s", pBlock->tag, pBlock->size, gState.saveFile);
    else
      Log_Add("[ERR]  Block_Save failed.");
  }
  ImGui::EndDisabled();

  // Load — always available (allocates a new block from the zone)
  if (ImGui::Button("Load Block", ImVec2(-1, 0))) {
    Allocator_t alloc      = Zone_GetAllocator(pZone);
    MemBlock_t* pLoaded    = nullptr;
    void*       pData      = Block_Load(&alloc, &pLoaded, gState.saveFile);
    if (pData)
      Log_Add("[DISK] Loaded block (tag=%u) ← %s", pLoaded->tag, gState.saveFile);
    else
      Log_Add("[ERR]  Block_Load failed — file missing or zone full.");
  }

  // Free — only for allocated blocks, red for emphasis
  ImGui::BeginDisabled(bIsFree);
  ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.25f, 0.25f, 1.0f));
  if (ImGui::Button("Free Block", ImVec2(-1, 0))) {
    Log_Add("[STATUS] Freed block tag=%u size=%u", pBlock->tag, pBlock->size);
    Z_Free(pZone, GetBlockData(pBlock));
    gState.selectedBlockAddr = 0; // selection is now invalid
  }
  ImGui::PopStyleColor(2);
  ImGui::EndDisabled();
}

/**
 * DrawZoneActions
 * Controls for whole-zone operations: allocate, purge by tag, clear.
 * Shown between the progress bar and the memory map.
 */
static void DrawZoneActions(MemZone_t* pZone)
{
  ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Zone Actions");
  ImGui::SameLine();

  // Allocate: size + tag inputs then button
  ImGui::SetNextItemWidth(80);
  ImGui::InputInt("##asize", &gState.allocSize, 0);
  ImGui::SameLine();
  ImGui::Text("KB");
  gState.allocSize = gState.allocSize < 1 ? 1 : gState.allocSize;
  ImGui::SameLine();

  const char* kTagNames[] = { "Static(1)", "Level(50)", "Purge(100)", "Cache(101)" };
  const int   kTagVals[]  = { PU_STATIC, PU_LEVEL, PU_CACHE, PU_CACHE };
  int tagIdx = 1;
  for (int i = 0; i < 4; ++i) { if (kTagVals[i] == gState.allocTag) { tagIdx = i; break; } }
  ImGui::SetNextItemWidth(100);
  if (ImGui::BeginCombo("##atag", kTagNames[tagIdx])) {
    for (int i = 0; i < 4; ++i)
      if (ImGui::Selectable(kTagNames[i], tagIdx == i)) gState.allocTag = kTagVals[i];
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  if (ImGui::Button("Alloc")) {
    const size_t kb = 1024;
    void* p = Z_Malloc(pZone, (uint32_t)gState.allocSize * 1024, (uint32_t)gState.allocTag, nullptr);
    if (p) Log_Add("[STATUS] Allocated %d B (tag=%d)", gState.allocSize, gState.allocTag);
    else   Log_Add("[ERR]  Z_Malloc failed — zone full?");
  }
  ImGui::SameLine();

  // Purge all blocks matching the current tag filter
  ImGui::BeginDisabled(gState.activeTagFilter == 0xFFFFFFFF);
  if (ImGui::Button("Purge Tag")) {
    Z_PurgeTag(pZone, gState.activeTagFilter);
    gState.selectedBlockAddr = 0;
    Log_Add("[STATUS] Purged tag %u", gState.activeTagFilter);
  }
  ImGui::EndDisabled();
  ImGui::SameLine();

  // Right-aligned Health Check
  //ImGui::SameLine();
  //ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 150);
  if (ImGui::Button("Run Health Check")) {
    uint32_t freeSize = Z_GetUsedSpace(pZone, PU_FREE);
    uint32_t calculatedTotal = pZone->used + freeSize;
    if (calculatedTotal == pZone->capacity) {
      Log_Add("[PASS] Memory accounting verified.");
    }
    else {
      Log_Add("[ERR] Accounting mismatch: %u expected, %u found.", pZone->capacity, calculatedTotal);
    }
  }
  ImGui::SameLine();

  // Clear entire zone
  ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.6f, 0.2f, 0.1f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.1f, 1.0f));
  if (ImGui::Button("Clear Zone")) {
    Z_Clear(pZone);
    gState.selectedBlockAddr = 0;
    Log_Add("[STATUS] Zone cleared: %s", pZone->name.name);
  }
  ImGui::PopStyleColor(2);
}

/**
 * DrawHierarchyNode
 * Recursively draws one row per block in pZone into an active 2-column table.
 * Column 0: tree node / leaf label.  Column 1: 80px mini usage bar.
 */
static void DrawHierarchyNode(MemZone_t* pZone)
{
  if (!pZone) return;
  uint32_t anchorOff = Z_GetOffset(pZone, &pZone->blocklist);
  uint32_t currOff = pZone->blocklist.next;
  while (currOff != anchorOff) {
    MemBlock_t* pBlock = Z_GetBlock(pZone, currOff);
    if (!pBlock) break;

    ImGui::PushID((int)currOff);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    bool bFree = (pBlock->tag == PU_FREE);
    eMemType_t type = (eMemType_t)pBlock->type;

    // --- Column 0: label / tree node ---
    uint32_t payloadBytes = pBlock->size > kMEM_BLOCK_SIZE
                            ? pBlock->size - kMEM_BLOCK_SIZE : 0;
    float payloadKB = payloadBytes / 1024.0f;

    char label[64];
    if (bFree) {
      snprintf(label, sizeof(label), "[FREE] %.1f KB", payloadKB);
    } else if (type == TYPE_ZONE) {
      const char* zName = MEM_STRATEGY(pBlock).GetBlockName(pBlock);
      snprintf(label, sizeof(label), "[Z] %s %.1f KB", zName ? zName : "?", payloadKB);
    } else if (type == TYPE_STACK) {
      const char* sName = MEM_STRATEGY(pBlock).GetBlockName(pBlock);
      snprintf(label, sizeof(label), "[S] %s %.1f KB", sName ? sName : "?", payloadKB);
    } else {
      TagInfo info = GetTagInfo(pBlock->tag);
      snprintf(label, sizeof(label), "[%s] %.1f KB", info.label, payloadKB);
    }

    bool bSelected = ((uintptr_t)pBlock == gState.selectedBlockAddr);
    bool bOpen = false;

    if (!bFree && type == TYPE_ZONE) {
      ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth
                               | ImGuiTreeNodeFlags_OpenOnArrow
                               | ImGuiTreeNodeFlags_DefaultOpen;
      if (bSelected) flags |= ImGuiTreeNodeFlags_Selected;
      bOpen = ImGui::TreeNodeEx(label, flags);
    } else {
      ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth
                               | ImGuiTreeNodeFlags_Leaf
                               | ImGuiTreeNodeFlags_NoTreePushOnOpen;
      if (bSelected) flags |= ImGuiTreeNodeFlags_Selected;
      ImGui::TreeNodeEx(label, flags);
    }
    if (ImGui::IsItemClicked()) {
      gState.selectedBlockAddr = (uintptr_t)pBlock;
    }

    // --- Column 1: mini usage bar ---
    ImGui::TableNextColumn();
    float fillRatio = 0.0f;
    if (!bFree) {
      if (type == TYPE_ZONE) {
        MemZone_t* pSub = (MemZone_t*)GetBlockData(pBlock);
        if (pSub->capacity > 0)
          fillRatio = (float)pSub->used / (float)pSub->capacity;
      } else if (type == TYPE_STACK) {
        MemStack_t* pStk = (MemStack_t*)GetBlockData(pBlock);
        if (pStk->capacity > 0)
          fillRatio = (float)pStk->used / (float)pStk->capacity;
      } else {
        fillRatio = 1.0f;
      }
    }
    TagInfo barInfo = GetTagInfo(pBlock->tag);
    ImVec2 barMin = ImGui::GetCursorScreenPos();
    float barW = ImGui::GetContentRegionAvail().x;
    float barH = ImGui::GetTextLineHeight();
    ImDrawList* pDL = ImGui::GetWindowDrawList();
    pDL->AddRectFilled(barMin, ImVec2(barMin.x + barW, barMin.y + barH),
                       IM_COL32(50, 50, 55, 255));
    if (fillRatio > 0.0f) {
      pDL->AddRectFilled(barMin,
                         ImVec2(barMin.x + barW * fillRatio, barMin.y + barH),
                         barInfo.color);
    }
    ImGui::Dummy(ImVec2(barW, barH));

    // --- Recurse into sub-zone ---
    if (bOpen) {
      DrawHierarchyNode((MemZone_t*)GetBlockData(pBlock));
      ImGui::TreePop();
    }

    ImGui::PopID();
    currOff = pBlock->next;
  }
}

/**
 * DrawBlockInspector
 * Scene-graph style sidebar showing the block hierarchy of pRootZone.
 */
static void DrawBlockInspector(MemZone_t* pRootZone)
{
  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.9f, 1.0f), "Hierarchy: %s",
                     pRootZone->name.name);
  ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV
                        | ImGuiTableFlags_ScrollY
                        | ImGuiTableFlags_RowBg;
  if (ImGui::BeginTable("##hier", 2, flags)) {
    ImGui::TableSetupScrollFreeze(0, 0);
    ImGui::TableSetupColumn("Block", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("",      ImGuiTableColumnFlags_WidthFixed, 80.0f);
    DrawHierarchyNode(pRootZone);
    ImGui::EndTable();
  }
}

/**
 * GUI_Update
 * Main entry point for the DoomZone Debugger UI.
 */
void GUI_Update(MemZone_t* pZone) {
  if (!pZone || !ImGui::Begin("Zone Debugger")) {
    if (pZone) ImGui::End();
    return;
  }

  DrawFilterToolbar(pZone, &gState);
  ImGui::Separator();

  // Sidebar (hierarchy) + main area side by side
  ImGui::BeginChild("Sidebar", ImVec2(280, 0), true);
  DrawBlockInspector(pZone);
  ImGui::EndChild();

  ImGui::SameLine();
  ImGui::BeginGroup();

  DrawHeaderStats(pZone);
  ImGui::Separator();
  DrawZoneActions(pZone);
  ImGui::Separator();
  DrawMemoryScaleWidget(pZone);
  DrawMemoryMap(pZone);
  DrawSubZoneMaps(pZone);

  float paneWidth = ImGui::GetContentRegionAvail().x * 0.5f;

  ImGui::BeginChild("Inspector", ImVec2(paneWidth - 4, 0), true);
  DrawBlockInspectorTable();
  DrawBlockInspectorSubZone();
  DrawBlockActions(pZone);
  ImGui::EndChild();

  ImGui::SameLine();

  ImGui::BeginChild("LogPane", ImVec2(0, 0), true);
  DrawSystemLog();
  ImGui::EndChild();

  ImGui::EndGroup();
  ImGui::End();
}
