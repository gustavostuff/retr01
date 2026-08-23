# CMake generated Testfile for 
# Source directory: /home/g/Repos/retr01/studio
# Build directory: /home/g/Repos/retr01/studio/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(core "/home/g/Repos/retr01/studio/build/test_core")
set_tests_properties(core PROPERTIES  WORKING_DIRECTORY "/home/g/Repos/retr01/studio/build" _BACKTRACE_TRIPLES "/home/g/Repos/retr01/studio/CMakeLists.txt;30;add_test;/home/g/Repos/retr01/studio/CMakeLists.txt;0;")
add_test(e2e "/home/g/Repos/retr01/studio/build/test_e2e")
set_tests_properties(e2e PROPERTIES  ENVIRONMENT "SDL_VIDEODRIVER=offscreen" WORKING_DIRECTORY "/home/g/Repos/retr01/studio/build" _BACKTRACE_TRIPLES "/home/g/Repos/retr01/studio/CMakeLists.txt;64;add_test;/home/g/Repos/retr01/studio/CMakeLists.txt;0;")
