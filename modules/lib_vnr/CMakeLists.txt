
## VNR Inference
add_library(fwk_voice_module_lib_vnr_inference INTERFACE)
file(GLOB_RECURSE VNR_INFERENCE_SOURCES src/inference/*.c src/inference/*.cc)
target_sources(fwk_voice_module_lib_vnr_inference INTERFACE ${VNR_INFERENCE_SOURCES})
target_include_directories(fwk_voice_module_lib_vnr_inference INTERFACE api/common api/inference)
target_link_libraries(fwk_voice_module_lib_vnr_inference
    INTERFACE
        core::xs3_math
        )
target_link_libraries(fwk_voice_module_lib_vnr_inference
    INTERFACE
        sdk::inferencing::lib_tflite_micro
        )

if(${CMAKE_SYSTEM_NAME} STREQUAL XCORE_XS3A)
    #  lib_tflite_micro compilation is dependant on the HW target which is why we don't build fwk_voice::vnr::inference as a static library
    target_compile_options(fwk_voice_module_lib_vnr_inference
        INTERFACE
            -Os
            -g
            -mcmodel=large
            -Wno-xcore-fptrgroup
    )

    target_compile_definitions(fwk_voice_module_lib_vnr_inference
        INTERFACE
            TF_LITE_STATIC_MEMORY=1
            TF_LITE_STRIP_ERROR_STRINGS=1            
            __xtflm_conf_h_exists__=1
            )
else()
    set(CMAKE_CXX_FLAGS "-std=c++11" CACHE STRING "C++ Compiler Base Flags" FORCE)

    set(BUILD_FLAGS
      "-Os"
      "-g"
      "-D__xtflm_conf_h_exists__"
      "-DNN_USE_REF" 
    )    
    target_compile_options(fwk_voice_module_lib_vnr_inference
        INTERFACE
            ${BUILD_FLAGS}
    )
endif()

add_library(fwk_voice::vnr::inference ALIAS fwk_voice_module_lib_vnr_inference)

## VNR Feature extraction
add_library(fwk_voice_module_lib_vnr_features STATIC)

file(GLOB_RECURSE VNR_FEATURES_SOURCES src/features/*.c)

target_sources(fwk_voice_module_lib_vnr_features PRIVATE ${VNR_FEATURES_SOURCES})

target_include_directories(fwk_voice_module_lib_vnr_features PUBLIC api/features api/common)

target_compile_definitions(fwk_voice_module_lib_vnr_features PRIVATE HEADROOM_CHECK=0)

target_compile_options(fwk_voice_module_lib_vnr_features
    PRIVATE
        -Os
        -g       
)

target_link_libraries(fwk_voice_module_lib_vnr_features
    PUBLIC
        core::xs3_math
        )

add_library(fwk_voice::vnr::features ALIAS fwk_voice_module_lib_vnr_features)
