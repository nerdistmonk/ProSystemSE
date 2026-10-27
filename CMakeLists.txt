cmake_minimum_required(VERSION 3.16)
project(ProSystemLinux CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MINIZIP REQUIRED minizip)

# In CMakeLists.txt:
file(GLOB CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Core/*.cpp")
file(GLOB LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Lib/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/Lib/*.cpp")
file(GLOB SDL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/SDL/*.cpp")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib
    ${CMAKE_CURRENT_SOURCE_DIR}/SDL
    ${MINIZIP_INCLUDE_DIRS}
)

add_executable(prosystem
    ${CORE_SOURCES}
    ${LIB_SOURCES}
    ${SDL_SOURCES}
)

target_link_libraries(prosystem PRIVATE
    SDL3::SDL3
    ZLIB::ZLIB
    ${MINIZIP_LIBRARIES}
)
