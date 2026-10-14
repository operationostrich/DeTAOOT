# Titanic: Adventure Out of Time -- MinGW-w64 32-bit build of the playable port.
#
# This is the main build. It needs only a 32-bit MinGW GCC; none of match/ (the optional
# byte-identical build with Visual C++ 4.2) is involved.
#
# Prerequisites (MSYS2):
#   pacman -S mingw-w64-i686-gcc mingw-w64-i686-make
#   export PATH="/mingw32/bin:$PATH"
# Python 3 is also needed (res/append_typed_res.py builds the resource file).
#
# Targets:
#   make / make release -- optimized build:  build/TI.EXE
#   make debug          -- symbolized, unoptimized build: build/TI_dbg.EXE
#   make trace          -- debug build + DF_TRACE logging: build/TI_trace.EXE
#                          (see include/df_trace.h; log path via $DF_TRACE_LOG)
#   make nofix          -- compile check with every DF_FIX_* switch at 0, i.e. the
#                          decoded original with no modern-OS fixes. Not expected
#                          to run. See include/df_fixes.h.
#
# Turn one fix off in a normal build:
#   make debug EXTRA_DEFS=-DDF_FIX_CLUT_FADE_REBLIT=0   (after a `make clean`)
#
# Both configurations build with -fno-strict-aliasing. The code was recovered from a
# disassembly, which punts between int / byte* / code* constantly; that is undefined
# behaviour under strict aliasing and the leading suspect for optimized-build-only crashes.

CC      = i686-w64-mingw32-gcc
WINDRES = windres
MAKE    ?= mingw32-make
PYTHON  ?= python

INCLUDES = -Iinclude -Isrc
WARN_FLAGS = -Wall -Wno-unused-variable -Wno-unused-function \
             -Wno-implicit-function-declaration -Wno-incompatible-pointer-types \
             -Wno-int-conversion -Wundef
DEFS     = -DWIN32 -D_WIN32 -D_WINDOWS -mwindows \
           -include include/df_types.h -include include/df_globals.h \
           -include include/df_trace.h -include include/df_fixes.h $(EXTRA_DEFS)

BASE_CFLAGS    = -std=gnu99 -fno-strict-aliasing $(WARN_FLAGS) $(DEFS)
RELEASE_CFLAGS = $(BASE_CFLAGS) -O1
DEBUG_CFLAGS   = $(BASE_CFLAGS) -O0 -g3 -fno-omit-frame-pointer
# -fstack-protector-all is deliberate. The dominant bug class in this port is a decompiler
# rendering one contiguous record as several adjacent MSVC locals: the original copies the whole
# record over the first one, which silently overruns the frame under GCC. Those corrupt the
# return address and crash somewhere unrelated with no usable backtrace. With the protector the
# trace build stops in __stack_chk_fail *in the offending function*. Break on
# __stack_chk_fail under gdb and read frame #1.
# -finstrument-functions gives the trace build a function-entry hook (__cyg_profile_func_enter
# in src/port/trace.c) that records which functions ever ran; it is flushed to df_coverage.txt
# next to $DF_TRACE_LOG on normal exit, on FatalError, and every 2000 new entries so a segfault
# still leaves a file.
TRACE_CFLAGS   = $(DEBUG_CFLAGS) -DDF_TRACE -fstack-protector-all -finstrument-functions
# Every DF_FIX_* at 0. Keeps the original-side (#else) code compiling so it cannot rot
# unnoticed. -Werror=undef: a misspelt switch is an error here.
NOFIX_CFLAGS   = $(DEBUG_CFLAGS) -DDF_FIXES_OFF -Werror=undef

RELEASE_LDFLAGS = -mwindows -static-libgcc -Wl,--stack,8388608 -Wl,--enable-stdcall-fixup
DEBUG_LDFLAGS   = -mwindows -g3 -Wl,--stack,8388608 -Wl,--enable-stdcall-fixup
TRACE_LDFLAGS   = $(DEBUG_LDFLAGS) -fstack-protector-all
LDLIBS          = -lkernel32 -luser32 -lgdi32 -lwinmm -lcomdlg32

