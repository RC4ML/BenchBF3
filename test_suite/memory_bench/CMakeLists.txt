project(MEMORY_BENCH)

set(SAMPLE_NAME memory_bench)

set(APP_ADDITIONAL_SOURCES memory_bench.cpp)

add_executable(${SAMPLE_NAME} ./main.cpp ${UTILSOURCES} ${APP_ADDITIONAL_SOURCES})

target_link_libraries(${SAMPLE_NAME} ${LIBRARIES})