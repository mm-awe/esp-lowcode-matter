cmake_minimum_required(VERSION 3.16)

set(IDF_TARGET esp32c6 CACHE STRING "" FORCE)

list(APPEND SDKCONFIG_DEFAULTS "sdkconfig.defaults.esp32c6")

if(NOT DEFINED ENV{ESP_AMP_PATH})
    message(FATAL_ERROR "Please set ESP_AMP_PATH to the path of the esp-amp repo")
endif()

set(ESP_AMP_PATH $ENV{ESP_AMP_PATH})
include(${ESP_AMP_PATH}/components/esp_amp/cmake/subcore_project.cmake)

idf_build_set_property(COMPILE_OPTIONS "-Wno-error=unused-label" APPEND)

set(COMPONENTS main)
set(PROJECT_VER "1.0")
project(rmt_loop_errata_test)
