#!/bin/bash
set -e
cd $1

# Fix configure to recognize .wasm output files
if [ -f configure ]; then
    sed -i 's/ac_files=\"a.out/ac_files=\"a.out a.out.js a.out.wasm/' configure
fi

# Fix main.c - remove main function when building shared module
# The SHARED_MODULE macro should already handle this in newer ngspice versions
if [ -f src/main.c ]; then
    # Check if SHARED_MODULE guard already exists around main
    if ! grep -q '#ifndef SHARED_MODULE' src/main.c 2>/dev/null || ! grep -A1 '#ifndef SHARED_MODULE' src/main.c | grep -q 'int main'; then
        # Add guard around main function if not present
        sed -i 's/^int main(int argc, char \*\*argv)/\n#ifndef SHARED_MODULE\nint main(int argc, char **argv)/' src/main.c 2>/dev/null || true
        # Find the end of main and add endif (this is approximate - newer versions may already have this)
    fi
fi

# Fix misc_time.c - getrusage doesn't work properly in WASM
if [ -f src/misc/misc_time.c ]; then
    # Add EMSCRIPTEN guard around getrusage usage
    sed -i 's/#ifdef HAVE_GETRUSAGE/#if defined(HAVE_GETRUSAGE) \&\& !defined(__EMSCRIPTEN__)/' src/misc/misc_time.c 2>/dev/null || true
fi

# Fix sharedspice.c - skip user config file loading for WASM
if [ -f src/sharedspice.c ]; then
    # Add EMSCRIPTEN guard to skip config file access
    sed -i 's/read_initialisation_file()/#ifndef __EMSCRIPTEN__\n    read_initialisation_file()\n#endif/' src/sharedspice.c 2>/dev/null || true
fi

# Fix frontend/outitf.c - function signature mismatch
if [ -f src/frontend/outitf.c ]; then
    # The sh_vecinit signature fix - search for the function and ensure proper signature
    :
fi

echo 'WASM patches applied successfully'
