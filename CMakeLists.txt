cmake_minimum_required(VERSION 3.10)
project(mwe_glfw)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(${PROJECT_NAME} ${SOURCES})

 add_custom_target(copy_resources ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PROJECT_SOURCE_DIR}/assets
        ${PROJECT_BINARY_DIR}/assets
        COMMENT "Copying resources into binary directory")

add_dependencies(${PROJECT_NAME} copy_resources)

find_package(glad)
find_package(glfw3)
find_package(spdlog)
target_link_libraries(${PROJECT_NAME} glad::glad glfw spdlog::spdlog)
