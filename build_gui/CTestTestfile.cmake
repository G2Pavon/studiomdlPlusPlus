# CMake generated Testfile for 
# Source directory: D:/MDL
# Build directory: D:/MDL/build_gui
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(mdltool_tests "D:/MDL/build_gui/bin/Debug/mdltool_tests.exe")
  set_tests_properties(mdltool_tests PROPERTIES  _BACKTRACE_TRIPLES "D:/MDL/CMakeLists.txt;130;add_test;D:/MDL/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(mdltool_tests "D:/MDL/build_gui/bin/Release/mdltool_tests.exe")
  set_tests_properties(mdltool_tests PROPERTIES  _BACKTRACE_TRIPLES "D:/MDL/CMakeLists.txt;130;add_test;D:/MDL/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test(mdltool_tests "D:/MDL/build_gui/bin/MinSizeRel/mdltool_tests.exe")
  set_tests_properties(mdltool_tests PROPERTIES  _BACKTRACE_TRIPLES "D:/MDL/CMakeLists.txt;130;add_test;D:/MDL/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test(mdltool_tests "D:/MDL/build_gui/bin/RelWithDebInfo/mdltool_tests.exe")
  set_tests_properties(mdltool_tests PROPERTIES  _BACKTRACE_TRIPLES "D:/MDL/CMakeLists.txt;130;add_test;D:/MDL/CMakeLists.txt;0;")
else()
  add_test(mdltool_tests NOT_AVAILABLE)
endif()
subdirs("_deps/glfw-build")
subdirs("_deps/nfd-build")
