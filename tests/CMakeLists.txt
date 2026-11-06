add_executable(xenon_tests_vm
    test_vm.cpp)

target_link_libraries(xenon_tests_vm PRIVATE xenon::core)

add_test(NAME xenon_tests_vm COMMAND xenon_tests_vm)

add_executable(xenon_tests_module
    test_module.cpp)

target_link_libraries(xenon_tests_module PRIVATE xenon::core)

add_test(NAME xenon_tests_module COMMAND xenon_tests_module)

add_executable(xenon_tests_env
    test_env.cpp)

target_link_libraries(xenon_tests_env PRIVATE xenon::core)

add_test(NAME xenon_tests_env COMMAND xenon_tests_env)

add_executable(xenon_tests_lang
    test_lang.cpp)

target_link_libraries(xenon_tests_lang PRIVATE xenon::core)

add_test(NAME xenon_tests_lang COMMAND xenon_tests_lang)
