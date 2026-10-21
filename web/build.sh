#!/bin/sh
#  Builds the engine to a standalone wasm module — no JS glue, no JUCE.
#  The page and the audio worklet instantiate it themselves.
set -e
cd "$(dirname "$0")"

export PATH="$HOME/emsdk/py/bin:$HOME/emsdk/upstream/emscripten:$PATH"

EXPORTS=_qd_init,_qd_buffer_l,_qd_buffer_r,_qd_process,_qd_transport,_qd_preset,\
_qd_preset_count,_qd_note,_qd_panic,_qd_set_knob,_qd_set_option,_qd_set_drawer,\
_qd_add,_qd_remove,_qd_move,_qd_connect,_qd_disconnect,_qd_state,_qd_specs,\
_qd_clear,_qd_roll,_qd_set_depth,_qd_set_step,_qd_set_mode,_qd_block_preset

em++ -O3 -std=c++17 -DTEST -fno-exceptions -fno-rtti \
     -I../Source -I../third_party/mi \
     bridge.cpp ../Source/Blocks.cpp ../Source/MIBlocks.cpp ../Source/Presets.cpp \
     $(find ../third_party/mi -name '*.cc' | grep -v -E 'test|example') \
     -sSTANDALONE_WASM=1 --no-entry \
     -sINITIAL_MEMORY=134217728 -sALLOW_MEMORY_GROWTH=0 \
     -sEXPORTED_FUNCTIONS=$EXPORTS \
     -o quadrat.wasm

ls -lh quadrat.wasm
