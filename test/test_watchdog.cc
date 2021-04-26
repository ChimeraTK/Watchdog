/*
 * test_processModule.cc
 *
 *  Created on: Jan 31, 2018
 *      Author: zenker
 */
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE ProcessModuleTest

#include <iostream>
#include <string>

#include "ProcessModule.h"
#ifdef ENABLE_LOGGING
#include "LoggingModule.h"
#endif
#include "WatchdogServer.h"
#include "ChimeraTK/ApplicationCore/TestFacility.h"
#include <ChimeraTK/ApplicationCore/ConfigReader.h>
#include <ChimeraTK/ApplicationCore/PeriodicTrigger.h>
#include <ChimeraTK/RegisterPath.h>
#include <libxml++/libxml++.h>

#include <boost/test/unit_test.hpp>
using namespace boost::unit_test_framework;

struct testWD: public ctk::Application {
  SystemInfoGroup systemInfo { this, "system",
      "Module reading system information" };

  ProcessGroup processGroup{this, "process", "Process module group"};

  WatchdogModuleGroup watchdog{this, "watchdog", "Module monitoring the watchdog process"};

  ctk::ControlSystemModule cs;
//  testWD():WatchdogServer(){ };

  testWD() :
      Application("WatchdogServer") {
    size_t nProcesses = 8;
    for(size_t i = 0; i < nProcesses; i++) {
      std::string processName = std::to_string(i);
      std::cout << "Adding process: " << processName << std::endl;
      processGroup.processes.emplace_back(&processGroup, processName, "process", true);
      processGroup.processes.back().logStream = nullptr;
#ifdef ENABLE_LOGGING
      processGroup.processesLog.emplace_back(&processGroup, processName, "process log");
      processGroup.processesLogExternal.emplace_back(&processGroup, processName , "process external log", "/Configuration/tick", "/processes/" + processName + "/config/logfileExternal");
#endif
    }
    ProcessHandler::setupHandler();
 }

  ~testWD(){
    shutdown();
  }

  void defineConnections() override{
  #ifdef ENABLE_LOGGING
    watchdog.process.logging.connectTo(watchdog.logging.input);
    systemInfo.info.logging.connectTo(systemInfo.logging.input);
    auto log = processGroup.processesLog.begin();
  #endif

    systemInfo.findTag("ProcessModuleInput").flatten().connectTo(watchdog.process.input);
    processGroup.findTag("CS").connectTo(cs["processes"]);
    for(auto &item : processGroup.processes) {
      systemInfo.findTag("ProcessModuleInput").flatten().connectTo(item.input);
  #ifdef ENABLE_LOGGING
      item.logging.connectTo((*log).input);
      log++;
  #endif
    }
    findTag("CS").connectTo(cs);
    dumpConnections();
  }
};


BOOST_AUTO_TEST_CASE( testPerformance) {
  BOOST_TEST_MESSAGE("Start test used for performance profiling.");
  // test used to profile the performance of the watchdog.
  testWD app;
  app.defineConnections();
  ChimeraTK::TestFacility tf;

  // Get the trigger variable thats blocking the application (i.e. ProcessControlModule)
  auto writeTrigger = tf.getScalar<uint64_t>("Configuration/tick");
#ifdef ENABLE_LOGGING
  tf.setScalarDefault("watchdog/config/logfile", (std::string)"test_watchdog.log");
  for(size_t i = 0; i < 8; ++i){
    std::string name = "processes/";
    name = name + std::to_string(i) + "/config/targetStream";
    tf.setScalarDefault(name.data(),(uint)1);
  }
  tf.setScalarDefault("watchdog/config/targetStream", (uint)1);
  tf.setScalarDefault("system/config/targetStream", (uint)1);
#endif
  tf.runApplication();
  for(size_t i = 1; i < 5; i++){
    writeTrigger = i;
    writeTrigger.write();
    tf.stepApplication();
    sleep(1);
  }
}
