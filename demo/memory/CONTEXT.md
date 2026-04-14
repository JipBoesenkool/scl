# MemoryGui — Demo Context

## What this is
An ImGui + SDL3 real-time debugger for the zone allocator.
Entry point: `main.cpp` → `GUI_Update(pRoot)` each frame.

## File map
| File | Role |
|------|------|
| `main.cpp` | SDL3/ImGui init, zone setup (`InitData`), render loop |
| `MemoryGui.cpp` | All drawing logic |
| `MemoryGui.hpp` | `MemoryGuiState_t` (selected block, filter, alloc inputs) |
| `imgui/Tag.hpp` | `TagInfo` registry, `DrawTagLegend`, `DrawFilterToolbar` |
| `imgui/Log.hpp` | `Log_Add` / `DrawSystemLog` — in-window message log |

## Key drawing functions
| Function | What it draws |
|----------|---------------|
| `DrawMemoryMap` | Iterates the circular block list, one `DrawMemoryBlock` call per block |
| `DrawMemoryBlock` | Header strip (gray) + data strip (tag color), hover tooltip, rover marker |
| `DrawHeaderStats` | Zone name, free/purgeable KB, usage progress bar, health check button |
| `DrawZoneActions` | Alloc (size + tag), Purge Tag, Clear Zone buttons |
| `DrawBlockActions` | Save/Load/Free for the selected block |
| `DrawBlockInspectorTable` | Raw field dump for the selected block |

## Block geometry
```
normalizedX = (blockOffset  / zone->capacity) * mapWidth
normalizedW = (block->size  / zone->capacity) * mapWidth
```
Minimum rendered width is **8 px** — below that, sub-kilobyte blocks in a MB-scale zone
would be invisible. This means tiny blocks render slightly wider than their true proportion;
that's intentional.

## Known bugs fixed
- **2026-04-09** — Newly allocated blocks were invisible.
  Cause: old minimum width was 1 px, so anything < ~8 KB in a 1 MB zone disappeared.
  Fix: raised minimum to 8 px in `DrawMemoryMap`.

## `MemoryGuiState_t` fields
| Field | Default | Purpose |
|-------|---------|---------|
| `selectedBlockAddr` | 0 | Raw address of the clicked `MemBlock_t*` |
| `activeTagFilter` | `PU_ALL (0xFFFFFFFF)` | Dims non-matching blocks to 20% alpha |
| `saveFile[64]` | `"block_dump.bin"` | Path used by Save/Load block actions |
| `allocSize` | 1024 | Bytes requested by the Alloc button |
| `allocTag` | 50 (`PU_LEVEL`) | Tag used by the Alloc button |

## Tag color registry (`Tag.hpp`)
| Tag | Color |
|-----|-------|
| `PU_FREE` | Dark gray `(45,45,50)` |
| `PU_STATIC` | Green `(46,204,113)` |
| `PU_LEVEL` | Blue `(52,152,219)` |
| `PU_PURGE` | Yellow `(241,196,15)` |
| `PU_CACHE` | Orange `(230,126,34)` |
| Unknown | Blue (same as Level) |

## Build
```
cmake -B build -DBUILD_DEMO=ON && cmake --build build
./build/demo/ZoneDebugger
```
The demo target is gated behind `BUILD_DEMO` in the root `CMakeLists.txt`.
