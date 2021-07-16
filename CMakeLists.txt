cmake_minimum_required(VERSION 3.16)
project(HlkTimers VERSION 1.0.3)

option(BUILD_EXAMPLES "Build examples" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pthread")

find_package(HlkEvents REQUIRED)

file(GLOB HEADERS include/hlk/timers/*.h)
file(GLOB SOURCES include/hlk/timers/*.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(Hlk::Timers ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PUBLIC Hlk::Events)

set_target_properties(
    ${PROJECT_NAME} PROPERTIES
        PUBLIC_HEADER "${HEADERS}"
        EXPORT_NAME Timers
)

target_include_directories(
    ${PROJECT_NAME} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include/hlk/timers>
)

install(
    TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    PUBLIC_HEADER
        DESTINATION "include/hlk/timers"
)

install(
    EXPORT ${PROJECT_NAME}Targets
    NAMESPACE Hlk::
    FILE "${PROJECT_NAME}Config.cmake"
    DESTINATION "lib/cmake/${PROJECT_NAME}"
)

# Generate local "${PROJECT_NAME}Config.cmake"
export(
    EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Config.cmake
)
# Since the export(...) is performed after find_package(...), create an empty file to avoid find_package(...) errors
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake)

# Generate config for version supporting
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${PROJECT_NAME}ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Install config for version supporting
install(
    FILES
        ${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    DESTINATION lib/cmake/${PROJECT_NAME}
)

if(BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

if(BUILD_TESTS)
    add_subdirectory(tests)
    enable_testing()
    add_test(NAME CascadeTimerCall COMMAND CascadeTimerCallTest)
    add_test(NAME CallingFromAnotherThread COMMAND CallingFromAnotherThreadTest)
    add_test(NAME RecallOneShotTimer COMMAND RecallOneShotTimerTest)
endif()
