#/**********************************************************\ 
# 
# Auto-generated CMakeLists.txt for the Verity Controller Plugin project
#
#\**********************************************************/

# Written to work with cmake 2.6
cmake_minimum_required (VERSION 2.6)
#set (CMAKE_BACKWARDS_COMPATIBILITY 2.6)

Project(Attendant)

if (WIN32)
  add_executable(attendant attendant_win.c main_win.c)
else()
  FILE(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/t/attendant)
  FILE(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/t/relay)
  FILE(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/t/server)

  macro(_create_test TEST)
    add_executable(${TEST} attendant_posix.c errors.c src/${TEST}.c src/t/ok.c ${ARGN})
    set_target_properties(${TEST} PROPERTIES COMPILE_FLAGS "-D_DEBUG=1 -Wall")
  endmacro()

  add_executable(relay relay.c errors.c)

  _create_test(t/relay/fds.t src/t/reset.c)
  _create_test(t/relay/signals.t src/t/reset.c)
  _create_test(t/relay/status.t)

  _create_test(t/server/fds.t)

  _create_test(t/attendant/abend.t)
  _create_test(t/attendant/cycle.t)
  _create_test(t/attendant/retry.t)
  _create_test(t/attendant/scram.t)
  _create_test(t/attendant/missing-relay.t)
  _create_test(t/attendant/missing-server.t)
endif()
