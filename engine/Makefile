# ---------------------------------------------------------------------------
# Zyphie compression engine
#
#   Windows (MinGW):      mingw32-make        <- plain `make` is not installed
#   Linux / CI / Docker:  make
#
# Each profile keeps its objects in its own tree, so switching profiles can
# never link objects that were compiled with different flags.
#
#   make            dev profile      -O2 -g
#   make release    shipping build   -O3 -DNDEBUG, stripped
#   make debug      ASan + UBSan     Linux only; MinGW ships no sanitizer runtime
#   make test       build and run the test suite
#   make clean
# ---------------------------------------------------------------------------

# `CC ?= gcc` would not work here: make predefines CC = cc as a built-in, so
# ?= never fires, and MinGW has no `cc`. Overriding only when the value came
# from make's own defaults leaves a caller-supplied CC=clang intact.
ifeq ($(origin CC),default)
  CC := gcc
endif

SRC_DIR  := src
INC_DIR  := include
TEST_DIR := tests

PROFILE   ?= dev
BUILD_DIR := build/$(PROFILE)

# MinGW's gcc appends .exe when -o has no suffix. Without matching that here,
# make would never see its target appear and would relink on every run.
ifeq ($(OS),Windows_NT)
  EXE := .exe
else
  EXE :=
endif

# -Wconversion is deliberately absent. It is genuinely useful against the
# bit-packing code, but far too noisy to combine with the -Werror CI uses.
# Run it by hand as an audit instead:  make CFLAGS_EXTRA=-Wconversion
WARNINGS := -Wall -Wextra -Wshadow -Wstrict-prototypes -Wmissing-prototypes \
            -Wold-style-definition -Wpointer-arith -Wwrite-strings \
            -Wredundant-decls -Wvla

CPPFLAGS += -I$(INC_DIR)
CFLAGS   += -std=c11 $(WARNINGS) -MMD -MP $(CFLAGS_EXTRA)

ifeq ($(PROFILE),dev)
  CFLAGS += -O2 -g
endif
ifeq ($(PROFILE),release)
  CFLAGS += -O3 -DNDEBUG
endif
ifeq ($(PROFILE),debug)
  CFLAGS  += -O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer
  LDFLAGS += -fsanitize=address,undefined
endif

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS    := $(OBJECTS:.o=.d)

TARGET := $(BUILD_DIR)/zyphie$(EXE)

.PHONY: all release debug test clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
ifeq ($(PROFILE),release)
	strip $@
endif

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

release:
	@$(MAKE) PROFILE=release all

debug:
	@$(MAKE) PROFILE=debug all

# Wired up in Phase 2, once tests/ exists.
test: all
	@if [ -d "$(TEST_DIR)" ]; then \
	    $(MAKE) -C $(TEST_DIR) run; \
	else \
	    echo "no $(TEST_DIR)/ yet - test suite lands in Phase 2"; \
	fi

clean:
	rm -rf build

-include $(DEPS)
