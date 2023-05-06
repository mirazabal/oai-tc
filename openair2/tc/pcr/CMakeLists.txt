cmake_minimum_required(VERSION 3.0)

project (pcr)
set(CMAKE_BUILD_TYPE Debug)

set (CMAKE_C_FLAGS "-Wall -Wextra -std=gnu11 -g")

set(SANITIZER "NONE" CACHE STRING "Sanitizers")
set_property(CACHE SANITIZER PROPERTY STRINGS "NONE" "ADDRESS" "THREAD")
message(STATUS "Selected SANITIZER TYPE: ${SANITIZER}")

if(SANITIZER STREQUAL "ADDRESS")

add_compile_options("-fno-omit-frame-pointer;-fsanitize=address;-g")
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
if( CODE_PROFILER )
add_compile_options("-pg")
add_link_options("-pg")
message("Code Profiler cmd: gprof tc gmon.out > analysis.txt && vim analysis.txt  ")
endif()





#option(USE_CLANG "build application with clang" OFF) # OFF is the default

#if(USE_CLANG)
#set(CMAKE_C_COMPILER clang)
#set (CMAKE_C_FLAGS "-Wall -fblocks -std=c11")
#else()
#set (CMAKE_C_FLAGS "-Wall -std=c11")
#endif(USE_CLANG)


include_directories(${CMAKE_CURRENT_SOURCE_DIR}/bdp/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dummy/)

add_library(dummy_pcr SHARED dummy/dummy_pcr.c)
add_library(bdp_pcr SHARED bdp/bdp_pcr.c 
                            ../alg_ds/alg/defer.c 
                            ../alg_ds/alg/rotate.c
                            ../alg_ds/alg/iter_swap.c)

#if(USE_CLANG)
#target_link_libraries(bdp_pcr -lBlocksRuntime)
#endif(USE_CLANG)

# If you are missing the blocks in your machine 
# sudo apt-get install libblocksruntime-dev

