add_executable(test_device test_device.cpp)
target_link_libraries(test_device PRIVATE pipslib)

add_test(NAME test_device COMMAND test_device)

add_executable(test_device_gpu_fallback ../examples/gpu_example.cpp)
target_link_libraries(test_device_gpu_fallback PRIVATE pipslib)

add_test(NAME test_device_gpu_fallback COMMAND test_device_gpu_fallback)

add_executable(test_value_methods test_value_methods.cpp)
target_link_libraries(test_value_methods PRIVATE pipslib)

add_test(NAME test_value_methods COMMAND test_value_methods)
