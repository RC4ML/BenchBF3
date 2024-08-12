project(EC_BENCH)

set(SAMPLE_NAME ec_bench)

set(APP_ADDITIONAL_SOURCES)

add_executable(${SAMPLE_NAME} ./main.cpp ec_bench.cpp ${UTILSOURCES} ${APP_ADDITIONAL_SOURCES})

target_link_libraries(${SAMPLE_NAME} ${LIBRARIES})