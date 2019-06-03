
set (API_TEST_LIB api_test)

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/
        ${CMAKE_CURRENT_SOURCE_DIR}/src/
        ${EXTERNAL_LOCATION}/nlohmann
)

message(STATUS "EXTERNAL_LOCATION: " ${EXTERNAL_LOCATION} )

link_directories(
        ${EXTERNAL_INSTALL_LOCATION}/lib
)

add_subdirectory(initial)
add_subdirectory(address)
enable_testing ()
