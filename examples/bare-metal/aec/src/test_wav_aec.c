// Copyright 2021 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#if !X86_BUILD
#ifdef __XC__
    #define chanend_t chanend
#else
    #include <xcore/chanend.h>
#endif
#include <platform.h>
#include <print.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#include "aec_defines.h"
#include "aec_api.h"

#include "aec_config.h"
#include "aec_memory_pool.h"
#include "fileio.h"
#include "wav_utils.h"


extern void aec_testapp_process_frame(
        aec_state_t *main_state,
        aec_state_t *shadow_state,
        int32_t (*y_data)[AEC_PROC_FRAME_LENGTH+2],
        int32_t (*x_data)[AEC_PROC_FRAME_LENGTH+2]);

void aec_task(const char *input_file_name, const char *output_file_name) {
    //check validity of compile time configuration
    assert(AEC_MAX_Y_CHANNELS <= AEC_LIB_MAX_Y_CHANNELS);
    assert(AEC_MAX_X_CHANNELS <= AEC_LIB_MAX_X_CHANNELS);
    assert((AEC_MAX_Y_CHANNELS * AEC_MAX_X_CHANNELS * AEC_MAIN_FILTER_PHASES) <= (AEC_LIB_MAX_Y_CHANNELS * AEC_LIB_MAX_X_CHANNELS * AEC_LIB_MAIN_FILTER_PHASES));
    assert((AEC_MAX_Y_CHANNELS * AEC_MAX_X_CHANNELS * AEC_SHADOW_FILTER_PHASES) <= (AEC_LIB_MAX_Y_CHANNELS * AEC_LIB_MAX_X_CHANNELS * AEC_LIB_SHADOW_FILTER_PHASES));
    
    //open files
    file_t input_file, output_file, H_hat_file, delay_file;
    int ret = file_open(&input_file, input_file_name, "rb");
    assert((!ret) && "Failed to open file");
    ret = file_open(&output_file, output_file_name, "wb");
    assert((!ret) && "Failed to open file");
    ret = file_open(&H_hat_file, "H_hat.bin", "wb");
    assert((!ret) && "Failed to open file");
    ret = file_open(&delay_file, "delay.bin", "wb");
    assert((!ret) && "Failed to open file");

    wav_header input_header_struct, output_header_struct;
    unsigned input_header_size;
    if(get_wav_header_details(&input_file, &input_header_struct, &input_header_size) != 0){
        printf("error in att_get_wav_header_details()\n");
        _Exit(1);
    }
    file_seek(&input_file, input_header_size, SEEK_SET);
    if(input_header_struct.bit_depth != 32)
     {
         printf("Error: unsupported wav bit depth (%d) for %s file. Only 32 supported\n", input_header_struct.bit_depth, input_file_name);
         _Exit(1);
     }

    if(input_header_struct.num_channels != (AEC_MAX_Y_CHANNELS+AEC_MAX_X_CHANNELS)){
        printf("Error: wav num channels(%d) does not match aec(%u)\n", input_header_struct.num_channels, (AEC_MAX_Y_CHANNELS+AEC_MAX_X_CHANNELS));
        _Exit(1);
    }
    

    unsigned frame_count = wav_get_num_frames(&input_header_struct);

    unsigned block_count = frame_count / AEC_FRAME_ADVANCE;
    //printf("num frames = %d\n",block_count);
    wav_form_header(&output_header_struct,
            input_header_struct.audio_format,
            AEC_MAX_Y_CHANNELS,
            input_header_struct.sample_rate,
            input_header_struct.bit_depth,
            block_count*AEC_FRAME_ADVANCE);

    file_write(&output_file, (uint8_t*)(&output_header_struct),  WAV_HEADER_BYTES);

    int32_t input_read_buffer[AEC_PROC_FRAME_LENGTH*(AEC_MAX_Y_CHANNELS + AEC_MAX_X_CHANNELS)] = {0};
    int32_t output_write_buffer[AEC_FRAME_ADVANCE * (AEC_MAX_Y_CHANNELS)];

    int32_t DWORD_ALIGNED frame_y[AEC_MAX_Y_CHANNELS][AEC_PROC_FRAME_LENGTH + 2];
    int32_t DWORD_ALIGNED frame_x[AEC_MAX_X_CHANNELS][AEC_PROC_FRAME_LENGTH + 2];
    unsigned bytes_per_frame = wav_get_num_bytes_per_frame(&input_header_struct);

    //Start AEC
    uint8_t DWORD_ALIGNED aec_memory_pool[sizeof(aec_memory_pool_t)];
    uint8_t DWORD_ALIGNED aec_shadow_filt_memory_pool[sizeof(aec_shadow_filt_memory_pool_t)]; 
    aec_state_t DWORD_ALIGNED main_state;
    aec_state_t DWORD_ALIGNED shadow_state;
    aec_shared_state_t DWORD_ALIGNED aec_shared_state;
    
    aec_init(&main_state, &shadow_state, &aec_shared_state,
            &aec_memory_pool[0], &aec_shadow_filt_memory_pool[0],
            AEC_MAX_Y_CHANNELS, AEC_MAX_X_CHANNELS,
            AEC_MAIN_FILTER_PHASES, AEC_SHADOW_FILTER_PHASES);

    for(unsigned b=0;b<block_count;b++){
        //printf("frame %d\n",b);
        long input_location =  wav_get_frame_start(&input_header_struct, b * AEC_FRAME_ADVANCE, input_header_size);
        file_seek (&input_file, input_location, SEEK_SET);
        file_read (&input_file, (uint8_t*)&input_read_buffer[(AEC_PROC_FRAME_LENGTH-AEC_FRAME_ADVANCE)*(AEC_MAX_Y_CHANNELS+AEC_MAX_Y_CHANNELS)], bytes_per_frame* AEC_FRAME_ADVANCE);
        memset(frame_y, 0, sizeof(frame_y));
        memset(frame_x, 0, sizeof(frame_x));
        for(unsigned f=0; f<AEC_PROC_FRAME_LENGTH; f++){
            for(unsigned ch=0;ch<AEC_MAX_Y_CHANNELS;ch++){
                unsigned i =(f * (AEC_MAX_Y_CHANNELS+AEC_MAX_X_CHANNELS)) + ch;
                frame_y[ch][f] = input_read_buffer[i];
            }
            for(unsigned ch=0;ch<AEC_MAX_X_CHANNELS;ch++){
                unsigned i =(f * (AEC_MAX_Y_CHANNELS+AEC_MAX_X_CHANNELS)) + AEC_MAX_Y_CHANNELS + ch;
                frame_x[ch][f] = input_read_buffer[i];
            }
        }        
        aec_testapp_process_frame(&main_state, &shadow_state, frame_y, frame_x);

        for (unsigned ch=0;ch<AEC_MAX_Y_CHANNELS;ch++){
            for(unsigned i=0;i<AEC_FRAME_ADVANCE;i++){
                output_write_buffer[i*(AEC_MAX_Y_CHANNELS) + ch] = main_state.output[ch].data[i];
            }
        }

        file_write(&output_file, (uint8_t*)(output_write_buffer), output_header_struct.bit_depth/8 * AEC_FRAME_ADVANCE * AEC_MAX_Y_CHANNELS);

        for(unsigned i=0;i<(AEC_PROC_FRAME_LENGTH - AEC_FRAME_ADVANCE)*(AEC_MAX_Y_CHANNELS + AEC_MAX_X_CHANNELS);i++){
            input_read_buffer[i] = input_read_buffer[i + AEC_FRAME_ADVANCE*(AEC_MAX_Y_CHANNELS + AEC_MAX_X_CHANNELS)];
        }
    }
    file_close(&input_file);
    file_close(&output_file);
    file_close(&H_hat_file);
    file_close(&delay_file);
    shutdown_session();
}


#if !X86_BUILD
#define IN_WAV_FILE_NAME    "input.wav"
#define OUT_WAV_FILE_NAME   "output.wav"
void main_tile0(chanend_t xscope_chan)
{
#if TEST_WAV_XSCOPE
    xscope_io_init(xscope_chan);
#endif 
    aec_task(IN_WAV_FILE_NAME, OUT_WAV_FILE_NAME);
}
#else //Linux build
int main(int argc, char **argv) {
    if(argc < 3) {
        printf("Arguments missing. Expected: <input file name> <output file name>\n");
        assert(0);
    }
    aec_task(argv[1], argv[2]);
    return 0;
}
#endif
