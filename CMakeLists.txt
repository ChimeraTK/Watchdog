PROJECT(watchdog_server)
cmake_minimum_required(VERSION 2.8)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

set(${PROJECT_NAME}_MAJOR_VERSION 02)
set(${PROJECT_NAME}_MINOR_VERSION 05)
set(${PROJECT_NAME}_PATCH_VERSION 00)
include(cmake/set_version_numbers.cmake)

# Moderate version of the compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC -Wextra -ansi -Wuninitialized -O3 -fomit-frame-pointer")

# add constants for HOST and USER
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSER=\\\"`whoami`\\\" -DHOST=\\\"`hostname`\\\"")

#use -DCMAKE_BUILD_TYPE=Debug in your cmake command to turn on the coverage option
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 --coverage")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O3 --coverage")

# linker flags
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--export-dynamic -Wl,--no-as-needed")

include(cmake/add_dependency.cmake)
add_dependency(ChimeraTK-ApplicationCore 00.05 REQUIRED)

IF(ADAPTER STREQUAL "OPCUA")
  message("Building against the OPC UA ControlSystemAdater")
  add_dependency(ControlSystemAdapterOPCUAAdapter REQUIRED)
ELSEIF(ADAPTER STREQUAL "DOOCS")
  message("Building against the DOOCS ControlSystemAdater")
  add_dependency(ChimeraTK-ControlSystemAdapter-DoocsAdapter REQUIRED)
ELSE()
  message(FATAL_ERROR "Please select your ControlSystemAdapter to use by passing to the cmake command line:\n"
                      "   -DADAPTER=DOOCS to use DOOCS\n"
                      "   -DADAPTER=OPCUA to use OPC-UA")
ENDIF()

#FIND_PACKAGE(Boost COMPONENTS thread system filesystem unit_test_framework REQUIRED)
#ENABLE_TESTING()

# extend compiler flags with flags from dependencies
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ChimeraTK-ControlSystemAdapter-DoocsAdapter_CXX_FLAGS} ${ControlSystemAdapterOPCUAAdapter_CXX_FLAGS} ${ChimeraTK-ApplicationCore_CXX_FLAGS}" )

set(CMAKE_LINK_FLAGS "-Wl,--no-as-needed ${ChimeraTK-ControlSystemAdapter-DoocsAdapter_LINK_FLAGS} ${ControlSystemAdapterOPCUAAdapter_LINK_FLAGS} ${ChimeraTK-ApplicationCore_LINK_FLAGS}")

# set include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
set(${PROJECT_NAME}_INCLUDE_DIRS ${${PROJECT_NAME}_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include/)

# create lists with source files
aux_source_directory(${CMAKE_SOURCE_DIR}/src library_sources)
set(server_sources ${CMAKE_SOURCE_DIR}/src/applicationInstance.cc)

# configure version.h
configure_file(include/version.h.in ${PROJECT_BINARY_DIR}/version/version.h)
include_directories(${PROJECT_BINARY_DIR}/version)

# keep the rpath in the installed executable and library to correctly find the dependencies
if("${CMAKE_INSTALL_PREFIX}" STREQUAL "/export/doocs/server")
  SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}/lib")
else()
  SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
endif()
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# server library (to avoid compiling everyting multiple times)
add_library(${PROJECT_NAME}lib SHARED ${library_sources} )
set_target_properties(${PROJECT_NAME}lib PROPERTIES VERSION ${${PROJECT_NAME}_FULL_LIBRARY_VERSION}
                                                    SOVERSION ${${PROJECT_NAME}_SOVERSION})
target_link_libraries(${PROJECT_NAME}lib ${ChimeraTK-ControlSystemAdapter-DoocsAdapter_LIBRARIES}
                                         ${ControlSystemAdapterOPCUAAdapter_LIBRARIES} ${Boost_LIBRARIES}
                                         ${ChimeraTK-ApplicationCore_LIBRARIES})

# main server executable
add_executable(${PROJECT_NAME} ${server_sources})
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}lib)

# XML file generation
add_executable(${PROJECT_NAME}-xmlGenerator ${server_sources})
set_target_properties(${PROJECT_NAME}-xmlGenerator PROPERTIES COMPILE_FLAGS "-DGENERATE_XML")
target_link_libraries(${PROJECT_NAME}-xmlGenerator ${PROJECT_NAME}lib)

add_custom_target(${PROJECT_NAME}-xml ALL DEPENDS ${PROJECT_NAME}-xmlGenerator COMMAND ${PROJECT_BINARY_DIR}/${PROJECT_NAME}-xmlGenerator WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

# Create the executable for automated unit testing.
# add the test executable with all test source files
aux_source_directory(${CMAKE_SOURCE_DIR}/test test_sources)
include_directories(${CMAKE_SOURCE_DIR}/test)
add_executable(test_${PROJECT_NAME} ${server_sources} ${test_sources})
target_link_libraries(test_${PROJECT_NAME} ${PROJECT_NAME}lib)
add_test(test_${PROJECT_NAME} test_${PROJECT_NAME})

# The make coverage command is only available in debug mode
IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
  configure_file(cmake/Makefile.coverage.in ${PROJECT_BINARY_DIR}/Makefile.coverage @ONLY)
  add_custom_target(coverage
    make -f Makefile.coverage
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating test coverage documentation" VERBATIM
  )
ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")

# copy config file for running in virtuallab environment
FILE( COPY ${CMAKE_SOURCE_DIR}/serverTypes/${TYPE}/configs/devMapFile.dmap DESTINATION ${PROJECT_BINARY_DIR})
FILE( COPY ${CMAKE_SOURCE_DIR}/serverTypes/${TYPE}/configs/dummy_sicav.map DESTINATION ${PROJECT_BINARY_DIR})
FILE( COPY ${CMAKE_SOURCE_DIR}/serverTypes/${TYPE}/configs/${PROJECT_NAME}.xlmap DESTINATION ${PROJECT_BINARY_DIR})
IF(ADAPTER STREQUAL "OPCUA")
  FILE( COPY ${CMAKE_SOURCE_DIR}/serverTypes/${TYPE}/configs/LLRFServer_mapping.xml DESTINATION ${PROJECT_BINARY_DIR})
  FILE( COPY ${CMAKE_SOURCE_DIR}/serverTypes/${TYPE}/configs/LLRFServer.persist DESTINATION ${PROJECT_BINARY_DIR})
ELSEIF(ADAPTER STREQUAL "DOOCS")
  FILE( COPY ${CMAKE_SOURCE_DIR}/serverTypes/${TYPE}/configs/${PROJECT_NAME}.conf DESTINATION ${PROJECT_BINARY_DIR})
ENDIF()
FILE( COPY ${CMAKE_SOURCE_DIR}/serverTypes/${TYPE}/initHardware.py DESTINATION ${PROJECT_BINARY_DIR})

# Install the executable and the RPC_LIBNO file
if("${CMAKE_INSTALL_PREFIX}" STREQUAL "/export/doocs/server")
  install(TARGETS ${PROJECT_NAME}lib LIBRARY DESTINATION llrfCtrl_server/lib)
  install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION llrfCtrl_server/bin)
  install(FILES RPC_LIBNO DESTINATION llrfCtrl_server)
else()
  install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
  install(TARGETS ${PROJECT_NAME}lib LIBRARY DESTINATION lib)
  # the RPC_LIBNO file is not installed when not following the DOOCS directory scheme
endif()


