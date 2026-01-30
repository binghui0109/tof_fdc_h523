# Default parameters
BOARD ?= EVK-PRE-FALL-H523
export BOARD
CONFIG ?= Debug
JOBS ?= 12
EXECUTABLE_NAME = $(BOARD)
HASH =  $(shell git rev-parse --short HEAD)
HOST_OS ?= WIN 
JLINK_EXE ?= JLink.exe

ifeq ($(OS),Windows_NT)
	HOST_OS = WIN 
else
	UNAME = $(shell uname -s)
	ifeq ($(UNAME),Linux)
		HOST_OS = LINUX
	endif
endif

ifeq ($(HOST_OS), WIN)
	JLINK_EXE ?= JLink.exe
else ifeq ($(HOST_OS), LINUX)
	JLINK_EXE = JLinkExe
endif

VALID_BOARDS := EVK-PRE-FALL-H523
VALID_CONFIGS := Debug Release
RUN_STATIC_ANALYSIS ?= 0
STATIC_ANALYZER ?= .venv/bin/python3 tools/run_static_analysis.py
ANALYZE_VENDOR ?= 0
STATIC_ANALYZER_FLAGS ?=
ifeq ($(ANALYZE_VENDOR),1)
STATIC_ANALYZER_FLAGS += --with-vendor
endif

ifeq ($(filter $(BOARD),$(VALID_BOARDS)),)
$(error "ERROR: Invalid BOARD Set. Available boards : $(VALID_BOARDS)")
endif

ifeq ($(filter $(CONFIG),$(VALID_CONFIGS)),)
$(error "ERROR: Invalid CONFIG set. Available Configurations : $(VALID_CONFIGS)")
endif

ifeq ($(findstring g473,$(BOARD)),H523)
	JLINK_DEVICE = STM32H523CE
endif

# The build directory will be build/<board>/<config>
BUILD_DIR = build/$(BOARD)/$(CONFIG)
EXECUTABLE_PATH = $(BUILD_DIR)/$(EXECUTABLE_NAME).hex
JLINK_SCRIPT = $(BUILD_DIR)/flash_$(BOARD).jlink

.PHONY: all configure build clean

# 'all' target: configures then builds
all: configure build
ifeq ($(RUN_STATIC_ANALYSIS),1)
all: static-analysis
endif

# Configure the project
configure:
	@echo "Configuring for board: $(BOARD)"
	@echo "Build configuration: $(CONFIG)"
	@echo ""
	@mkdir -p $(BUILD_DIR)
# @cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=$(CONFIG) -DBOARD=$(BOARD) ../../..
	@cmake --preset $(CONFIG) -DGIT_HASH=$(HASH) -DBOARD=$(BOARD)
#copy compile_commands.json file to project root to make intellsense happy
	@echo "Copying compile_commands.json to project root"
	@cp ${BUILD_DIR}/compile_commands.json .

# Build the project
build:
# $(MAKE) -C $(BUILD_DIR) -j${JOBS}
	cmake --build --preset $(CONFIG) --parallel ${JOBS}

.PHONY: static-analysis
static-analysis: configure build
	@if [ ! -x ".venv/bin/python3" ]; then \
		echo "Python venv not found. Please create it (python3 -m venv .venv) and install deps."; \
		exit 1; \
	fi
	@echo "Running static analysis (vendor scan: $(ANALYZE_VENDOR))"
	@$(STATIC_ANALYZER) --jobs $(JOBS) $(STATIC_ANALYZER_FLAGS)

$(BUILD_DIR)/$(BOARD).jlink: $(EXECUTABLE_PATH)
	@echo halt > $@
	@echo loadfile $^ >> $@
	@echo r >> $@
	@echo go >> $@
	@echo exit >> $@

# Flash using jlink
flash: all $(BUILD_DIR)/$(BOARD).jlink 
	@$(JLINK_EXE) -device $(JLINK_DEVICE) -if SWD -JTAGConf -1,-1 -speed auto -CommandFile $(BUILD_DIR)/$(BOARD).jlink

# Clean the build directory
clean:
	@echo "Cleaning ${BUILD_DIR}"
	@cmake --build ${BUILD_DIR} --target clean

realclean:
	@echo "Purging full build dir"
	@rm -rf build/*
