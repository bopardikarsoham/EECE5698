# Target platform and hardware definition
PLATFORM = /opt/xilinx/platforms/xilinx_u280_gen3x16_xdma_1_202211_1/xilinx_u280_gen3x16_xdma_1_202211_1.xpfm
TARGET   = hw
TYPE     = c++

# Directories
SRC_DIR  = src
BUILD_DIR = build

# Kernel clock in MHz (empty: the platform default, 300 MHz).
# make kernel KERNEL_FREQ=400
KERNEL_FREQ ?=
FREQ_FLAG = $(if $(KERNEL_FREQ),--kernel_frequency $(KERNEL_FREQ))

# Compiler flags for Host
CXX = g++
CXXFLAGS = -std=c++17 -Wall -O3 -I/opt/xilinx/xrt/include
LDFLAGS = -L/opt/xilinx/xrt/lib -lxrt_coreutil -lpthread

all: host kernel

# 1. Compile Host Application
host: $(BUILD_DIR)/host

$(BUILD_DIR)/host: $(SRC_DIR)/host/host.cpp $(SRC_DIR)/host/case_io.h $(SRC_DIR)/info_layout.h | $(BUILD_DIR)
	@echo "Compiling host application..."
	$(CXX) $(CXXFLAGS) $(SRC_DIR)/host/host.cpp -o $(BUILD_DIR)/host $(LDFLAGS)
	@echo "Host build complete: $(BUILD_DIR)/host"

# 2. Compile FPGA Kernel (Generates .xclbin bitstream)
kernel: $(BUILD_DIR)/qec_decoder.xclbin

$(BUILD_DIR)/qec_decoder.xclbin: $(SRC_DIR)/kernel/qec_decoder.cpp $(SRC_DIR)/uf_core.h $(SRC_DIR)/uf_fast.h $(SRC_DIR)/info_layout.h | $(BUILD_DIR)
	@echo "Starting Vitis HLS kernel synthesis (This will take a while)..."
	v++ -t $(TARGET) --platform $(PLATFORM) -k qec_decoder $(FREQ_FLAG) \
		--compile -R2 -o $(BUILD_DIR)/qec_decoder.xo $(SRC_DIR)/kernel/qec_decoder.cpp
	v++ -t $(TARGET) --platform $(PLATFORM) $(FREQ_FLAG) \
		--link $(BUILD_DIR)/qec_decoder.xo -o $(BUILD_DIR)/qec_decoder.xclbin
	@echo "FPGA bitstream build complete: $(BUILD_DIR)/qec_decoder.xclbin"

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) *.log *.jou .Xil _x
	@echo "Cleaned build artifacts."

# 3. Software (CPU) version of the decoder - runs on any machine, no Xilinx tools.
#    make sw        CFG=<case_dir>   decode a case with the C++ union-find decoder
#    make score     CFG=<case_dir>   logical error rate vs truth and vs PyMatching
#    make freeze    CFG=<case_dir>   copy a case into tests/cases/ as a regression test
#    make check_all                  decode every case in tests/cases/ and compare
#    make toy                        regenerate the toy cases in tests/cases/
PY       ?= .venv/bin/python
SW_CXX   ?= g++
SW_FLAGS  = -std=c++17 -O2 -Wall -Wextra
SW_BIN    = $(BUILD_DIR)/sw/golden
CFG      ?= tests/cases/toy_rep_d3

$(SW_BIN): $(SRC_DIR)/golden.cpp $(SRC_DIR)/uf_core.h
	mkdir -p $(BUILD_DIR)/sw
	$(SW_CXX) $(SW_FLAGS) $(SRC_DIR)/golden.cpp -o $(SW_BIN)

golden: $(SW_BIN)

sw: $(SW_BIN)
	./$(SW_BIN) $(CFG)

score:
	$(PY) python/score.py $(CFG)

freeze:
	sh tests/freeze.sh $(CFG)

check_all: $(SW_BIN)
	sh tests/check_all.sh $(SW_BIN)

toy:
	$(PY) python/make_toy.py

# 4. FPGA kernel testbench. kernel_csim runs it with g++ on any machine; hls_csim and
#    hls_synth run Vitis HLS C simulation and synthesis (needs Vitis HLS).
#    make kernel_csim                 kernel vs golden on every case in tests/cases/
#    make kernel_csim CASES="<dirs>"  kernel vs golden on other cases
#    make hls_cosim                   C/RTL co-simulation: exact cycles per shot
KERNEL_TB = $(BUILD_DIR)/sw/kernel_tb
CASES    ?= $(wildcard tests/cases/*)

$(KERNEL_TB): $(SRC_DIR)/kernel/tb.cpp $(SRC_DIR)/kernel/qec_decoder.cpp $(SRC_DIR)/uf_core.h $(SRC_DIR)/uf_fast.h $(SRC_DIR)/info_layout.h $(SRC_DIR)/host/case_io.h
	mkdir -p $(BUILD_DIR)/sw
	$(SW_CXX) $(SW_FLAGS) -Wno-unknown-pragmas -Wno-unused-label $(SRC_DIR)/kernel/tb.cpp $(SRC_DIR)/kernel/qec_decoder.cpp -o $(KERNEL_TB)

kernel_csim: $(KERNEL_TB)
	./$(KERNEL_TB) $(CASES)

hls_csim:
	vitis_hls -f hls/run_hls.tcl -tclargs csim

hls_synth:
	vitis_hls -f hls/run_hls.tcl -tclargs synth

hls_cosim:
	mkdir -p results
	vitis_hls -f hls/run_hls.tcl -tclargs cosim

.PHONY: all host kernel clean golden sw score freeze check_all toy kernel_csim hls_csim hls_synth hls_cosim
