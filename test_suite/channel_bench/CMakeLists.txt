project(CHANNEL_BENCH)

set(SAMPLE_NAME channel_bench)

set(APP_ADDITIONAL_SOURCES ${CMAKE_SOURCE_DIR}/utils/proto/dma_connect.pb.cc)

if (EXISTS "${CMAKE_SOURCE_DIR}/test_suite/channel_bench/build_app")
    file(STRINGS "${CMAKE_SOURCE_DIR}/test_suite/channel_bench/build_app" APP_DEFINE)
else ()
    message(STATUS "No build_app found. No application will be compiled.")
    return()
endif ()

if (APP_DEFINE STREQUAL "rtt")
    add_definitions(-DRTT)
    message(STATUS "Using RTT for channel_bench")
elseif (APP_DEFINE STREQUAL "dma")
    add_definitions(-DDMA)
    message(STATUS "Using DMA for channel_bench")
elseif (APP_DEFINE STREQUAL "sync")
    add_definitions(-DSYNC)
    set(APP_ADDITIONAL_SOURCES ${APP_ADDITIONAL_SOURCES} sync_event.cpp)
    message(STATUS "Using SYNC for channel_bench")
else ()
    message(FATAL_ERROR "error for compile type")
    return()
endif ()


add_executable(${SAMPLE_NAME} ./main.cpp ./client.cpp ./server.cpp channel_bench.cpp ${UTILSOURCES} ${APP_ADDITIONAL_SOURCES})

target_link_libraries(${SAMPLE_NAME} ${LIBRARIES})