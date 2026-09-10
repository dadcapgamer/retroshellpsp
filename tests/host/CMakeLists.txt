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

add_executable(natural_order_test natural_order_test.cpp)
target_compile_features(natural_order_test PRIVATE cxx_std_17)
target_include_directories(natural_order_test PRIVATE ../..)
target_compile_options(natural_order_test PRIVATE -Wall -Wextra -Werror
  -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(natural_order_test PRIVATE -fsanitize=address,undefined)
add_test(NAME natural_order_test COMMAND natural_order_test)

add_executable(core_package_policy_test core_package_policy_test.cpp)
target_compile_features(core_package_policy_test PRIVATE cxx_std_17)
target_include_directories(core_package_policy_test PRIVATE ../..)
target_compile_options(core_package_policy_test PRIVATE -Wall -Wextra -Werror
  -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(core_package_policy_test PRIVATE -fsanitize=address,undefined)
add_test(NAME core_package_policy_test COMMAND core_package_policy_test)

add_executable(native_adapter_protocol_test native_adapter_protocol_test.cpp)
target_compile_features(native_adapter_protocol_test PRIVATE cxx_std_17)
target_include_directories(native_adapter_protocol_test PRIVATE ../..)
target_compile_options(native_adapter_protocol_test PRIVATE -Wall -Wextra -Werror
  -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(native_adapter_protocol_test PRIVATE -fsanitize=address,undefined)
add_test(NAME native_adapter_protocol_test COMMAND native_adapter_protocol_test)

add_executable(pause_menu_contract_test pause_menu_contract_test.cpp)
target_compile_features(pause_menu_contract_test PRIVATE cxx_std_17)
target_include_directories(pause_menu_contract_test PRIVATE ../..)
target_compile_options(pause_menu_contract_test PRIVATE -Wall -Wextra -Werror
  -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(pause_menu_contract_test PRIVATE -fsanitize=address,undefined)
add_test(NAME pause_menu_contract_test COMMAND pause_menu_contract_test)
