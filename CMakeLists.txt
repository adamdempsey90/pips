
cmake_minimum_required(VERSION 3.15)
project(pipslib VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(pipslib INTERFACE)

target_include_directories(pipslib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/pipslib>
)

# Install headers
install(DIRECTORY pips/ DESTINATION include/pipslib)

# Install the interface target
install(TARGETS pipslib EXPORT pipslibTargets)

# Export the targets for find_package
install(EXPORT pipslibTargets
    FILE pipslibTargets.cmake
    NAMESPACE pipslib::
    DESTINATION lib/cmake/pipslib
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/pipslibConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pipslibConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/pipslibConfig.cmake"
    INSTALL_DESTINATION lib/cmake/pipslib
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/pipslibConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/pipslibConfigVersion.cmake"
    DESTINATION lib/cmake/pipslib
)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    add_subdirectory(repl)
endif()