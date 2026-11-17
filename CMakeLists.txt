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

# Copy runtime assets adjacent to the binary in the build directory
add_custom_command(
    TARGET my_target POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/ProSystem.dat"
        "${CMAKE_CURRENT_SOURCE_DIR}/License.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/gamecontrollerdb.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/shot.png"
        "$<TARGET_FILE_DIR:my_target>/ProSystem.dat"
)