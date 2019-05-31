
file (GLOB TESTS_SOURCES ${TESTS_SOURCES}
        ./*.cpp
        )

set (TEST api-initial-test)

add_executable(${TEST} ${TESTS_SOURCES})

target_link_libraries (
        ${TEST}
        ${PROJECT_LIB}
        gtest gtest_main
)

add_test (test ${TEST})
enable_testing ()
