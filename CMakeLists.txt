PROJECT(watchdog-server)
cmake_minimum_required(VERSION 2.8)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

set(${PROJECT_NAME}_MAJOR_VERSION 01)
set(${PROJECT_NAME}_MINOR_VERSION 01)
set(${PROJECT_NAME}_PATCH_VERSION 00)
include(cmake/set_version_numbers.cmake)

# Options
option(ENABLE_TESTING      "Build unit tests"      ON)
option(ENABLE_LOGGING      "Enable logging for the watchdog server"      ON)
option(ENABLE_DOCUMENTAION "Enable create of doxygen documentation"      ON)

if( ENABLE_DOCUMENTAION )
  SET( SUPPRESS_AUTO_DOC_BUILD FALSE)
else()
  SET( SUPPRESS_AUTO_DOC_BUILD TRUE)
endif( ENABLE_DOCUMENTAION )

# Moderate version of the compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC -Wextra -ansi -Wuninitialized -O3 -fomit-frame-pointer")

# add constants for HOST and USER
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSER=\\\"`whoami`\\\" -DHOST=\\\"`hostname`\\\"")

#use -DCMAKE_BUILD_TYPE=Debug in your cmake command to turn on the coverage option
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 --coverage")
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O3 --coverage")

# linker flags
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--export-dynamic -Wl,--no-as-needed")

include(cmake/add_dependency.cmake)
add_dependency(ChimeraTK-ApplicationCore 01.04 REQUIRED)

find_library(procps NAMES procps REQUIRED)

if(ADAPTER STREQUAL "OPCUA")
  message("Building against the OPC UA ControlSystemAdater")
  add_dependency(ChimeraTK-ControlSystemAdapter-OPCUAAdapter REQUIRED)
elseif(ADAPTER STREQUAL "DOOCS")
  message("Building against the DOOCS ControlSystemAdater")
  add_dependency(ChimeraTK-ControlSystemAdapter-DoocsAdapter REQUIRED)
else()
  message(FATAL_ERROR "Please select your ControlSystemAdapter to use by passing to the cmake command line:\n"
                      "   -DADAPTER=DOOCS to use DOOCS\n"
                      "   -DADAPTER=OPCUA to use OPC-UA")
endif()

if( ENABLE_TESTING )
  FIND_PACKAGE(Boost COMPONENTS unit_test_framework date_time filesystem)
  ENABLE_TESTING()
endif( ENABLE_TESTING )

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


if(ENABLE_LOGGING)
  MESSAGE(STATUS "Logging modules are enabled.")
  add_definitions(-DENABLE_LOGGING)
else()
  MESSAGE(STATUS "Logging modules are disabled.")
endif(ENABLE_LOGGING)


