
set (API_TEST_LIB api_test)

include_directories(
        ${OPENSSL_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/
        ${CMAKE_CURRENT_SOURCE_DIR}/src/
        ${EXTERNAL_LOCATION}
        ${libuv_INCLUDEDIR}
)

message(STATUS "EXTERNAL_LOCATION: " ${EXTERNAL_LOCATION} )
message(STATUS "OPENSSL: " ${OPENSSL_INCLUDE_DIR} )
message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")

link_directories(
        ${EXTERNAL_INSTALL_LOCATION}/lib
        ${libuv_LIBRARY_DIRS}
)

add_subdirectory(initial)
add_subdirectory(address)
add_subdirectory(broker)
#add_subdirectory(amqp-cpp-async)
add_subdirectory(async-rpc-server)
add_subdirectory(async-rpc-client)
enable_testing ()
