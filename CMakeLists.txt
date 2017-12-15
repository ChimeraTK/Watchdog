PROJECT(ChimeraTK-WatchdogServer)
cmake_minimum_required(VERSION 2.8)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

set(${PROJECT_NAME}_MAJOR_VERSION 00)
set(${PROJECT_NAME}_MINOR_VERSION 03)
set(${PROJECT_NAME}_PATCH_VERSION 00)
include(cmake/set_version_numbers.cmake)

# Moderate version of the compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC -Wextra -ansi -Wuninitialized -O3 -fomit-frame-pointer")

# add constants for HOST and USER
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSER=\\\"`whoami`\\\" -DHOST=\\\"`hostname`\\\"")

#use -DCMAKE_BUILD_TYPE=Debug in your cmake command to turn on the coverage option
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 --coverage -DDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 --coverage")
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O3 --coverage")

# linker flags
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--export-dynamic -Wl,--no-as-needed")

include(cmake/add_dependency.cmake)
add_dependency(ChimeraTK-ApplicationCore 00.05 REQUIRED)

find_library(procps NAMES procps REQUIRED)

IF(ADAPTER STREQUAL "OPCUA")
  message("Building against the OPC UA ControlSystemAdater")
  add_dependency(ChimeraTK-ControlSystemAdapter-OPCUAAdapter REQUIRED)
ELSEIF(ADAPTER STREQUAL "DOOCS")
  message("Building against the DOOCS ControlSystemAdater")
  add_dependency(ChimeraTK-ControlSystemAdapter-DoocsAdapter REQUIRED)
ELSE()
  message(FATAL_ERROR "Please select your ControlSystemAdapter to use by passing to the cmake command line:\n"
                      "   -DADAPTER=DOOCS to use DOOCS\n"
                      "   -DADAPTER=OPCUA to use OPC-UA")
ENDIF()

##################RPATH handling ##################################
#RPATH handling
# use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


# the RPATH to be used when installing, but only if it's not a system directory
LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
IF("${isSystemDir}" STREQUAL "-1")
  if("${CMAKE_INSTALL_PREFIX}" STREQUAL "/export/doocs/server")
    SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}/lib")
  else()
    SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
  endif()
ENDIF("${isSystemDir}" STREQUAL "-1")

##################FIX THIS#########################################
#check boost version that includes boost.process
FIND_PACKAGE(Boost 1.64 COMPONENTS thread system filesystem unit_test_framework)

IF(Boost_FOUND)
  MESSAGE(STATUS "Found boost version that includes boost process (>1.64).")
  add_definitions( "-DBOOST_1_64" )
ELSE()
  MESSAGE(STATUS "Found no boost version that includes boost process. Will use fork/exec.")
  FIND_PACKAGE(Boost COMPONENTS thread system filesystem unit_test_framework)
ENDIF(Boost_FOUND)
ENABLE_TESTING()
####################QUICK FIX#########################################

FIND_PACKAGE(PkgConfig REQUIRED)
set(LIBXML++_VERSION "libxml++-2.6")
PKG_CHECK_MODULES(LibXML++ REQUIRED ${LIBXML++_VERSION})
PKG_CHECK_MODULES(glib REQUIRED glib-2.0)
include_directories(SYSTEM "${LibXML++_INCLUDEDIR}/${LIBXML++_VERSION}")
include_directories(${LibXML++_INCLUDE_DIRS})

# set include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
set(${PROJECT_NAME}_INCLUDE_DIRS ${${PROJECT_NAME}_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include/)

# create lists with source files
aux_source_directory(${CMAKE_SOURCE_DIR}/src library_sources)
list(REMOVE_ITEM library_sources ${CMAKE_SOURCE_DIR}/src/applicationInstance.cc)
set(server_sources ${CMAKE_SOURCE_DIR}/src/applicationInstance.cc)

# configure version.h
configure_file(include/version.h.in ${PROJECT_BINARY_DIR}/version/version.h)
include_directories(${PROJECT_BINARY_DIR}/version)

# server library (to avoid compiling everyting multiple times)
add_library(${PROJECT_NAME}lib SHARED ${library_sources} )
set_target_properties(${PROJECT_NAME}lib PROPERTIES VERSION ${${PROJECT_NAME}_FULL_LIBRARY_VERSION}
                                                    SOVERSION ${${PROJECT_NAME}_SOVERSION})
target_link_libraries(${PROJECT_NAME}lib ${Boost_LIBRARIES}
                                         ${ChimeraTK-ApplicationCore_LIBRARIES}
                                         procps
                                         ${Boost_process_LIBRARIES})

# main server executable
add_executable(${PROJECT_NAME} ${server_sources})
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}lib ${ChimeraTK-ControlSystemAdapter-DoocsAdapter_LIBRARIES} ${ChimeraTK-ControlSystemAdapter-OPCUAAdapter_LIBRARIES})

