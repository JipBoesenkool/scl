#ifndef TAG_IMGUI_HPP
#define TAG_IMGUI_HPP

#include "imgui.h"

#include <stdint.h>
#include <cLib/memory/zone/Zone.h>
#include <cLib/memory/zone/ZoneStats.h>

#define PU_ALL 0xFFFFFFFF
struct TagInfo { 
  uint32_t value; 
  const char* label; 
  ImU32 color;
};
/**
 * GetTagInfo
 * Centralized registry for all memory tags.
 */
static TagInfo GetTagInfo(uint32_t tag) {
  if (tag == PU_FREE)    return { PU_FREE,    "Free",      IM_COL32(45, 45, 50, 255) };
  if (tag == PU_STATIC)  return { PU_STATIC,  "Static",    IM_COL32(46, 204, 113, 255) };
  if (tag == PU_LEVEL)   return { PU_LEVEL,   "Level",     IM_COL32(52, 152, 219, 255) };
  if (tag == PU_CACHE)   return { PU_CACHE,   "Purgeable", IM_COL32(241, 196, 15, 255) };
  if (tag == PU_CACHE)   return { PU_CACHE,   "Cache",     IM_COL32(230, 126, 34, 255) };
  // Default for unknown or standard tags
  return { tag, "Standard", IM_COL32(52, 152, 219, 255) };
}
/**
 * DrawTagLegend
 * Renders a row of color-coded buttons to identify memory tags.
 */
static void DrawTagLegend() {
  uint32_t displayTags[] = { PU_FREE, PU_STATIC, PU_LEVEL, PU_CACHE };
  for (uint32_t t : displayTags) {
    TagInfo info = GetTagInfo(t);
    ImGui::ColorButton(info.label, ImColor(info.color), ImGuiColorEditFlags_NoTooltip);
    ImGui::SameLine(); ImGui::TextUnformatted(info.label); ImGui::SameLine(0, 20);
  }
  ImGui::NewLine();
}

static void DrawFilterToolbar(MemZone_t* pZone, MemoryGuiState_t* pState) {
  uint32_t filterList[] = { PU_ALL, PU_FREE, PU_STATIC, PU_LEVEL, PU_CACHE };
  const char* activeLabel = (pState->activeTagFilter == PU_ALL) ? "Show All" : GetTagInfo(pState->activeTagFilter).label;

  ImGui::SetNextItemWidth(120);
  if (ImGui::BeginCombo("Filter", activeLabel)) {
    for (uint32_t val : filterList) {
      if (ImGui::Selectable(val == PU_ALL ? "Show All" : GetTagInfo(val).label, pState->activeTagFilter == val))
        pState->activeTagFilter = val;
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  DrawTagLegend();
  if (pState->activeTagFilter != PU_ALL) {
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 150);
    ImGui::TextColored({1,1,0,1}, "Footprint: %u B", Z_GetUsedSpace(pZone, pState->activeTagFilter));
  }
}

#endif // TAG_IMGUI_HPP
