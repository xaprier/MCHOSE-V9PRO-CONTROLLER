qt_add_executable(tst_protocol
    tst_protocol.cpp
)

target_link_libraries(tst_protocol
    PRIVATE
        Qt6::Test
        V9::Core
)

add_test(NAME tst_protocol COMMAND tst_protocol)

qt_add_executable(tst_engines
    tst_engines.cpp
)

target_link_libraries(tst_engines
    PRIVATE
        Qt6::Test
        V9::Core
)

add_test(NAME tst_engines COMMAND tst_engines)

qt_add_executable(tst_patchbay
    tst_patchbay.cpp
)

target_link_libraries(tst_patchbay
    PRIVATE
        Qt6::Test
        V9::Core
)

add_test(NAME tst_patchbay COMMAND tst_patchbay)
