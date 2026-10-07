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

add_executable(relative_time_test relative_time_test.cpp)
target_compile_features(relative_time_test PRIVATE cxx_std_17)
target_include_directories(relative_time_test PRIVATE ../.. ../../src)
target_compile_options(relative_time_test PRIVATE -Wall -Wextra -Werror
  -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(relative_time_test PRIVATE -fsanitize=address,undefined)
add_test(NAME relative_time_test COMMAND relative_time_test)

add_executable(home_nav_test home_nav_test.cpp)
target_compile_features(home_nav_test PRIVATE cxx_std_17)
target_include_directories(home_nav_test PRIVATE ../.. ../../src)
target_compile_options(home_nav_test PRIVATE -Wall -Wextra -Werror
  -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(home_nav_test PRIVATE -fsanitize=address,undefined)
add_test(NAME home_nav_test COMMAND home_nav_test)

add_executable(title_clean_test title_clean_test.cpp)
target_compile_features(title_clean_test PRIVATE cxx_std_17)
target_include_directories(title_clean_test PRIVATE ../.. ../../src)
target_compile_options(title_clean_test PRIVATE -Wall -Wextra -Werror
  -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(title_clean_test PRIVATE -fsanitize=address,undefined)
add_test(NAME title_clean_test COMMAND title_clean_test)

add_executable(library_view_test library_view_test.cpp)
target_compile_features(library_view_test PRIVATE cxx_std_17)
target_include_directories(library_view_test PRIVATE ../.. ../../src)
target_compile_options(library_view_test PRIVATE -Wall -Wextra -Werror
  -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(library_view_test PRIVATE -fsanitize=address,undefined)
add_test(NAME library_view_test COMMAND library_view_test)

add_executable(launch_notice_test launch_notice_test.cpp)
target_compile_features(launch_notice_test PRIVATE cxx_std_17)
target_include_directories(launch_notice_test PRIVATE ../.. ../../src)
target_compile_options(launch_notice_test PRIVATE -Wall -Wextra -Werror
  -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(launch_notice_test PRIVATE -fsanitize=address,undefined)
add_test(NAME launch_notice_test COMMAND launch_notice_test)

add_executable(core_options_test core_options_test.cpp)
target_compile_features(core_options_test PRIVATE cxx_std_17)
target_include_directories(core_options_test PRIVATE ../.. ../../src)
target_compile_options(core_options_test PRIVATE -Wall -Wextra -Werror
  -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(core_options_test PRIVATE -fsanitize=address,undefined)
add_test(NAME core_options_test COMMAND core_options_test)
