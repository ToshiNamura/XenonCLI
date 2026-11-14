# One test executable per component keeps test output focused and lets CTest run
# them in parallel. Add new components to the list; the naming convention is
# tests/test_<name>.cpp -> xenon_tests_<name>.
set(XENON_TEST_COMPONENTS
    vm
    module
    env
    lang
    hint
    asm)

foreach(component IN LISTS XENON_TEST_COMPONENTS)
    add_executable(xenon_tests_${component}
        test_${component}.cpp)

    target_link_libraries(xenon_tests_${component} PRIVATE xenon::core)

    if(component STREQUAL "env")
        target_link_libraries(xenon_tests_${component} PRIVATE xenon::env)
    endif()

    add_test(NAME xenon_tests_${component} COMMAND xenon_tests_${component})
endforeach()
