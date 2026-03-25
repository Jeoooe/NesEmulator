cmake_minimum_required(VERSION 3.10.0)
project(nes VERSION 0.1.0 LANGUAGES C CXX)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    message("Target platform: Linux")
elseif (CMAKE_SYSTEM_NAME MATCHES "Windows")
    message("Target platform: Windows")
    set(CMAKE_PREFIX_PATH "dfafjasdf")
else()
    message("Target platform: Unknown")
endif()

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
src/apu.cpp
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
find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3-static)
find_library(SDL3_Win 
    NAMES libSDL3.dll.a PATHS /usr/x86_64-w64-mingw32/lib/SDL REQUIRED)
message("SDL3_Win: ${SDL3_Win}")

add_executable(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PRIVATE src/include)


#Main sources
target_sources(${PROJECT_NAME} PRIVATE ${sources} ${headers})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3-static)  
    elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
        add_library(SDL3_Win_Lib STATIC IMPORTED)
        set_target_properties(SDL3_Win_Lib PROPERTIES
            IMPORTED_LOCATION "${SDL3_Win}"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/windows-toolchain/include"
        )
        target_link_libraries(${PROJECT_NAME} PRIVATE SDL3_Win_Lib)
    endif() 
endif()


target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
endif()