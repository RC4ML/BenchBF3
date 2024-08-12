project(COMPRESS_BENCH)

set(SAMPLE_NAME compress_bench)

set(APP_ADDITIONAL_SOURCES)

add_executable(${SAMPLE_NAME} ./main.cpp compress_bench.cpp ${UTILSOURCES} ${APP_ADDITIONAL_SOURCES})

target_link_libraries(${SAMPLE_NAME} ${LIBRARIES})