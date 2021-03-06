enable_testing()

# See if we have google benchmark
include(CheckCXXSymbolExists)
set(CMAKE_REQUIRED_LIBRARIES benchmark)
check_cxx_symbol_exists(benchmark::Initialize benchmark/benchmark.h
                        HAVE_BENCHMARK_H)
cmake_reset_check_state()

if(NOT HAVE_BENCHMARK_H AND NOT $ENV{CONTINUOUS_INTEGRATION})
  include(ExternalProject)

  externalproject_add(google-benchmark
    UPDATE_COMMAND ""
    GIT_REPOSITORY https://github.com/google/benchmark
    GIT_TAG v1.1.0
    GIT_SHALLOW 1
    CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
      -DCMAKE_BUILD_TYPE=Release
      -DBENCHMARK_ENABLE_LTO=true
    BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/lib/libbenchmark.a
  )

  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include ${CMAKE_BINARY_DIR}/lib)

  add_library(benchmark STATIC IMPORTED GLOBAL)

  set_target_properties(benchmark PROPERTIES
    IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/lib/libbenchmark.a
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR}/include
    POSITION_INDEPENDENT_CODE ON
    INTERPROCEDURAL_OPTIMIZATION ${IPO}
  )

  add_dependencies(benchmark google-benchmark)
  set(HAVE_BENCHMARK_H 1)
endif()

include_directories(${CMAKE_SOURCE_DIR}/lib/src)

if(HAVE_BENCHMARK_H)
  add_executable(bench_sock bench.cc common.c ${CMAKE_SOURCE_DIR}/lib/src/in_cksum.c)
  target_link_libraries(bench_sock PUBLIC benchmark sock_dyn)
  set_target_properties(bench_sock
    PROPERTIES
      POSITION_INDEPENDENT_CODE ON
      INTERPROCEDURAL_OPTIMIZATION ${IPO}
  )
  add_test(bench_sock bench_sock)

  if(HAVE_NETMAP_H)
    add_executable(bench_warp bench.cc common.c ${CMAKE_SOURCE_DIR}/lib/src/in_cksum.c)
    target_compile_definitions(bench_warp PRIVATE -DWITH_NETMAP)
    target_link_libraries(bench_warp PUBLIC benchmark warp_dyn)
    set_target_properties(bench_warp
      PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        INTERPROCEDURAL_OPTIMIZATION ${IPO}
    )
    add_test(bench_warp bench_warp)
  endif()
endif()


add_executable(test_sock common.c test_sock.c)
target_link_libraries(test_sock PUBLIC sock_dyn)
set_target_properties(test_sock
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    INTERPROCEDURAL_OPTIMIZATION ${IPO}
)
add_test(test_sock test_sock)

add_executable(test_iov common.c test_iov.c)
target_link_libraries(test_iov PUBLIC sock_dyn)
set_target_properties(test_iov
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    INTERPROCEDURAL_OPTIMIZATION ${IPO}
)
add_test(test_iov test_iov)

if(HAVE_NETMAP_H)
  add_executable(test_warp common.c test_sock.c)
  target_compile_definitions(test_warp PRIVATE -DWITH_NETMAP)
  target_link_libraries(test_warp PUBLIC warp_dyn)
  set_target_properties(test_warp
    PROPERTIES
      POSITION_INDEPENDENT_CODE ON
      INTERPROCEDURAL_OPTIMIZATION ${IPO}
  )
  add_test(test_warp test_warp)
endif()

add_executable(test_hexdump common.c test_hexdump.c)
target_link_libraries(test_hexdump PUBLIC sock_dyn)
set_target_properties(test_hexdump
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    INTERPROCEDURAL_OPTIMIZATION ${IPO}
)
add_test(test_hexdump test_hexdump)

add_executable(test_queue common.c test_queue.c)
target_link_libraries(test_queue PUBLIC sock_dyn)
set_target_properties(test_queue
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    INTERPROCEDURAL_OPTIMIZATION ${IPO}
)
add_test(test_queue test_queue)