# XML file generation
add_executable(${PROJECT_NAME}-xmlGenerator ${server_sources})
set_target_properties(${PROJECT_NAME}-xmlGenerator PROPERTIES COMPILE_FLAGS "-DGENERATE_XML")
target_link_libraries(${PROJECT_NAME}-xmlGenerator ${PROJECT_NAME}lib)



# Create the executable for automated unit testing.
add_executable(test_procReader ${CMAKE_SOURCE_DIR}/test/test_procReader.cc)
target_link_libraries(test_procReader ${PROJECT_NAME}lib ${Boost_LIBRARIES})
add_test(test_procReader test_procReader)

# The make coverage command is only available in debug mode
IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_custom_target(${PROJECT_NAME}-xml ALL DEPENDS ${PROJECT_NAME}-xmlGenerator COMMAND ${PROJECT_BINARY_DIR}/${PROJECT_NAME}-xmlGenerator WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
  #configure_file(cmake/Makefile.coverage.in ${PROJECT_BINARY_DIR}/Makefile.coverage @ONLY)
  #add_custom_target(coverage
  #  make -f Makefile.coverage
  #  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  #  COMMENT "Generating test coverage documentation" VERBATIM
  #)
ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")

# copy config file for running in virtuallab environment
FILE( COPY ${CMAKE_SOURCE_DIR}/configs/devMapFile.dmap DESTINATION ${PROJECT_BINARY_DIR})
FILE( COPY ${CMAKE_SOURCE_DIR}/configs/${PROJECT_NAME}.xlmap DESTINATION ${PROJECT_BINARY_DIR})
FILE( COPY ${CMAKE_SOURCE_DIR}/configs/${PROJECT_NAME}_processes.xml DESTINATION ${PROJECT_BINARY_DIR})
IF(ADAPTER STREQUAL "OPCUA")
  FILE( COPY ${CMAKE_SOURCE_DIR}/configs/WatchdogServer_mapping.xml DESTINATION ${PROJECT_BINARY_DIR})
#  FILE( COPY ${CMAKE_SOURCE_DIR}/serverTypes/${TYPE}/configs/LLRFServer.persist DESTINATION ${PROJECT_BINARY_DIR})
ELSEIF(ADAPTER STREQUAL "DOOCS")
  FILE( COPY ${CMAKE_SOURCE_DIR}/configs/${PROJECT_NAME}.conf DESTINATION ${PROJECT_BINARY_DIR})
ENDIF()

SET(CMAKE_INSTALL_PREFIX "/export/ChimeraTK/WatchdogServer/"
CACHE PATH "Install path prefix")

# Install the executable and the RPC_LIBNO file
if("${CMAKE_INSTALL_PREFIX}" STREQUAL "/export/doocs/server")
  install(TARGETS ${PROJECT_NAME}lib LIBRARY DESTINATION ${PROJECT_NAME}/lib)
  install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${PROJECT_NAME}/bin)
  install(FILES ${CMAKE_SOURCE_DIR}/configs/RPC_LIBNO DESTINATION ${PROJECT_NAME})
else()
  install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
  install(TARGETS ${PROJECT_NAME}lib LIBRARY DESTINATION lib)
  # the RPC_LIBNO file is not installed when not following the DOOCS directory scheme
endif()

find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/main.dox.in ${CMAKE_CURRENT_BINARY_DIR}/main.dox @ONLY)
  
  add_custom_target(doc ${DOC_DEPENDENCY}
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
  #note the / after ${CMAKE_BINARY_DIR}/doc/. This causes the directory to be renamed to the destination, not copied into
  install(DIRECTORY ${CMAKE_BINARY_DIR}/doc/ DESTINATION share/doc/${PROJECT_NAME}-${${PROJECT_NAME}_SOVERSION}
          COMPONENT doc
          OPTIONAL)
else(DOXYGEN_FOUND)
  message("Doxygen not found, documentation cannot be build.")
endif(DOXYGEN_FOUND)
