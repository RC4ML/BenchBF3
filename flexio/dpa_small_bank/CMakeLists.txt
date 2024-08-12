project(DPA_SMALL_BANK)

set(SAMPLE_NAME dpa_small_bank)


file(GLOB FLEXIOSOURCES ${CMAKE_SOURCE_DIR}/wrapper_flexio/*.cpp)
list(APPEND APP_ADDITIONAL_SOURCES ${FLEXIOSOURCES})
set(app_dev_build_dir ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set(program_lib_include_dir -I${CMAKE_SOURCE_DIR}/wrapper_flexio)
#set(program_user_lib -lflexio_dev)
# don't use link it
set(program_user_lib)
set(dpacc_build_script ${CMAKE_SOURCE_DIR}/build_dpacc_samples.sh)



set(server_lib_name ${SAMPLE_NAME}_server_device)

set(server_source_file ${CMAKE_CURRENT_SOURCE_DIR}/device/dpa_small_bank_server_device.c)
list(APPEND server_source_file ${CMAKE_SOURCE_DIR}/wrapper_flexio/wrapper_flexio_device.c)


add_executable(${SAMPLE_NAME}_server ./server.cpp ${UTILSOURCES} ${APP_ADDITIONAL_SOURCES})

add_custom_command(OUTPUT dpacc_${SAMPLE_NAME}_server
        COMMAND ${dpacc_build_script} ${server_lib_name} \"${server_source_file}\" ${app_dev_build_dir} ${program_lib_include_dir} ${program_user_lib}
)
add_custom_target(
        dpacc_${SAMPLE_NAME}_server_target ALL
        DEPENDS dpacc_${SAMPLE_NAME}_server
)
add_dependencies(${SAMPLE_NAME}_server dpacc_${SAMPLE_NAME}_server_target)
target_link_libraries(${SAMPLE_NAME}_server ${LIBRARIES} ${app_dev_build_dir}/${server_lib_name}.a ibverbs mlx5)


set(client_lib_name ${SAMPLE_NAME}_client_device)

set(client_source_file ${CMAKE_CURRENT_SOURCE_DIR}/device/dpa_small_bank_client_device.c)
list(APPEND client_source_file ${CMAKE_SOURCE_DIR}/wrapper_flexio/wrapper_flexio_device.c)


add_executable(${SAMPLE_NAME}_client ./client.cpp ${UTILSOURCES} ${APP_ADDITIONAL_SOURCES})

add_custom_command(OUTPUT dpacc_${SAMPLE_NAME}_client
        COMMAND ${dpacc_build_script} ${client_lib_name} \"${client_source_file}\" ${app_dev_build_dir} ${program_lib_include_dir} ${program_user_lib}
)

add_custom_target(
        dpacc_${SAMPLE_NAME}_client_target ALL
        DEPENDS dpacc_${SAMPLE_NAME}_client
)
add_dependencies(${SAMPLE_NAME}_client dpacc_${SAMPLE_NAME}_client_target)
target_link_libraries(${SAMPLE_NAME}_client ${LIBRARIES} ${app_dev_build_dir}/${client_lib_name}.a ibverbs mlx5)


