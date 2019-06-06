set (TEST api-exchange-test)

#file(COPY .env
#        DESTINATION ${EXECUTABLE_OUTPUT_PATH})

#configure_package_config_file(
#        ".env"
#        "${project_config}"
#        INSTALL_DESTINATION "${config_install_dir}"
#)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/.env ${CMAKE_CURRENT_BINARY_DIR}/.env COPYONLY)

#add_custom_command(
#        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/.env
#        COMMAND ${CMAKE_COMMAND} -E copy
#        ${CMAKE_CURRENT_SOURCE_DIR}/.env
#        ${CMAKE_CURRENT_BINARY_DIR}/.env)

include(../CMakeCommonTest.in)
