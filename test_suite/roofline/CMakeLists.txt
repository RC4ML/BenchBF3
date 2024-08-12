project(ROOFLINE)

set(SAMPLE_NAME roofline)

set(APP_ADDITIONAL_SOURCES roofline.cpp)

# don't use vectorize instruction in this test
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-tree-vectorize")

add_executable(${SAMPLE_NAME} ./main.cpp ${UTILSOURCES} ${APP_ADDITIONAL_SOURCES})

target_link_libraries(${SAMPLE_NAME} ${LIBRARIES})