
cmake_minimum_required(VERSION 3.15)
project(pipslib VERSION 1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(pipslib_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/pipslib")

add_library(pipslib INTERFACE)

target_include_directories(pipslib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Install headers
install(DIRECTORY pips DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Install the interface target
install(TARGETS pipslib EXPORT pipslibTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export the targets for find_package
install(EXPORT pipslibTargets
    FILE pipslibTargets.cmake
    NAMESPACE pipslib::
    DESTINATION ${pipslib_INSTALL_CMAKEDIR}
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/pipslibConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pipslibConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/pipslibConfig.cmake"
    INSTALL_DESTINATION ${pipslib_INSTALL_CMAKEDIR}
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/pipslibConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/pipslibConfigVersion.cmake"
    DESTINATION ${pipslib_INSTALL_CMAKEDIR}
)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(repl)
    add_subdirectory(tests)
    add_subdirectory(applications)
endif()