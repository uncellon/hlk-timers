cmake_minimum_required(VERSION 3.16)
project(HlkTimers VERSION 2.1.2)

option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_TESTS "Build tests" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pthread")

find_package(HlkEvents REQUIRED)
find_package(HlkPool REQUIRED)

file(GLOB HEADERS include/hlk/timers/*.h)
file(GLOB SOURCES include/hlk/timers/*.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(Hlk::Timers ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PUBLIC Hlk::Events Hlk::Pool rt)

set_target_properties(
    ${PROJECT_NAME} PROPERTIES
        PUBLIC_HEADER "${HEADERS}"
        EXPORT_NAME Timers
)

target_include_directories(
    ${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include/hlk/timers>
)

install(
    TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    PUBLIC_HEADER
        DESTINATION "include/hlk/timers"
    LIBRARY
        DESTINATION "lib"
)

install(
    EXPORT ${PROJECT_NAME}Targets
    NAMESPACE Hlk::
    FILE "${PROJECT_NAME}Targets.cmake"
    DESTINATION "lib/cmake/${PROJECT_NAME}"
)

# Generate local "${PROJECT_NAME}Targets.cmake"
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake)
export(
    EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    INSTALL_DESTINATION "lib/cmake/${PROJECT_NAME}"
)

write_basic_package_version_file(
    ${PROJECT_NAME}ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
        ${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    DESTINATION "lib/cmake/${PROJECT_NAME}"
)

if(BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    add_test(NAME CascadeTimerCall COMMAND CascadeTimerCallTest)
    add_test(NAME CallingFromAnotherThread COMMAND CallingFromAnotherThreadTest)
    add_test(NAME RecallOneShotTimer COMMAND RecallOneShotTimerTest)
    add_test(NAME RestartTimerBeforeTimeout COMMAND RestartTimerBeforeTimeoutTest)
    add_test(NAME StopFromTimer COMMAND StopFromTimerTest)
    add_test(NAME StopTimer COMMAND StopTimerTest)
    add_test(NAME Concurrence COMMAND ConcurrenceTest)
endif()
