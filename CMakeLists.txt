cmake_minimum_required(VERSION 3.0)
project(vox2obj)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -std=c++11")
    if(NOT WIN32)
        set(GLAD_LIBRARIES dl)
    endif()
endif()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

include_directories(${CMAKE_SOURCE_DIR})

file(GLOB PROJECT_HEADERS ${CMAKE_SOURCE_DIR}/*.h)
file(GLOB PROJECT_SOURCES ${CMAKE_SOURCE_DIR}/*.cpp)
file(GLOB PROJECT_CONFIGS ${CMAKE_SOURCE_DIR}/CMakeLists.txt
                          ${CMAKE_SOURCE_DIR}/Readme.md
                         ${CMAKE_SOURCE_DIR}/.gitignore)

source_group("Headers" FILES ${PROJECT_HEADERS})
source_group("Sources" FILES ${PROJECT_SOURCES})

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES} ${PROJECT_HEADERS})

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME})