# src/<subsystem>/*.c: the game code, src/data (globals, keyword tables), src/port (trace,
# CRT shims). The link order of the original exe is src/units.txt; it matters only to match/.
SRCS = $(wildcard src/*/*.c)

RELEASE_TARGET = build/TI.EXE
DEBUG_TARGET   = build/TI_dbg.EXE
TRACE_TARGET   = build/TI_trace.EXE
NOFIX_TARGET   = build/TI_nofix.EXE

RELEASE_OBJS = $(patsubst src/%.c,build/obj/release/%.o,$(SRCS)) build/obj/release/titanic_res.o
DEBUG_OBJS   = $(patsubst src/%.c,build/obj/debug/%.o,$(SRCS)) build/obj/debug/titanic_res.o
TRACE_OBJS   = $(patsubst src/%.c,build/obj/trace/%.o,$(SRCS)) build/obj/trace/titanic_res.o
NOFIX_OBJS   = $(patsubst src/%.c,build/obj/nofix/%.o,$(SRCS)) build/obj/debug/titanic_res.o

.PHONY: all release debug trace nofix clean

all: release

release: $(RELEASE_TARGET)

debug: $(DEBUG_TARGET)

# Same as `debug` but with DF_TRACE on: every DF_TRACE() point in the source becomes a real log
# line (see include/df_trace.h). Separate object dir so switching between debug and trace builds
# never links a stale mix of the two.
trace: $(TRACE_TARGET)

nofix: $(NOFIX_TARGET)

$(RELEASE_TARGET): $(RELEASE_OBJS)
	$(CC) $(RELEASE_LDFLAGS) -o $@ $(RELEASE_OBJS) $(LDLIBS)

$(DEBUG_TARGET): $(DEBUG_OBJS)
	$(CC) $(DEBUG_LDFLAGS) -o $@ $(DEBUG_OBJS) $(LDLIBS)

$(TRACE_TARGET): $(TRACE_OBJS)
	$(CC) $(TRACE_LDFLAGS) -o $@ $(TRACE_OBJS) $(LDLIBS)

$(NOFIX_TARGET): $(NOFIX_OBJS)
	$(CC) $(DEBUG_LDFLAGS) -o $@ $(NOFIX_OBJS) $(LDLIBS)

# include/df_types.h, df_globals.h, df_trace.h and df_fixes.h are force-included via -include
# above rather than #include, so Make's automatic dependency scanning cannot see that every
# object file depends on them. Listed explicitly so that changing a global's type correctly
# triggers a full rebuild instead of silently linking stale objects against the old declarations.
FORCE_HEADERS = include/df_types.h include/df_globals.h include/df_trace.h include/df_fixes.h

build/obj/release/%.o: src/%.c $(FORCE_HEADERS)
	@mkdir -p $(@D)
	$(CC) $(RELEASE_CFLAGS) $(INCLUDES) -c -o $@ $<

build/obj/debug/%.o: src/%.c $(FORCE_HEADERS)
	@mkdir -p $(@D)
	$(CC) $(DEBUG_CFLAGS) $(INCLUDES) -c -o $@ $<

build/obj/trace/%.o: src/%.c $(FORCE_HEADERS)
	@mkdir -p $(@D)
	$(CC) $(TRACE_CFLAGS) $(INCLUDES) -c -o $@ $<

build/obj/nofix/%.o: src/%.c $(FORCE_HEADERS)
	@mkdir -p $(@D)
	$(CC) $(NOFIX_CFLAGS) $(INCLUDES) -c -o $@ $<

# Resources. res/titanic.rc holds the STRINGTABLE and RAW blocks, extracted from the original
# exe. The icons, menu, dialogs and accelerators (res/typed/, the exe's own bytes) are appended
# to the compiled .res by res/append_typed_res.py: no resource compiler accepts raw bytes for
# those predefined types.
build/obj/%/titanic_res.o: res/titanic.rc $(wildcard res/typed/*.bin) res/append_typed_res.py
	@mkdir -p $(@D)
	$(WINDRES) -O res -o $(@:.o=.base.res) $<
	$(PYTHON) res/append_typed_res.py $(@:.o=.base.res) $(@:.o=.res)
	$(WINDRES) -J res -i $(@:.o=.res) -O coff -o $@

clean:
	rm -rf build
