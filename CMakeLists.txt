#/**********************************************************\ 
# 
# Auto-generated CMakeLists.txt for the Verity Controller Plugin project
#
#\**********************************************************/

# Written to work with cmake 2.6
cmake_minimum_required (VERSION 2.6)
set (CMAKE_BACKWARDS_COMPATIBILITY 2.6)

Project(Attendant)

add_definitions(-D_DEBUG=1)

macro(_create_test TEST)
  add_executable(${TEST} attendant_posix.c errors.c src/${TEST}.c src/t/ok.c)
endmacro()

_create_test(t/attendant/abend.t)
_create_test(t/attendant/cycle.t)
_create_test(t/attendant/retry.t)
_create_test(t/attendant/scram.t)
_create_test(t/attendant/missing-relay.t)
_create_test(t/attendant/missing-server.t)

