configure_file(${CMAKE_CURRENT_SOURCE_DIR}/.env ${CMAKE_CURRENT_BINARY_DIR}/.env COPYONLY)
set (TEST api-exchange-test)
include(../CMakeCommonTest.in)
