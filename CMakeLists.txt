cmake_minimum_required(VERSION 3.16)

project(
  "AgISOVirtualTerminal"
  LANGUAGES CXX C
  VERSION 1.4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CAN_Stack MODULE)
find_package(git_version MODULE)
find_package(Threads REQUIRED)

set(SOURCE_FILES
    src/main_headless.cpp
    src/HeadlessVT.cpp
    src/DrawingAPI.cpp
)

if(APPLE)
  list(APPEND SOURCE_FILES src/BluetoothCANInterface.mm)
  set_source_files_properties(src/BluetoothCANInterface.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
endif()

add_executable(AgISOVirtualTerminalHeadless ${SOURCE_FILES})

target_include_directories(AgISOVirtualTerminalHeadless
                           PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)

target_link_libraries(
  AgISOVirtualTerminalHeadless
  PRIVATE isobus::Isobus isobus::HardwareIntegration isobus::Utility Threads::Threads
  PUBLIC cmake_git_version_tracking)

if(APPLE)
  find_library(COREBLUETOOTH_FRAMEWORK CoreBluetooth)
  find_library(FOUNDATION_FRAMEWORK Foundation)
  target_link_libraries(AgISOVirtualTerminalHeadless PRIVATE ${COREBLUETOOTH_FRAMEWORK} ${FOUNDATION_FRAMEWORK})
endif()

install(TARGETS AgISOVirtualTerminalHeadless RUNTIME DESTINATION bin)
