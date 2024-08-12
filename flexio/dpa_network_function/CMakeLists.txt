project(DPA_NETWORK_FUNCTION)

set(SAMPLE_NAME dpa_network_function)


file(GLOB FLEXIOSOURCES ${CMAKE_SOURCE_DIR}/wrapper_flexio/*.cpp)
list(APPEND APP_ADDITIONAL_SOURCES ${FLEXIOSOURCES})

set(program_lib_name dpa_network_function_device)
file(GLOB_RECURSE app_dev_source_file ${CMAKE_CURRENT_SOURCE_DIR}/device/*.c)
list(APPEND app_dev_source_file ${CMAKE_SOURCE_DIR}/wrapper_flexio/wrapper_flexio_device.c)


set(app_dev_build_dir ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set(program_lib_include_dir -I${CMAKE_SOURCE_DIR}/wrapper_flexio)
#set(program_user_lib -lflexio_dev)
# don't use link it
set(program_user_lib)
set(dpacc_build_script ${CMAKE_SOURCE_DIR}/build_dpacc_samples.sh)

add_executable(${SAMPLE_NAME} ./main.cpp dpa_network_function.cpp ${UTILSOURCES} ${APP_ADDITIONAL_SOURCES})

add_custom_command(OUTPUT dpacc_${SAMPLE_NAME}
        COMMAND ${dpacc_build_script} ${program_lib_name} \"${app_dev_source_file}\" ${app_dev_build_dir} ${program_lib_include_dir} ${program_user_lib}
)
add_custom_target(
        dpacc_${SAMPLE_NAME}_target ALL
        DEPENDS dpacc_${SAMPLE_NAME}
)

add_dependencies(${SAMPLE_NAME} dpacc_${SAMPLE_NAME}_target)
target_link_libraries(${SAMPLE_NAME} ${LIBRARIES} ${app_dev_build_dir}/${program_lib_name}.a ibverbs mlx5)



