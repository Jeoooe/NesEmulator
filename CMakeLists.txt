cmake_minimum_required(VERSION 3.10.0)
project(nes VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(sources 
src/main.cpp 
src/app.cpp
src/ppu.cpp
src/cartridge.cpp 
src/bus.cpp
src/cpu.cpp
src/mapper.cpp
src/timing.cpp
src/window.cpp
src/controller.cpp
)

set(headers 
src/include/app.h
src/include/ppu.h
src/include/cartridge.h
src/include/bus.h
src/include/cpu.h
src/include/mapper.h
src/include/types.h
)

find_package(SDL3 REQUIRED CONFIG)

# set(sources ${sources} src/test/check_log.hpp)

add_executable(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PRIVATE src/include)


#Main sources
target_sources(${PROJECT_NAME} PRIVATE ${sources} ${headers})
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)


target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
endif()