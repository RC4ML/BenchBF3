project(RDMA_BENCH)

set(SAMPLE_NAME rdma_send_bench)

set(APP_ADDITIONAL_SOURCES ${CMAKE_SOURCE_DIR}/utils/proto/dma_connect.pb.cc)

add_executable(${SAMPLE_NAME} ./main.cpp ./sender.cpp ./receiver.cpp ./rdma_bench.cpp ${UTILSOURCES} ${APP_ADDITIONAL_SOURCES})

### discard not initialized field
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers -Wno-old-style-cast")

add_executable(rdma_recv_control ./receive_control.cpp ${UTILSOURCES} ${APP_ADDITIONAL_SOURCES})

target_link_libraries(${SAMPLE_NAME} ${LIBRARIES})

target_link_libraries(rdma_recv_control ${LIBRARIES})