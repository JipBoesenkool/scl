#include "SDL3/SDL.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include <cLib/Log.h>
#include <cLib/memory/zone/Zone.h>
#include <cLib/memory/zone/MemBlock.h>
#include <cLib/memory/strategy/MemStrategy.h>
#include "imgui/MemoryGui.hpp"
#include <stdlib.h>

#include <cLib/memory/Memory.h>

/**
 * InitData
 * Populates the zone with a mix of static and purgeable blocks 
 * to demonstrate the visual debugger's color coding.
 */
void InitData(MemZone_t* pZone) {
  void* pLevelData = NULL;
  void* pTexCache = NULL;
  void* pSoundBuffer = NULL;
  void* pSystemModule = NULL;

  // 1. Allocate a 'Static' block (Green)
  Z_Malloc(pZone, 64 * 1024, 10, &pLevelData);
  
  // 2. Allocate 'Purgeable' cache blocks (Yellow)
  Z_Malloc(pZone, 128 * 1024, PU_CACHE, &pTexCache);
  Z_Malloc(pZone, 256 * 1024, PU_CACHE, &pSoundBuffer);
  
  // 3. Allocate and then Free a block to create a "hole" (Grey)
  Z_Malloc(pZone, 32 * 1024, 5, &pSystemModule);
  Z_Free(pZone, pSystemModule);

  // 4. Nested RENDER sub-zone
  MemZone_t* pRender = NULL;
  void* pRenderMem = Z_Malloc(pZone, 512 * 1024, PU_STATIC, &pRender);
  if (pRenderMem) {
    MemBlock_t* pBlk = GetBlockHeader(pRenderMem);
    pBlk->type = TYPE_ZONE;
    MEM_STRATEGY(pBlk).Init(pBlk, 0, PU_STATIC, "RENDER");
    Z_Malloc(pRender, 64  * 1024, PU_STATIC, nullptr);
    Z_Malloc(pRender, 32  * 1024, PU_CACHE,  nullptr);
  }
}

int main(int argc, char* argv[]) {
  Log_Init();

  // 1. Memory Setup
  uint32_t heapSize = GB; // 1MB
  void* pHeapMemory = malloc(heapSize);
  
  // Initialize the zone within the allocated buffer
  MemZone_t* pRoot = Z_Init((MemZone_t*)pHeapMemory, heapSize, PU_STATIC, "MASTER_HEAP");
  InitData(pRoot);
  // 2. SDL3 + ImGui Initialization
  if (SDL_Init(SDL_INIT_VIDEO) == false) {
    return -1;
  }
  SDL_Window* pWindow = SDL_CreateWindow("DoomZone Debugger", 1280, 720, SDL_WINDOW_RESIZABLE);
  SDL_Renderer* pRenderer = SDL_CreateRenderer(pWindow, NULL);
  
  ImGui::CreateContext();
  ImGui_ImplSDL3_InitForSDLRenderer(pWindow, pRenderer);
  ImGui_ImplSDLRenderer3_Init(pRenderer);

  // 3. Main Loop
  bool bRunning = true;
  while (bRunning) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        bRunning = false;
      }
    }
    // New Frame
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    // Render the Debugger UI
    GUI_Update(pRoot);
    // Draw to Screen
    ImGui::Render();
    SDL_SetRenderDrawColor(pRenderer, 20, 20, 25, 255);
    SDL_RenderClear(pRenderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), pRenderer);
    SDL_RenderPresent(pRenderer);
  }
  // 4. Cleanup & Shutdown
  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  SDL_DestroyRenderer(pRenderer);
  SDL_DestroyWindow(pWindow);
  SDL_Quit();
  Z_Clear(pRoot);
  // Release the entire memory heap
  free(pHeapMemory);
  return 0;
}
