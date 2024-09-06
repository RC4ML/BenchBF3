cmake_minimum_required(VERSION 3.10)

execute_process(COMMAND cat /opt/mellanox/doca/applications/VERSION OUTPUT_VARIABLE DOCA_VERSION)
string(STRIP ${DOCA_VERSION} DOCA_VERSION)

message(STATUS "DOCA_VERSION: ${DOCA_VERSION}")

project(DOCA_SAMPLE VERSION ${DOCA_VERSION})

# whether on DPU or HOST
# this option is useless now
option(DPU OFF)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    message(STATUS "Compile on DPU")
    message(STATUS "Output Dir is /bin_dpu")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin_dpu)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build_dpu)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib_dpu)

elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    message(STATUS "Compile on HOST")
    message(STATUS "Output Dir is /bin_host")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin_host)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build_host)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib_host)
else()
    # 在其他架构下的逻辑代码
    message("Unknown architecture detected")
    return()
endif ()


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DALLOW_EXPERIMENTAL_API -std=c++20 -Wall -Wextra -Werror -pedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wold-style-cast -Wno-unused-function -Wno-missing-braces")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-address-of-packed-member -Wno-nested-anon-types -Wno-keyword-macro -Wno-deprecated-declarations")

add_definitions(-DDOCA_ALLOW_EXPERIMENTAL_API)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DOCA IMPORTED_TARGET doca)
pkg_check_modules(LIBDPDK IMPORTED_TARGET libdpdk)
pkg_check_modules(LIBFLEXIO IMPORTED_TARGET libflexio)

set(Protobuf_USE_STATIC_LIBS ON)
find_package(Protobuf REQUIRED)
if (Protobuf_FOUND)
    message(STATUS "Using Protocol Buffers ${Protobuf_VERSION}")
    #  message(STATUS "${Protobuf_INCLUDE_DIRS}")
    #  message(STATUS "${Protobuf_LITE_LIBRARIES}")
endif ()

if (DOCA_FOUND)
    message(STATUS "DOCA_LIB found")
else ()
    message(FATAL_ERROR "DOCA_LIB not found")
    return()
endif ()

if (LIBDPDK_FOUND)
    message(STATUS "LIBDPDK found")
else ()
    message(FATAL_ERROR "LIBDPDK not found")
    return()
endif ()

if (LIBFLEXIO_FOUND)
    message(STATUS "LIBFLEXIO found")
else ()
    message(FATAL_ERROR "LIBFLEXIO not found")
    return()
endif ()


#include: utils
include_directories("${CMAKE_SOURCE_DIR}/utils")

#include: utils/bench
include_directories("${CMAKE_SOURCE_DIR}/utils/bench")

#include: utils/network
include_directories("${CMAKE_SOURCE_DIR}/utils/network")

#include: utils/dpdk
include_directories("${CMAKE_SOURCE_DIR}/utils/dpdk")

#include: wrapper
include_directories("${CMAKE_SOURCE_DIR}/wrapper")

include_directories("${CMAKE_SOURCE_DIR}/wrapper_flexio")

#sub-projects: json
include_directories(${CMAKE_SOURCE_DIR}/third_party/json/include)

#sub-project: HdrHistogram
include_directories(${CMAKE_SOURCE_DIR}/third_party/HdrHistogram/include)


#include: DOCA
include_directories("${DOCA_INCLUDE_DIRS}")

#include: LIBDPDK
include_directories("${LIBDPDK_INCLUDE_DIRS}")

#include: asio
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/third_party/asio/include)

#include: proto
include_directories(${Protobuf_INCLUDE_DIRS})

# Common sub-projects: atomic_queue
include_directories(${CMAKE_SOURCE_DIR}/third_party/atomic_queue/include)

#include: eRPC
# if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
#     include_directories(${CMAKE_SOURCE_DIR}/third_party/eRPC/src)
# endif()

