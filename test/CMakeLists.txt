FIND_PACKAGE(Boost COMPONENTS unit_test_framework date_time filesystem)

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
    
add_executable(test_sys_stat ${CMAKE_SOURCE_DIR}/test/test_sys_stat.cc)
target_link_libraries(test_sys_stat procps
                                    ${Boost_LIBRARIES}
                                    ${PROJECT_NAME}lib)
add_test(test_sys_stat test_sys_stat)

if(ChimeraTK-DAQ_FOUND)
  if(USE_ROOT_DAQ)
    set_target_properties(test_watchdog PROPERTIES COMPILE_FLAGS "-DROOTDAQ")
  else()
    set_target_properties(test_watchdog PROPERTIES COMPILE_FLAGS "-DMICRODAQ")
  endif()
endif(ChimeraTK-DAQ_FOUND)          
                                    
FILE( COPY cpuinfo_arm
           cpuinfo_amd64
      DESTINATION ${PROJECT_BINARY_DIR}/test )