PROJECT(watchdog-server)
cmake_minimum_required(VERSION 2.8)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

set(${PROJECT_NAME}_MAJOR_VERSION 00)
set(${PROJECT_NAME}_MINOR_VERSION 02)
set(${PROJECT_NAME}_PATCH_VERSION 03)
include(cmake/set_version_numbers.cmake)

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

FIND_PACKAGE(Boost COMPONENTS unit_test_framework)
ENABLE_TESTING()

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

IF(DEFINED ENABLE_LOGGING)
  MESSAGE(STATUS "Logging modules are enabled.")
  add_definitions(-DENABLE_LOGGING)
ENDIF(DEFINED ENABLE_LOGGING)


message("CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
IF(CMAKE_BUILD_TYPE MATCHES Debug)
  add_library(${PROJECT_NAME}lib SHARED ${library_sources} )
  set_target_properties(${PROJECT_NAME}lib PROPERTIES VERSION ${${PROJECT_NAME}_FULL_LIBRARY_VERSION}
                                                    SOVERSION ${${PROJECT_NAME}_SOVERSION})
  target_link_libraries(${PROJECT_NAME}lib ${Boost_LIBRARIES}
                                           ${ChimeraTK-ApplicationCore_LIBRARIES}
                                           procps
                                           ${Boost_process_LIBRARIES})
  
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
  
  # Create the executable for automated unit testing.
  add_executable(test_procReader ${CMAKE_SOURCE_DIR}/test/test_procReader.cc)
  target_link_libraries(test_procReader ${ChimeraTK-ApplicationCore_LIBRARIES} 
                                        ${LibXML++_LIBRARIES} 
                                        procps 
                                        ${Boost_LIBRARIES}
                                        ${PROJECT_NAME}lib)
  add_test(test_procReader test_procReader)
  IF(DEFINED ENABLE_LOGGING)
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
  ENDIF(DEFINED ENABLE_LOGGING)
  
  add_executable(test_watchdog ${CMAKE_SOURCE_DIR}/test/test_watchdog.cc)
  target_link_libraries(test_watchdog ${ChimeraTK-ApplicationCore_LIBRARIES} 
                                        ${LibXML++_LIBRARIES} 
                                        procps 
                                        ${Boost_LIBRARIES}
                                        ${PROJECT_NAME}lib)
ELSE()
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
ENDIF(CMAKE_BUILD_TYPE MATCHES Debug)

# The make coverage command is only available in debug mode
IF(CMAKE_BUILD_TYPE MATCHES Debug)
  configure_file(cmake/Makefile.coverage.in ${PROJECT_BINARY_DIR}/Makefile.coverage @ONLY)
  add_custom_target(coverage
    make -f Makefile.coverage
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating test coverage documentation" VERBATIM
  )
ENDIF(CMAKE_BUILD_TYPE MATCHES Debug)

FILE( COPY ${CMAKE_SOURCE_DIR}/configs/WatchdogServer_processes.xml DESTINATION ${PROJECT_BINARY_DIR})
IF(ADAPTER STREQUAL "OPCUA")
  FILE( COPY ${CMAKE_SOURCE_DIR}/configs/WatchdogServer_mapping.xml DESTINATION ${PROJECT_BINARY_DIR})
ELSEIF(ADAPTER STREQUAL "DOOCS")
  FILE( COPY ${CMAKE_SOURCE_DIR}/configs/${PROJECT_NAME}.conf DESTINATION ${PROJECT_BINARY_DIR})
ENDIF()

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
install(FILES ${CMAKE_SOURCE_DIR}/configs/WatchdogServer_processes.xml
        DESTINATION /etc/chimeratk/${PROJECT_NAME})
# move systemd service file to /var similar to other system services        
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/chimeratk-watchdog.service
        DESTINATION /var/lib/chimeratk/${PROJECT_NAME})
IF(ADAPTER STREQUAL "OPCUA")
  install( FILES ${CMAKE_SOURCE_DIR}/configs/WatchdogServer_mapping.xml DESTINATION /etc/chimeratk/${PROJECT_NAME})
ELSEIF(ADAPTER STREQUAL "DOOCS")
  install( FILES ${CMAKE_SOURCE_DIR}/configs/${PROJECT_NAME}.conf DESTINATION ${PROJECT_BINARY_DIR})
ENDIF()
