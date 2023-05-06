cmake_minimum_required(VERSION 3.0)

project(tc VERSION 0.0.1 LANGUAGES C)

set(default_build_type "Debug")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")

# Create YouCompleteMe json files
SET( CMAKE_EXPORT_COMPILE_COMMANDS ON )
IF( EXISTS "${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json" )
  EXECUTE_PROCESS( COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json
    ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json
  )
ENDIF()

set(SANITIZER "NONE" CACHE STRING "Sanitizers")
#set(SANITIZER "ADDRESS" CACHE STRING "Sanitizers")

set_property(CACHE SANITIZER PROPERTY STRINGS "NONE" "ADDRESS" "THREAD")
message(STATUS "Selected SANITIZER TYPE: ${SANITIZER}")

if(SANITIZER STREQUAL "ADDRESS")
  add_compile_options("-fno-omit-frame-pointer;-fsanitize=address;-g;-flto")
  add_link_options("-fsanitize=address")
elseif(SANITIZER STREQUAL  "THREAD" )
  add_compile_options("-fsanitize=thread;-g;-flto")
  add_link_options("-fsanitize=thread;")

elseif(SANITIZER STREQUAL "NONE" )
  #  add_compile_options("-Wall; -Wextra; -g;")
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


include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(test_tc_rf  
  alg_ds/alg/accumulate.c 
  alg_ds/alg/defer.c 
  alg_ds/alg/find.c
  alg_ds/alg/for_each.c
  alg_ds/alg/murmur_hash_32.c
  alg_ds/alg/lower_bound.c
  alg_ds/alg/eq_float.c

  alg_ds/ds/seq_container/seq_arr.c
  alg_ds/ds/seq_container/seq_ring.c
  alg_ds/ds/seq_container/seq_list.c
  alg_ds/ds/seq_container/seq_deque.c

  alg_ds/ds/assoc_container/assoc_rb_tree.c
  alg_ds/ds/assoc_container/assoc_rb_tree_lck.c
  alg_ds/ds/assoc_container/assoc_ht_open_address.c
  alg_ds/ds/assoc_container/bimap.c
  alg_ds/meter/meter.c
  alg_ds/ds/statistics/moving_average/mv_avg_time.c
  alg_ds/ds/statistics/moving_average/mv_avg_elm.c
  main.c 
  tc_api.c
  tc.c 
  pkt.c
  time/time.c
  plc_shp_pair.c
  shp/shp.c
  plc/plc.c
  tc_sm/ie/tc_data_ie.c
  )
  
target_link_libraries(test_tc_rf -lpthread -ldl -lm )


target_compile_definitions(test_tc_rf PRIVATE TC_CURRENT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(test_tc_rf PRIVATE TEST_TC_RF)

#"/home/mir/workspace/oai-tc/openair2/tc/sch/build/librr_sch.so")
#" "${CMAKE_CURRENT_SOURCE_DIR})

message(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(sch)
add_subdirectory(cls)
add_subdirectory(pcr)
add_subdirectory(queue)


