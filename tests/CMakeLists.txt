
set (API_TEST_LIB api_test)


link_directories(
        ${EXTERNAL_INSTALL_LOCATION}/lib
)

include_directories(
        ./
        ../include
        ${EXTERNAL_INSTALL_LOCATION}/include
        ../external/nlohmann/include

)

add_subdirectory(initial)
enable_testing ()
