# WebLibMKF - MKF WebAssembly Build

## Project Overview
Compiles the MKF C++ library to WebAssembly for use in web browsers.

## Architecture
- **Build System:** CMake + Ninja
- **Compiler:** Emscripten (emcc)
- **Output:** JavaScript + WASM

## Key Files
- `src/libMKF.cpp` - Main wrapper exposing MKF functions to JS
- `src/libCrossReferencers.cpp` - Cross-referencing functions
- `src/libInsulationCoordinator.cpp` - Insulation calculations

## Outputs
Built files are in `build/`:
- `libMKF.wasm.js` - JavaScript wrapper
- `libMKF.wasm.wasm` - WebAssembly binary
- `libCrossReferencers.wasm.js/.wasm` - Cross-referencer WASM
- `libInsulationCoordinator.wasm.js/.wasm` - Insulation WASM

## Building

### Prerequisites
- Emscripten SDK installed
- CMake
- Ninja

### Build Commands
```bash
cd build

# Activate Emscripten environment
source ~/emsdk/emsdk_env.sh

# Build MKF WASM (all modules)
ninja -j8

# Or build specific modules
ninja libMKF -j8
ninja libCrossReferencers -j8
ninja libInsulationCoordinator -j8
```

### Copy to WebFrontend
After building, copy the WASM files to WebFrontend:
```bash
cp build/libMKF.wasm.js build/libMKF.wasm.wasm \
   ../WebFrontend/src/assets/js/
```

### Complete Workflow (MKF Change → Frontend)
When modifying MKF source files and updating the frontend:

```bash
# 1. Edit MKF source (e.g., StrayCapacitance.cpp)
cd /home/alf/OpenMagnetics/MKF/src/physical_models
# Make your changes...

# 2. Copy updated file to WebLibMKF deps
cp StrayCapacitance.cpp \
   /home/alf/OpenMagnetics/WebLibMKF/build/_deps/mkf-src/src/physical_models/

# If header files changed, copy those too:
# cp StrayCapacitance.h \
#    /home/alf/OpenMagnetics/WebLibMKF/build/_deps/mkf-src/src/physical_models/

# 3. Build WebLibMKF
cd /home/alf/OpenMagnetics/WebLibMKF/build
source ~/emsdk/emsdk_env.sh
ninja -j8

# 4. Copy to WebFrontend
cp libMKF.wasm.js libMKF.wasm.wasm \
   ../WebFrontend/src/assets/js/

# 5. WebFrontend will automatically reload with new WASM
```

## How It Works

### MKF Dependency
WebLibMKF fetches MKF as a CMake dependency (from OpenMagnetics/MKF repo).
The MKF source is in `build/_deps/mkf-src/`.

**Important:** When modifying MKF:
1. Edit files in `MKF/src/`
2. Copy to WebLibMKF deps: `cp MKF/src/... WebLibMKF/build/_deps/mkf-src/src/...`
3. Rebuild: `ninja libMKF`
4. Copy outputs to WebFrontend

### WASM Function Exposure
Functions are exposed via Embind in `src/libMKF.cpp`:
```cpp
EMSCRIPTEN_BINDINGS(libMKF) {
    function("calculate_flyback_inputs", &calculate_flyback_inputs);
    function("simulate_flyback_ideal_waveforms", &simulate_flyback_ideal_waveforms);
    // ... etc
}
```

### Worker Mode
WASM can run in:
1. **Main thread** - Direct module import (blocking)
2. **Web Worker** - Via `mkfWorker.js` (non-blocking, recommended)

## Debugging WASM

### Enable Debug Mode
```bash
emcc ... -g4 -s ASSERTIONS=1 ...
```

### Check Console
Browser console shows:
- Module loading status
- Memory allocation errors
- Function call errors

### Common Issues
- **"memory access out of bounds"** - Stack overflow, increase stack size
- **"function signature mismatch"** - Check Embind type declarations
- **Slow performance** - Ensure worker mode is enabled

## Testing WASM
After building, test in WebFrontend:
1. Copy WASM files to WebFrontend
2. Start WebFrontend dev server
3. Open browser console to see MKF initialization
4. Run a wizard to test calculations

## Build Configuration
Key CMake options in `build/CMakeCache.txt`:
- Emscripten-specific flags
- Memory settings
- Export configurations

## Updates
To update MKF:
1. Commit changes to MKF repo
2. In WebLibMKF: `rm -rf build/_deps/mkf-*`
3. Rebuild: `ninja libMKF` (fetches latest MKF)

Or manually copy files as described above for faster iteration.

## Specific File Workflows

### Modifying Coil.cpp
When modifying the coil winding/compaction logic (e.g., `delimit_and_compact_round_window()`, `wind_toroidal_additional_turns()`):

```bash
# 1. Edit the file in MKF repo
cd /home/alf/OpenMagnetics/MKF/src/constructive_models
# Make changes to Coil.cpp...

# 2. Copy to WebLibMKF deps
cp Coil.cpp /home/alf/OpenMagnetics/WebLibMKF/build/_deps/mkf-src/src/constructive_models/

# 3. Build WebLibMKF
cd /home/alf/OpenMagnetics/WebLibMKF/build
ninja -j8

# 4. Copy WASM to WebFrontend (both locations)
cp libMKF.wasm.wasm libMKF.wasm.js \
   /home/alf/OpenMagnetics/WebFrontend/src/assets/js/
cp libMKF.wasm.wasm libMKF.wasm.js \
   /home/alf/OpenMagnetics/WebFrontend/MagneticBuilder/src/assets/js/

# 5. Test the changes in the frontend
```

**Note:** `Coil.cpp` contains the toroidal coil redistribution logic. Changes to `wind_toroidal_additional_turns()` or `delimit_and_compact_round_window()` affect how turns are packed into layers.
**Related:** See `/home/alf/OpenMagnetics/MKF/AGENTS.md` for MKF-specific workflows.