message("CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
if(CMAKE_BUILD_TYPE MATCHES Debug)
  if( NOT ENABLE_LOGGING )
    list(REMOVE_ITEM library_sources ${CMAKE_CURRENT_SOURCE_DIR}/src/Logging.cc
                                     ${CMAKE_CURRENT_SOURCE_DIR}/src/LoggingModule.cc)
  endif( NOT ENABLE_LOGGING )
  
  add_library(${PROJECT_NAME}lib SHARED ${library_sources} )  
  set_target_properties(${PROJECT_NAME}lib PROPERTIES VERSION ${${PROJECT_NAME}_FULL_LIBRARY_VERSION}
                                                    SOVERSION ${${PROJECT_NAME}_SOVERSION})
  target_link_libraries(${PROJECT_NAME}lib ${Boost_LIBRARIES}
                                           ${ChimeraTK-ApplicationCore_LIBRARIES}
                                           procps
                                           ${Boost_process_LIBRARIES})
  install(TARGETS ${PROJECT_NAME}lib LIBRARY DESTINATION  lib)                                            
  
  # main server executable
  add_executable(${PROJECT_NAME} ${server_sources})
  target_link_libraries(${PROJECT_NAME} ${ChimeraTK-ApplicationCore_LIBRARIES} 
                                        ${ChimeraTK-ControlSystemAdapter-DoocsAdapter_LIBRARIES} 
                                        ${ChimeraTK-ControlSystemAdapter-OPCUAAdapter_LIBRARIES}
                                        ${LibXML++_LIBRARIES} 
                                        procps
                                        ${PROJECT_NAME}lib)
  
  # XML file generation
  add_executable(${PROJECT_NAME}-xmlGenerator ${server_sources})
  set_target_properties(${PROJECT_NAME}-xmlGenerator PROPERTIES COMPILE_FLAGS "-DGENERATE_XML")
  target_link_libraries(${PROJECT_NAME}-xmlGenerator ${ChimeraTK-ApplicationCore_LIBRARIES} 
                                                     ${LibXML++_LIBRARIES} 
                                                     procps
                                                     ${PROJECT_NAME}lib)
  
  if( ENABLE_TESTING )
    # Create the executable for automated unit testing.
    add_executable(test_procReader ${CMAKE_SOURCE_DIR}/test/test_procReader.cc)
    target_link_libraries(test_procReader ${ChimeraTK-ApplicationCore_LIBRARIES} 
                                          ${LibXML++_LIBRARIES} 
                                          procps 
                                          ${Boost_LIBRARIES}
                                          ${PROJECT_NAME}lib)
    add_test(test_procReader test_procReader)
    if( ENABLE_LOGGING )
      add_executable(test_processModule ${CMAKE_SOURCE_DIR}/test/test_processModule.cc)
      target_link_libraries(test_processModule ${ChimeraTK-ApplicationCore_LIBRARIES} 
                                            ${LibXML++_LIBRARIES} 
                                            procps 
                                            ${Boost_LIBRARIES}
                                            ${PROJECT_NAME}lib)
      add_test(test_processModule test_processModule)
    
      add_executable(test_loggingModule ${CMAKE_SOURCE_DIR}/test/test_loggingModule.cc)
      target_link_libraries(test_loggingModule ${ChimeraTK-ApplicationCore_LIBRARIES} 
                                            ${LibXML++_LIBRARIES} 
                                            procps 
                                            ${Boost_LIBRARIES}
                                            ${PROJECT_NAME}lib)
      add_test(test_loggingModule test_loggingModule)
    endif( ENABLE_LOGGING )
    
    add_executable(test_watchdog ${CMAKE_SOURCE_DIR}/test/test_watchdog.cc)
    target_link_libraries(test_watchdog ${ChimeraTK-ApplicationCore_LIBRARIES} 
                                          ${LibXML++_LIBRARIES} 
                                          procps 
                                          ${Boost_LIBRARIES}
                                          ${PROJECT_NAME}lib)
    add_test(test_watchdog test_watchdog)                                          
  endif( ENABLE_TESTING )                                          

else()
  # main server executable
  add_executable(${PROJECT_NAME} ${server_sources} ${library_sources})
  target_link_libraries(${PROJECT_NAME} ${ChimeraTK-ApplicationCore_LIBRARIES} 
                                        ${ChimeraTK-ControlSystemAdapter-DoocsAdapter_LIBRARIES} 
                                        ${ChimeraTK-ControlSystemAdapter-OPCUAAdapter_LIBRARIES}
                                        ${LibXML++_LIBRARIES} 
                                        procps)
  
  # XML file generation
  add_executable(${PROJECT_NAME}-xmlGenerator ${server_sources} ${library_sources})
  set_target_properties(${PROJECT_NAME}-xmlGenerator PROPERTIES COMPILE_FLAGS "-DGENERATE_XML")
  target_link_libraries(${PROJECT_NAME}-xmlGenerator ${ChimeraTK-ApplicationCore_LIBRARIES} 
                                                     ${LibXML++_LIBRARIES} 
                                                     procps) 
endif(CMAKE_BUILD_TYPE MATCHES Debug)

# The make coverage command is only available in debug mode
if(CMAKE_BUILD_TYPE MATCHES Debug)
  configure_file(cmake/Makefile.coverage.in ${PROJECT_BINARY_DIR}/Makefile.coverage @ONLY)
  add_custom_target(coverage
    make -f Makefile.coverage
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating test coverage documentation" VERBATIM
  )
endif(CMAKE_BUILD_TYPE MATCHES Debug)

FILE( COPY ${CMAKE_SOURCE_DIR}/configs/WatchdogServerConfig.xml DESTINATION ${PROJECT_BINARY_DIR})
if(ADAPTER STREQUAL "OPCUA")
  FILE( COPY ${CMAKE_SOURCE_DIR}/configs/WatchdogServer_mapping.xml DESTINATION ${PROJECT_BINARY_DIR})
elseif(ADAPTER STREQUAL "DOOCS")
  FILE( COPY ${CMAKE_SOURCE_DIR}/configs/${PROJECT_NAME}.conf DESTINATION ${PROJECT_BINARY_DIR})
endif()

# add a target to generate API documentation with Doxygen
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/main.dox.in ${CMAKE_CURRENT_BINARY_DIR}/main.dox @ONLY)
include(cmake/enable_doxygen_documentation.cmake)

# Install the executable, server information and the RPC_LIBNO file
if("${CMAKE_INSTALL_PREFIX}" STREQUAL "/export/doocs/server")
  install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${PROJECT_NAME})
  install(FILES ${CMAKE_SOURCE_DIR}/configs/RPC_LIBNO DESTINATION ${PROJECT_NAME})
  install(TARGETS ${PROJECT_NAME}-xmlGenerator RUNTIME DESTINATION ${PROJECT_NAME})
else()
  install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION  bin)
  install(TARGETS ${PROJECT_NAME}-xmlGenerator RUNTIME DESTINATION bin)
endif()

# install config files to /etc/... This allows the user to change config files and keeping these changes even if a new version of the 
# package is installed. 
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configs/chimeratk-watchdog.service.in ${CMAKE_CURRENT_BINARY_DIR}/chimeratk-watchdog.service @ONLY)
install(FILES ${CMAKE_SOURCE_DIR}/configs/WatchdogServerConfig.xml
        DESTINATION /etc/chimeratk/${PROJECT_NAME})
# most systemd service files are stored in /var, but here we want to allow the user to change it -> install to /etc/... like other conf files        
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/chimeratk-watchdog.service
        DESTINATION /etc/chimeratk/${PROJECT_NAME})
IF(ADAPTER STREQUAL "OPCUA")
  install( FILES ${CMAKE_SOURCE_DIR}/configs/WatchdogServer_mapping.xml DESTINATION /etc/chimeratk/${PROJECT_NAME})
ELSEIF(ADAPTER STREQUAL "DOOCS")
  install( FILES ${CMAKE_SOURCE_DIR}/configs/${PROJECT_NAME}.conf DESTINATION ${PROJECT_BINARY_DIR})
ENDIF()
