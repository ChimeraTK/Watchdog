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
#include "ChimeraTK/ApplicationCore/Logging.h"
#include "WatchdogServer.h"
#include "ChimeraTK/ApplicationCore/TestFacility.h"
#include <ChimeraTK/ApplicationCore/ConfigReader.h>
#include <ChimeraTK/ApplicationCore/PeriodicTrigger.h>
#include <ChimeraTK/RegisterPath.h>
#include <libxml++/libxml++.h>

#include <boost/test/unit_test.hpp>
using namespace boost::unit_test_framework;

struct testWD : public ctk::Application {
  SystemInfoModule systemInfo{this, "system", "Module reading system information"};

  ProcessGroup processGroup{this, "process", "Process module group"};

  WatchdogModuleGroup watchdog{this, "watchdog", "Module monitoring the watchdog process"};

  logging::LoggingModule logging;

  testWD() : Application("WatchdogServer") {
    size_t nProcesses = 8;
    for(size_t i = 0; i < nProcesses; i++) {
      std::string processName = std::to_string(i);
      std::cout << "Adding process: " << processName << std::endl;
      processGroup.processes.emplace_back(&processGroup, processName, "process", true);
      //      processGroup.processes.back().logStream = nullptr;
      processGroup.processesLogExternal.emplace_back(&processGroup, processName, "process external log",
          "/Trigger/tick", "/processes/" + processName + "/config/logfileExternal");
    }
    ProcessHandler::setupHandler();
    logging = logging::LoggingModule{this, "logging", "LoggingModule logging watchdog internal messages"};
  }

  ~testWD() { shutdown(); }

  void initialise() override {
    Application::initialise();
    dumpConnections();
  }
};

BOOST_AUTO_TEST_CASE(testPerformance) {
  BOOST_TEST_MESSAGE("Start test used for performance profiling.");
  // test used to profile the performance of the watchdog.
  testWD app;
  ChimeraTK::TestFacility tf;

  tf.setScalarDefault("logging/logFile", (std::string) "test_watchdog.log");
  tf.setScalarDefault("logging/targetStream", (uint)1);
  tf.runApplication();
  for(size_t i = 1; i < 5; i++) {
    tf.writeScalar("Trigger/tick", i);
    tf.stepApplication();
    sleep(1);
  }
}