set(UTILSOURCES
        ${CMAKE_SOURCE_DIR}/utils/utils_common.cpp
        ${CMAKE_SOURCE_DIR}/utils/hdr_histogram.cpp
        ${CMAKE_SOURCE_DIR}/utils/numautil.cpp
        ${CMAKE_SOURCE_DIR}/utils/gflags_common.cpp
        ${CMAKE_SOURCE_DIR}/utils/log.cpp
        ${CMAKE_SOURCE_DIR}/utils/dpdk/offload_rules.c
        ${CMAKE_SOURCE_DIR}/utils/dpdk/dpdk_utils.c
)
file(GLOB WRAPPERSOURCES ${CMAKE_SOURCE_DIR}/wrapper/*.cpp)
list(APPEND UTILSOURCES ${WRAPPERSOURCES})

set(LIBRARIES ${LIBRARIES} PkgConfig::DOCA PkgConfig::LIBDPDK PkgConfig::LIBFLEXIO nlohmann_json::nlohmann_json ${Protobuf_LITE_LIBRARIES} gflags numa hdr_histogram_static z lz4)
## used for cache_invalid
set(LIBRARIES ${LIBRARIES} ibverbs mlx5)

add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/json)
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/HdrHistogram)

# if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
#     add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/eRPC)
# endif()

if (EXISTS "${CMAKE_SOURCE_DIR}/test_suite/build_app")
    file(STRINGS "${CMAKE_SOURCE_DIR}/test_suite/build_app" APP)
else ()
    message(STATUS "No build_app found. No application will be compiled.")
    return()
endif ()

if (APP STREQUAL "dma_local_to_local_sample")
    add_subdirectory(${CMAKE_SOURCE_DIR}/test_suite/dma_local_to_local_sample)
elseif (APP STREQUAL "dma_copy_bench")
    add_subdirectory(${CMAKE_SOURCE_DIR}/test_suite/dma_copy_bench)
elseif (APP STREQUAL "channel_bench")
    add_subdirectory(${CMAKE_SOURCE_DIR}/test_suite/channel_bench)
elseif (APP STREQUAL "regex_bench")
    add_subdirectory(${CMAKE_SOURCE_DIR}/test_suite/regex_bench)
elseif (APP STREQUAL "compress_bench")
    add_subdirectory(${CMAKE_SOURCE_DIR}/test_suite/compress_bench)
elseif (APP STREQUAL "ec_bench")
    add_subdirectory(${CMAKE_SOURCE_DIR}/test_suite/ec_bench)
elseif (APP STREQUAL "memory_bench")
    add_subdirectory(${CMAKE_SOURCE_DIR}/test_suite/memory_bench)
elseif (APP STREQUAL "roofline")
    add_subdirectory(${CMAKE_SOURCE_DIR}/test_suite/roofline)
elseif(APP STREQUAL "small_bank" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    add_subdirectory(${CMAKE_SOURCE_DIR}/test_suite/small_bank)
elseif(APP STREQUAL "rdma_bench")
    add_subdirectory(${CMAKE_SOURCE_DIR}/test_suite/rdma_bench)
elseif(APP STREQUAL "gdr_bench")
    add_subdirectory(${CMAKE_SOURCE_DIR}/test_suite/gdr_bench)
else ()
    message(SATTUS "No ${APP} found. No application will be compiled.")
endif ()

add_subdirectory(${CMAKE_SOURCE_DIR}/flexio/dpa_send_mt)
add_subdirectory(${CMAKE_SOURCE_DIR}/flexio/dpa_send_ntp)
add_subdirectory(${CMAKE_SOURCE_DIR}/flexio/dpa_send)
add_subdirectory(${CMAKE_SOURCE_DIR}/flexio/dpa_refactor_mt)
add_subdirectory(${CMAKE_SOURCE_DIR}/flexio/dpa_refactor_workingset)
add_subdirectory(${CMAKE_SOURCE_DIR}/flexio/dpa_refactor_random_access)
add_subdirectory(${CMAKE_SOURCE_DIR}/flexio/dpa_network_function)
add_subdirectory(${CMAKE_SOURCE_DIR}/flexio/dpa_kv_aggregation)
