if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.env)
    file (WRITE ".env" "CAPY_AMQP_ADDRESS=amqp://guest:guest@localhost:5672/")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.env)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/.env ${CMAKE_CURRENT_BINARY_DIR}/.env COPYONLY)
endif()

include_directories(../../src/impl)

set (TEST api-async-rpc-client-test)
include(../CMakeCommonTest.in)
