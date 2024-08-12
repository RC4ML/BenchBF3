project(DMA_LOCAL_TO_LOCAL_SAMPLE)

set(SAMPLE_NAME dma_local_sample)

set(APP_ADDITIONAL_SOURCES "")

add_executable(${SAMPLE_NAME} dma_local_sample_main.cpp dma_local_sample.cpp ${UTILSOURCES} ${APP_ADDITIONAL_SOURCES})

target_link_libraries(${SAMPLE_NAME} ${LIBRARIES})