project(GDR_BENCH)

set(SAMPLE_NAME gdr_bench)

set(APP_ADDITIONAL_SOURCES ${CMAKE_SOURCE_DIR}/utils/proto/dma_connect.pb.cc)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    find_package(CUDAToolkit REQUIRED)
    include_directories(${CUDA_INCLUDE_DIRS})
endif()

add_executable(${SAMPLE_NAME} ./main.cpp ./exporter.cpp ./importer.cpp ${UTILSOURCES} ${APP_ADDITIONAL_SOURCES})

target_link_libraries(${SAMPLE_NAME} ${LIBRARIES})

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    target_link_libraries(${SAMPLE_NAME} CUDA::cudart CUDA::cuda_driver)
endif()