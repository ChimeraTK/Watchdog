FIND_PACKAGE(Boost COMPONENTS unit_test_framework date_time filesystem)

# Create the executable for automated unit testing.
add_executable(test_procReader ${CMAKE_SOURCE_DIR}/test/test_procReader.cc)
target_link_libraries(test_procReader ${PROJECT_NAME}lib
                                      ${Boost_UNIT_TEST_FRAMEWORK_LIBRARIES})
add_test(test_procReader test_procReader)
add_executable(test_processModule ${CMAKE_SOURCE_DIR}/test/test_processModule.cc)
target_link_libraries(test_processModule ${PROJECT_NAME}lib
                                      ${Boost_UNIT_TEST_FRAMEWORK_LIBRARIES})
add_test(test_processModule test_processModule)
    
add_executable(test_watchdog ${CMAKE_SOURCE_DIR}/test/test_watchdog.cc)
target_link_libraries(test_watchdog ${PROJECT_NAME}lib
                                    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARIES})
add_test(test_watchdog test_watchdog) 
    
add_executable(test_sys_stat ${CMAKE_SOURCE_DIR}/test/test_sys_stat.cc)
target_link_libraries(test_sys_stat ${PROJECT_NAME}lib
                                    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARIES})
add_test(test_sys_stat test_sys_stat)

FILE( COPY cpuinfo_arm
           cpuinfo_amd64
      DESTINATION ${PROJECT_BINARY_DIR}/test )