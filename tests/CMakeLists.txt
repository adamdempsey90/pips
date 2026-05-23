add_executable(test_device test_device.cpp)
target_link_libraries(test_device PRIVATE pipslib)

add_test(NAME test_device COMMAND test_device)
