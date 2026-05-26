# VirtuaPPU

`VirtuaPPU` is exposed as a small C runtime that can be embedded in a C engine.

Public API:
- `include/virtuappu.h`
- `include/ppu_memory.h`
- `include/modes_impl.h`

Build:
- `xmake` builds a static library named `VirtuaPPU`
- the submodule is C-only (`c17`)

Notes:
- Mode 0 uses the shared `virtuappu_vram` buffer.
- Modes 1 and 2 expose `virtuappu_mode1_bind_gba_memory()`
- Mode 7 reads from the shared `virtuappu_vram` buffer.
