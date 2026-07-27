cmake_minimum_required(VERSION 3.20)
project(mxutil LANGUAGES CXX)

add_executable(mxutil
  src/main.cpp
  src/driver.cpp
  src/inject.cpp
)

target_include_directories(mxutil PRIVATE include)
target_compile_features(mxutil PRIVATE cxx_std_17)
target_compile_definitions(mxutil PRIVATE UNICODE _UNICODE)
target_link_libraries(mxutil PRIVATE advapi32 user32 crypt32 ntdll)

add_custom_command(TARGET mxutil POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/mxdrv.sys
          $<TARGET_FILE_DIR:mxutil>/mxdrv.sys
)
