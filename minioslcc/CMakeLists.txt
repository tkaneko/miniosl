cmake_minimum_required(VERSION 3.22)
project(minioslcc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

#set(CMAKE_CXX_VISIBILITY_PRESET hidden)
add_library(minioslcc20 src/basic-type.cc src/base-state.cc src/state.cc
  src/record.cc src/impl/effect.cc src/impl/more.cc
  src/impl/checkmate.cc src/impl/bitpack.cc src/impl/hash.cc)
target_include_directories(minioslcc20 PRIVATE src)

add_subdirectory(pybind11)
pybind11_add_module(minioslcc src/pyb/miniosl.cc src/pyb/miniosl-state.cc)
target_include_directories(minioslcc PRIVATE src)
target_link_libraries(minioslcc PRIVATE minioslcc20)

option(BUILD_UTILS "build utilities" ON)
if(BUILD_UTILS)
  add_executable(csa-to-sfen util/csa-to-sfen.cc)  
  target_include_directories(csa-to-sfen PRIVATE src)
  target_link_libraries(csa-to-sfen PRIVATE minioslcc20)

  add_executable(validate-sfen util/validate-sfen.cc)  
  target_include_directories(validate-sfen PRIVATE src)
  target_link_libraries(validate-sfen PRIVATE minioslcc20)

  add_executable(compress-sfen util/compress-sfen.cc)  
  target_include_directories(compress-sfen PRIVATE src)
  target_link_libraries(compress-sfen PRIVATE minioslcc20)
endif()

option(BUILD_TEST "build test executable" OFF)

if(BUILD_TEST)
  enable_testing()
  
  add_executable(minitest test/minitest.cc)
  target_include_directories(minitest PRIVATE src)
  target_include_directories(minitest PRIVATE acutest/include)
  target_compile_options(minitest PRIVATE -Wno-deprecated-declarations)
  target_link_libraries(minitest PRIVATE minioslcc20)
  add_test(NAME run_test_mini COMMAND minitest)
  
  add_executable(minitest_file test/minitest_file.cc)
  target_include_directories(minitest_file PRIVATE src)
  target_include_directories(minitest_file PRIVATE acutest/include)
  target_compile_options(minitest_file PRIVATE -Wno-deprecated-declarations)
  target_link_libraries(minitest_file PRIVATE minioslcc20)
  add_test(NAME run_test_file COMMAND minitest_file)
endif()
