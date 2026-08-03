cmake_minimum_required(VERSION 3.16)
project(retrosuite_host_tests LANGUAGES CXX)
enable_testing()

add_executable(bounds_test bounds_test.cpp)
target_compile_features(bounds_test PRIVATE cxx_std_17)
target_include_directories(bounds_test PRIVATE ../..)
target_compile_options(bounds_test PRIVATE -Wall -Wextra -Werror
  -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(bounds_test PRIVATE -fsanitize=address,undefined)
add_test(NAME bounds_test COMMAND bounds_test)

add_executable(frame_probe_test frame_probe_test.cpp)
target_compile_features(frame_probe_test PRIVATE cxx_std_17)
target_include_directories(frame_probe_test PRIVATE ../.. ../../src)
target_compile_options(frame_probe_test PRIVATE -Wall -Wextra -Werror
  -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(frame_probe_test PRIVATE -fsanitize=address,undefined)
add_test(NAME frame_probe_test COMMAND frame_probe_test)

add_executable(grid_window_test grid_window_test.cpp)
target_compile_features(grid_window_test PRIVATE cxx_std_17)
target_include_directories(grid_window_test PRIVATE ../..)
target_compile_options(grid_window_test PRIVATE -Wall -Wextra -Werror
  -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(grid_window_test PRIVATE -fsanitize=address,undefined)
add_test(NAME grid_window_test COMMAND grid_window_test)
