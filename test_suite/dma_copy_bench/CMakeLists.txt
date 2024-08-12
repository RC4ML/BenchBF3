project(DMA_COPY_BENCH)

set(SAMPLE_NAME dma_copy_bench)

set(APP_ADDITIONAL_SOURCES ./dma_copy_bench.cpp ${CMAKE_SOURCE_DIR}/utils/proto/dma_connect.pb.cc)

add_executable(${SAMPLE_NAME} ./main.cpp ./exporter.cpp ./importer.cpp ${UTILSOURCES} ${APP_ADDITIONAL_SOURCES})

target_link_libraries(${SAMPLE_NAME} ${LIBRARIES})