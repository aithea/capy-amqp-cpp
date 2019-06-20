set (TEST amqp-cpp-async-test)
include(../CMakeCommonTest.in)

if (${amqpcpp_FOUND})
    add_dependencies(
            ${TEST}
            AMQP-CPP
    )
endif()

target_link_libraries(
        ${TEST}
        ${PROJECT_LIB}
        amqpcpp
)
