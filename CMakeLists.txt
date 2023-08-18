cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(eurorack
    VERSION 0.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)

function(add_module)
    cmake_parse_arguments(ARGS "" "TARGET" "SOURCES" ${ARGN})

    set(TARGET mi_${ARGS_TARGET})

    add_library(${TARGET} STATIC ${ARGS_SOURCES})

    target_compile_definitions(${TARGET} 
        PUBLIC
            -DTEST
    )

    target_include_directories(${TARGET}
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_TARGET}>
            $<INSTALL_INTERFACE:${ARGS_TARGET}>
    )

    if (NOT ${ARGS_TARGET} STREQUAL "stmlib")
        target_link_libraries(${TARGET} PUBLIC mi_stmlib)
    endif()
endfunction()

add_module(
    TARGET  stmlib
    SOURCES
        stmlib/utils/random.cc
        stmlib/dsp/atan.cc
        stmlib/dsp/units.cc
)

add_module(
    TARGET  rings
    SOURCES
        rings/dsp/fm_voice.cc
        rings/dsp/part.cc
        rings/dsp/string_synth_part.cc
        rings/dsp/string.cc
        rings/dsp/resonator.cc
        rings/resources.cc
)

add_module(
    TARGET  clouds
    SOURCES
        clouds/dsp/correlator.cc
        clouds/dsp/granular_processor.cc
        clouds/dsp/mu_law.cc
        clouds/dsp/pvoc/frame_transformation.cc
        clouds/dsp/pvoc/phase_vocoder.cc
        clouds/dsp/pvoc/stft.cc
        clouds/resources.cc
)

add_module(
    TARGET  warps
    SOURCES
        warps/dsp/modulator.cc
        warps/dsp/oscillator.cc
        warps/dsp/vocoder.cc
        warps/dsp/filter_bank.cc
        warps/resources.cc
)

add_module(
    TARGET  elements
    SOURCES
        elements/dsp/exciter.cc
        elements/dsp/ominous_voice.cc
        elements/dsp/resonator.cc
        elements/dsp/tube.cc
        elements/dsp/multistage_envelope.cc
        elements/dsp/part.cc
        elements/dsp/string.cc
        elements/dsp/voice.cc
        elements/resources.cc
)

file(GLOB PLAITS_SOURCES_0 ${CMAKE_CURRENT_SOURCE_DIR}/plaits/dsp/*.cc)
file(GLOB PLAITS_SOURCES_1 ${CMAKE_CURRENT_SOURCE_DIR}/plaits/dsp/engine/*.cc)
file(GLOB PLAITS_SOURCES_2 ${CMAKE_CURRENT_SOURCE_DIR}/plaits/dsp/speech/*.cc)
file(GLOB PLAITS_SOURCES_3 ${CMAKE_CURRENT_SOURCE_DIR}/plaits/dsp/physical_modelling/*.cc)
add_module(
    TARGET  plaits
    SOURCES
        ${PLAITS_SOURCES_0}
        ${PLAITS_SOURCES_1}
        ${PLAITS_SOURCES_2}
        ${PLAITS_SOURCES_3}
        plaits/resources.cc
)
