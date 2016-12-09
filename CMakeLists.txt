cmake_minimum_required(VERSION 2.8)
set(CMAKE_BUILD_TYPE Debug)
# set(CMAKE_CXX_COMPILER clang++)
project(midiplay)

# set(CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++")
set(CMAKE_CXX_FLAGS "-std=c++11 ")

find_package(PkgConfig)
pkg_check_modules(LIBALSA REQUIRED alsa)

include_directories(${LIBALSA_INCLUDE_DIRS} src/)

add_executable(main src/app/main.cpp src/app/led.cpp src/midi/midi.cpp src/snd/snd.cpp)
target_link_libraries(main pthread ${LIBALSA_LIBRARIES})
