file( GLOB_RECURSE    LIB_IC_SOURCES       src/*.c )

## Create library target
add_library(fwk_voice_module_lib_ic STATIC)

target_sources(fwk_voice_module_lib_ic
    PRIVATE
        ${LIB_IC_SOURCES}
)

target_include_directories(fwk_voice_module_lib_ic
    PUBLIC
        api
)

target_compile_options(fwk_voice_module_lib_ic
    PRIVATE
        -Os
        -g
)

target_link_libraries(fwk_voice_module_lib_ic
    PUBLIC
        core::xs3_math
        fwk_voice::aec
)

## Create an alias
add_library(fwk_voice::ic ALIAS fwk_voice_module_lib_ic)
