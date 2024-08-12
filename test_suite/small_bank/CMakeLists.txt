project(small_bank)

set(PROGRAMS client server)

set(APP_ADDITIONAL_SOURCES ${UTILSOURCES})

foreach (project_name IN LISTS PROGRAMS)
    add_executable(smallbank_${project_name} ${CMAKE_CURRENT_SOURCE_DIR}/${project_name}/${project_name}.cpp ${APP_ADDITIONAL_SOURCES})
    target_link_libraries(smallbank_${project_name} erpc ${LIBRARIES})
endforeach ()
