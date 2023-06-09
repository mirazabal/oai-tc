cmake_minimum_required(VERSION 3.0)

project (queue)
set(CMAKE_BUILD_TYPE Debug)

set (CMAKE_C_FLAGS "-Wall -Wextra -std=gnu11 -g")


set(SANITIZER "NONE" CACHE STRING "Sanitizers")

set_property(CACHE SANITIZER PROPERTY STRINGS "NONE" "ADDRESS" "THREAD")
message(STATUS "Selected SANITIZER TYPE: ${SANITIZER}")

if(SANITIZER STREQUAL "ADDRESS")

add_compile_options("-fno-omit-frame-pointer;-fsanitize=address")
add_link_options("-fsanitize=address")

elseif(SANITIZER STREQUAL  "THREAD" )

add_compile_options("-fsanitize=thread;-g;")
add_link_options("-fsanitize=thread;")

endif()

option(CODE_COVERAGE "Code coverage" OFF)
if(CODE_COVERAGE)
add_compile_options("-fprofile-arcs;-ftest-coverage")
add_link_options("-lgcov;-coverage;")
message("Code Coverage cmd: cd CMakeFiles/tc.dir && lcov --capture --directory . --output-file coverage.info && genhtml coverage.info --output-directory out && cd out && firefox index.html")
endif()

option(CODE_PROFILER "Code Profiler" OFF)
if(CODE_PROFILER)
add_compile_options("-pg")
add_link_options("-pg")
message("Code Profiler cmd: gprof tc gmon.out > analysis.txt && vim analysis.txt")
endif()


include_directories(${CMAKE_CURRENT_SOURCE_DIR}/codel/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/fifo/)

add_library(fifo_queue SHARED  fifo/fifo_queue.c ../alg_ds/alg/defer.c)
add_library(codel_queue SHARED codel/codel_queue.c ../alg_ds/alg/defer.c)
add_library(ecn_codel_queue SHARED ecn_codel/ecn_codel_queue.c ../alg_ds/alg/defer.c)

target_link_libraries(codel_queue -lm)
target_link_libraries(ecn_codel_queue -lm)

 

