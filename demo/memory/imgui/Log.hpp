#ifndef LOG_GUI_HPP
#define LOG_GUI_HPP

#include "imgui.h"
#include <cLib/Log.h>

static void DrawSystemLog() {
  ImGui::Text("System History:");
  ImGui::BeginChild("LogRegion", ImVec2(0, 0), true);
  for (uint32_t i = 0; i < Log_GetCount(); i++) {
    const char* pLine = Log_GetLine(i);
    if (!pLine) continue;

    if (strstr(pLine, "[ERR]"))       ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1), "%s", pLine);
    else if (strstr(pLine, "[DISK]")) ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1), "%s", pLine);
    else if (strstr(pLine, "[INIT]")) ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1), "%s", pLine);
    else                              ImGui::TextUnformatted(pLine);
  }
  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
  ImGui::EndChild();
}

#endif
