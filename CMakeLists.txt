cmake_minimum_required(VERSION 3.5)
PROJECT(watchdog-server)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

set(${PROJECT_NAME}_MAJOR_VERSION 02)
set(${PROJECT_NAME}_MINOR_VERSION 02)
set(${PROJECT_NAME}_PATCH_VERSION 01)

include(cmake/set_version_numbers.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/set_default_flags.cmake)
include(cmake/set_control_system_adapter.cmake)


# Options
option(ENABLE_TESTING      "Build unit tests"      ON)
option(ENABLE_DOCUMENTATION "Enable create of doxygen documentation"      ON)

if( ENABLE_DOCUMENTATION )
  SET( SUPPRESS_AUTO_DOC_BUILD FALSE)
else()
  SET( SUPPRESS_AUTO_DOC_BUILD TRUE)
endif( ENABLE_DOCUMENTATION )

find_package(ChimeraTK-ApplicationCore 03.00 REQUIRED)
find_package(ChimeraTK-ApplicationCore-MicroDAQ 02.00 REQUIRED)
find_package(ChimeraTK-ApplicationCore-LoggingModule 01.00 REQUIRED)
find_package(ChimeraTK-ApplicationCore-ServerHistoryModule 01.00 REQUIRED)

find_package(Boost COMPONENTS date_time REQUIRED)

FIND_PACKAGE(PkgConfig REQUIRED)
PKG_CHECK_MODULES(libproc2 IMPORTED_TARGET libproc2)
if(libproc2_FOUND)
  SET(PROCLIB PkgConfig::libproc2)
else()
  find_library(procps NAMES procps REQUIRED)
  # Just check for procps.h although more headers are used by the watchdog
  # If that header is found assume that the dev package is installed
  include(CheckIncludeFile)
  CHECK_INCLUDE_FILE( proc/procps.h HAS_PROCPS_H)
  if(NOT HAS_PROCPS_H)
    message(FATAL_ERROR "Could not find procps header file. Please install procps dev pacakge.")
  endif(NOT HAS_PROCPS_H)
  SET(PROCLIB procps)
endif(libproc2_FOUND) 

include(cmake/enable_code_coverage_report.cmake)
# enable coding style test
include(cmake/enable_code_style_check.cmake)

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


if(CMAKE_BUILD_TYPE MATCHES Debug)
  add_library(${PROJECT_NAME}lib SHARED ${library_sources} )
  set_target_properties(${PROJECT_NAME}lib PROPERTIES VERSION ${${PROJECT_NAME}_FULL_LIBRARY_VERSION}
                                                    SOVERSION ${${PROJECT_NAME}_SOVERSION})
  target_link_libraries(${PROJECT_NAME}lib ChimeraTK::ChimeraTK-ApplicationCore 
                                           ChimeraTK::ChimeraTK-ApplicationCore-LoggingModule
                                           ChimeraTK::ChimeraTK-ApplicationCore-ServerHistoryModule
                                           ChimeraTK::ChimeraTK-ApplicationCore-MicroDAQ
                                           ${PROCLIB}
                                           )
  install(TARGETS ${PROJECT_NAME}lib LIBRARY DESTINATION lib)

  # main server executable
  add_executable(${PROJECT_NAME} ${server_sources})
  target_link_libraries(${PROJECT_NAME} ChimeraTK::ChimeraTK-ApplicationCore
                                        ChimeraTK::SelectedAdapter
                                        ${PROJECT_NAME}lib)

  # XML file generation
  add_executable(${PROJECT_NAME}-xmlGenerator ${server_sources})
  target_link_libraries(${PROJECT_NAME}-xmlGenerator ChimeraTK::ChimeraTK-ApplicationCore
                                                     ${PROJECT_NAME}lib)
  if(libproc2_FOUND)
    set_target_properties(${PROJECT_NAME}-xmlGenerator PROPERTIES COMPILE_FLAGS "-DGENERATE_XML")
  else()
    set_target_properties(${PROJECT_NAME}lib PROPERTIES COMPILE_FLAGS "-DWITH_PROCPS")
    set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-DWITH_PROCPS")
    set_target_properties(${PROJECT_NAME}-xmlGenerator PROPERTIES COMPILE_FLAGS "-DGENERATE_XML -DWITH_PROCPS")
  endif(libproc2_FOUND)


  if( ENABLE_TESTING )
    ENABLE_TESTING()
    add_subdirectory(test)
  endif( ENABLE_TESTING )

