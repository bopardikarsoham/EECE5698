#include "ap_int.h"

#define SYNDROME_WIDTH 24  
#define CORRECTION_WIDTH 12 

extern "C" {
void qec_decoder(
    ap_uint<SYNDROME_WIDTH>* stream_in, 
    ap_uint<CORRECTION_WIDTH>* stream_out, 
    int num_shots
) {
    // AXI Master memory bundles for data pointers
    #pragma HLS INTERFACE m_axi port=stream_in bundle=gmem0 offset=slave
    #pragma HLS INTERFACE m_axi port=stream_out bundle=gmem1 offset=slave

    // Unified AXI-Lite control bundle for all scalar parameters and port offsets
    #pragma HLS INTERFACE s_axilite port=stream_in bundle=control
    #pragma HLS INTERFACE s_axilite port=stream_out bundle=control
    #pragma HLS INTERFACE s_axilite port=num_shots bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control

    for (int i = 0; i < num_shots; i++) {
        #pragma HLS PIPELINE II=1

        ap_uint<SYNDROME_WIDTH> current_syndrome = stream_in[i];
        ap_uint<CORRECTION_WIDTH> correction = 0;

        if (current_syndrome != 0) {
            correction = current_syndrome(CORRECTION_WIDTH-1, 0);
        } else {
            correction = 0;
        }

        stream_out[i] = correction;
    }
}
}
