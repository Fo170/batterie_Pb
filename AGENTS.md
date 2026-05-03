# AGENTS.md - batterie_Pb

## Build & Test
- **PlatformIO**: `pio run` (uses `platformio.ini` with `src_dir = .` and C99)
- **GCC**: `gcc -std=c99 -O2 -Wall -o main examples/basic_usage/main.c`

## Key Files
- **Library**: `src/batterie_Pb.h` (header-only, all functions `static inline`)
- **Example**: `examples/basic_usage/main.c`
- **Config**: `platformio.ini`

## C Standard
- Requires `-std=c99` or `-std=c11`

## Project Architecture
- Single-file library (no `.c` to compile/link)
- Supports 4 battery types: Sn-Pb Liquide, Ca-Pb Liquide, AGM, GEL
- Functions: thresholds, currents, temperature compensation, charge state machine