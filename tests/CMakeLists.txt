qt_add_executable(tst_protocol
    tst_protocol.cpp
)

target_link_libraries(tst_protocol
    PRIVATE
        Qt6::Test
        V9::Core
)

add_test(NAME tst_protocol COMMAND tst_protocol)
