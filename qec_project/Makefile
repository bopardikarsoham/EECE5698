# Target platform and hardware definition
PLATFORM = /opt/xilinx/platforms/xilinx_u280_gen3x16_xdma_1_202211_1/xilinx_u280_gen3x16_xdma_1_202211_1.xpfm
TARGET   = hw
TYPE     = c++

# Directories
SRC_DIR  = src
BUILD_DIR = build

# Compiler flags for Host
CXX = g++
CXXFLAGS = -std=c++17 -Wall -O3 -I/opt/xilinx/xrt/include
LDFLAGS = -L/opt/xilinx/xrt/lib -lxrt_coreutil -lpthread

all: host kernel

# 1. Compile Host Application
host: $(BUILD_DIR)/host

$(BUILD_DIR)/host: $(SRC_DIR)/host/host.cpp | $(BUILD_DIR)
	@echo "Compiling host application..."
	$(CXX) $(CXXFLAGS) $(SRC_DIR)/host/host.cpp -o $(BUILD_DIR)/host $(LDFLAGS)
	@echo "Host build complete: $(BUILD_DIR)/host"

# 2. Compile FPGA Kernel (Generates .xclbin bitstream)
kernel: $(BUILD_DIR)/qec_decoder.xclbin

$(BUILD_DIR)/qec_decoder.xclbin: $(SRC_DIR)/kernel/qec_decoder.cpp | $(BUILD_DIR)
	@echo "Starting Vitis HLS kernel synthesis (This will take a while)..."
	v++ -t $(TARGET) --platform $(PLATFORM) -k qec_decoder \
		--compile -R2 -o $(BUILD_DIR)/qec_decoder.xo $(SRC_DIR)/kernel/qec_decoder.cpp
	v++ -t $(TARGET) --platform $(PLATFORM) \
		--link $(BUILD_DIR)/qec_decoder.xo -o $(BUILD_DIR)/qec_decoder.xclbin
	@echo "FPGA bitstream build complete: $(BUILD_DIR)/qec_decoder.xclbin"

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) *.log *.jou .Xil _x
	@echo "Cleaned build artifacts."

.PHONY: all host kernel clean