else()
  # main server executable
  add_executable(${PROJECT_NAME} ${server_sources} ${library_sources})
  if(ADAPTER STREQUAL EPICSIOC) # This is exclusive to EPICSIOC, because this line was not present, before I added EPICSIOS as an adapter.
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS ${Adapter_LINK_FLAGS})
  endif(ADAPTER STREQUAL EPICSIOC)
  target_link_libraries(${PROJECT_NAME} ChimeraTK::SelectedAdapter
                                        ${PROCLIB}
                                        ChimeraTK::ChimeraTK-ApplicationCore 
                                        ChimeraTK::ChimeraTK-ApplicationCore-LoggingModule
                                        ChimeraTK::ChimeraTK-ApplicationCore-ServerHistoryModule
                                        ChimeraTK::ChimeraTK-ApplicationCore-MicroDAQ)

  # XML file generation
  add_executable(${PROJECT_NAME}-xmlGenerator ${server_sources} ${library_sources})
  target_link_libraries(${PROJECT_NAME}-xmlGenerator ${PROCLIB}
                                                     ChimeraTK::ChimeraTK-ApplicationCore 
                                                     ChimeraTK::ChimeraTK-ApplicationCore-LoggingModule
                                                     ChimeraTK::ChimeraTK-ApplicationCore-ServerHistoryModule
                                                     ChimeraTK::ChimeraTK-ApplicationCore-MicroDAQ)

  set_target_properties(${PROJECT_NAME}-xmlGenerator PROPERTIES COMPILE_FLAGS "-DGENERATE_XML")
  if(NOT libproc2_FOUND)
    set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-DWITH_PROCPS")
    set_target_properties(${PROJECT_NAME}-xmlGenerator PROPERTIES COMPILE_FLAGS "-DWITH_PROCPS -DGENERATE_XML")
  endif(NOT libproc2_FOUND)

endif(CMAKE_BUILD_TYPE MATCHES Debug)

# Set RPATH to find root dependencies automatically
set_target_properties( ${PROJECT_NAME} PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
set_target_properties( ${PROJECT_NAME}-xmlGenerator PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)

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

# Use GNUInstallDirs to resolve /etc/ and /usr/ correctly
# When CMAKE_INSTALL_PREFIX is set config files end up in ${CMAKE_INSTALL_PREFIX}/etc/chimeratk/watchdog-server/
# If not set they are installed to /usr/local/etc/chimeratk/watchdog-server/
# If set to "/usr" (i.e. when building a debian package) or "/" they are installed to /etc/chimeratk/watchdog-server/
include(GNUInstallDirs)

# Install config files to the system config location (e.g. /etc/ on unix). 
# This allows the user to change config files and keeping these changes even if a new version of the
# package is installed.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configs/chimeratk-watchdog.service.in ${CMAKE_CURRENT_BINARY_DIR}/chimeratk-watchdog.service @ONLY)
install(FILES ${CMAKE_SOURCE_DIR}/configs/WatchdogServerConfig.xml
        DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/chimeratk/${PROJECT_NAME})
# most systemd service files are stored in /var, but here we want to allow the user to change it -> install to /etc/... like other config files
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/chimeratk-watchdog.service
        DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/chimeratk/${PROJECT_NAME})
IF(ADAPTER STREQUAL "OPCUA")
  install( FILES ${CMAKE_SOURCE_DIR}/configs/WatchdogServer_mapping.xml DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/chimeratk/${PROJECT_NAME})
ELSEIF(ADAPTER STREQUAL "DOOCS")
  install( FILES ${CMAKE_SOURCE_DIR}/configs/${PROJECT_NAME}.conf DESTINATION ${PROJECT_BINARY_DIR})
ENDIF()