# Makefile for driving CMake and other tools.
# In windows it requires mingw32-make.exe.
# >make help // to see the available commands and options

# Use this character to start a rule recipe
.RECIPEPREFIX = >
CLEANFILES=model

# Sets compiler option
CC=-DCMAKE_C_COMPILER=gcc
ifdef gcc
	CC=-DCMAKE_C_COMPILER=gcc 
endif
ifdef clang
	CC=-DCMAKE_C_COMPILER=clang 
endif

# Sets the build type option
BUILD_TYPE=-DCMAKE_BUILD_TYPE=Debug
ifdef debug
	BUILD_TYPE=-DCMAKE_BUILD_TYPE=Debug
endif
ifdef release
	BUILD_TYPE=-DCMAKE_BUILD_TYPE=Release
	ifdef debinfo
		BUILD_TYPE=-DCMAKE_BUILD_TYPE=Relwithdebinfo
	endif
endif

# Sets address sanitizer option
ifdef asan 
	ASAN=-DCMAKE_C_FLAGS="-fsanitize=address,leak"
endif

# Sets memory sanitizer options
ifdef msan 
	MSAN=-DCMAKE_C_FLAGS="-fsanitize=memory -fsanitize-memory-track-origins=2"
endif

# Sets thread sanitizer option
ifdef tsan
	TSAN=-DCMAKE_C_FLAGS="-fsanitize=thread"
endif

# Sets profiling optionProfiling with gprof
ifdef gprof
	GPROF=-DCMAKE_C_FLAGS="-pg"
endif

# Sets clang xray tracer 
ifdef xray
	ifndef clang
		$(error xray is supported only by clang)
	endif
	XRAY=-DCMAKE_C_FLAGS="-fxray-instrument"
endif

# Build system generator
GEN=-G "Unix Makefiles"
#GEN=-G "Ninja"
ifdef ninja
	GEN=-G "Ninja"
endif

# Build parallel jobs
jobs=$(shell nproc)
JOBS=-j$(jobs)

# Current build options
BUILD_OPTIONS=$(GEN) $(CC) $(BUILD_TYPE) $(ASAN) $(MSAN) $(TSAN) $(XRAY) $(GPROF)

#
# Default targets
#
.PHONY: all
all: deftarget

#
# CMake build target (default target)
# 
deftarget: build
> cmake --build build -- $(JOBS) 

#
# CMake configuration target
#
build: 
> cmake $(GEN) -Bbuild $(BUILD_OPTIONS) -DCPM_SOURCE_CACHE=deps_cache
> echo "$(BUILD_OPTIONS)" > build_options

#
# Forces a new CMake configuration
#
.PHONY: config
config:
> cmake $(GEN) -Bbuild $(BUILD_OPTIONS)
> echo "$(BUILD_OPTIONS)" > build_options

# Run executable with xray tracing enabled.
# The executable must have been built with xray=1
# Will produce an event log file.
xray-run:
> cmake -E env XRAY_OPTIONS="patch_premain=true xray_mode=xray-basic verbose=1" $(exec)

#
# Converts xray log to google chrome tracing format
#
xray-evt:
> llvm-xray convert -output-format=trace_event -output=$(xlog).evt -symbolize -sort -instr_map=$(exec) $(xlog)

#
# Clean build artifacts
#
.PHONY: clean
clean:
> cmake -E remove_directory -f build
> cmake -E remove -f build_options
> cmake -E remove $(CLEANFILES)
> cmake -E remove gmon.out
> cmake -E remove xray-log*

.PHONY: clean_all
clean_all: clean
> cmake -E remove_directory -f deps_cache

#
# Preprocess and formats 'main.c'
#
.PHONY: pp
pp:
> clang -P -E -I ../include main.c | clang-format

#
# Debug target
#
.PHONY: d
d: all
> gdb ./tests

#
# Run target
#
.PHONY: r
r: all
> ./tests

#
# Show help target
#
NULL:=
TAB:=$(NULL)	$(NULL)
help:
> $(info ----------------------------------------------)
> $(info Configure and build project:)
> $(info >make <option> ... <option>)
> $(info options:)
> $(info $(TAB)ninja=1		use ninja (default=makefiles))
> $(info $(TAB)jobs=n		number of jobs (default=nproc))
> $(info $(TAB)clang=1		use clang compiler (default=gcc))
> $(info $(TAB)release=1	release build (default=debug))
> $(info $(TAB)debinfo=1	generates debug info)
> $(info $(TAB)gprof=1		generates profiler info)
> $(info $(TAB)asan=1		use address sanitizer)
> $(info $(TAB)msan=1		use memory sanitizer)
> $(info $(TAB)tsan=1		use thread sanitizer)
> $(info $(TAB)xray=1		enable clang xray tracer)
> $(info ----------------------------------------------)
> $(info Run executable generating xray tracer log:)
> $(info >make xray-run exec=<"executable [args]">)
> $(info ----------------------------------------------)
> $(info Converts xray log to google-chrome tracing format (json))
> $(info >make xray-evt exec="executable" xlog=<xray log file>)
> $(info ----------------------------------------------)
> $(info Remove build artifacts:)
> $(info >make clean)
> $(info ----------------------------------------------)
> $(info Remove project build directory:)
> $(info >make clean_all)
> $(info ----------------------------------------------)

#
# Show variables for testing
#
vars:
> $(info "BUILD_OPTIONS:  $(BUILD_OPTIONS)")
> $(info "GEN:        $(GEN)")
> $(info "JOBS      : $(JOBS)")
> $(info "CC:         $(CC)")
> $(info "BUILD_TYPE: $(BUILD_TYPE)")
> $(info "ASAN      : $(ASAN)")
> $(info "MSAN      : $(MSAN)")
> $(info "TSAN      : $(TSAN)")
> $(info "GPROF     : $(GPROF)")

