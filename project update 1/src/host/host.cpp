#include <iostream>
#include <vector>
#include <fstream>
#include <chrono>

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

int main(int argc, char** argv) {
    std::cout << "Starting QEC FPGA Host Application...\n";

    // 1. Load the FPGA device (Index 0)
    unsigned int device_index = 0;
    auto device = xrt::device(device_index);
    std::cout << "Connected to device: " << device.get_info<xrt::info::device::name>() << "\n";

    // 2. Load the compiled FPGA binary (.xclbin)
    std::string xclbin_path = "qec_decoder.xclbin";
    std::cout << "Loading bitstream: " << xclbin_path << "\n";
    auto uuid = device.load_xclbin(xclbin_path);

    // 3. Create the kernel handle first so we can query memory bank group IDs
    auto decoder_kernel = xrt::kernel(device, uuid, "qec_decoder");

    // 4. Load real syndromes from the Python simulation binary file
    std::string input_file_path = "simulation/sample_syndromes.bin";
    std::ifstream bien(input_file_path, std::ios::binary | std::ios::ate);
    if (!bien.is_open()) {
        std::cerr << "Error: Could not open " << input_file_path << "\n";
        return 1;
    }
    
    std::streamsize file_size = bien.tellg();
    bien.seekg(0, std::ios::beg);

    size_t num_shots = file_size / sizeof(uint32_t);
    std::vector<uint32_t> input_syndromes(num_shots);
    if (!bien.read(reinterpret_cast<char*>(input_syndromes.data()), file_size)) {
        std::cerr << "Error: Failed to read syndrome data from file.\n";
        return 1;
    }
    bien.close();

    std::cout << "Loaded " << num_shots << " syndrome shots from " << input_file_path << "\n";
    size_t vector_size_bytes = num_shots * sizeof(uint32_t);
    std::vector<uint32_t> output_corrections(num_shots, 0);

    // 5. Allocate Buffer Objects (BO) using the kernel's argument group IDs
    auto bo_in  = xrt::bo(device, vector_size_bytes, decoder_kernel.group_id(0));
    auto bo_out = xrt::bo(device, vector_size_bytes, decoder_kernel.group_id(1));

    // 6. Map and copy input data to the device buffer
    bo_in.write(input_syndromes.data());
    bo_in.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    std::cout << "Executing QEC decoder kernel on FPGA with real syndromes...\n";
    auto start_time = std::chrono::high_resolution_clock::now();

    // 7. Launch the kernel: arguments are (input_buffer, output_buffer, num_shots)
    auto run = decoder_kernel(bo_in, bo_out, num_shots);
    run.wait();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    std::cout << "Kernel execution finished in " << duration << " microseconds.\n";

    // 8. Read output corrections back from the device buffer
    bo_out.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    bo_out.read(output_corrections.data());

    std::cout << "First 5 simulation results (Decimal):\n";
    for (size_t i = 0; i < 5 && i < num_shots; i++) {
        std::cout << "Shot " << i 
                  << " -> Input Syndrome: " << input_syndromes[i] 
                  << " | FPGA Correction: " << output_corrections[i] << "\n";
    }

    std::cout << "Host execution completed successfully!\n";
    return 0;
}
