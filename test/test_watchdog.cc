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
#include <ChimeraTK/RegisterPath.h>
#include <libxml++/libxml++.h>

#include <boost/test/unit_test.hpp>
using namespace boost::unit_test_framework;

struct testWD: public ctk::Application {
  SystemInfoModule systemInfo { this, "system",
      "Module reading system information" };

  ProcessGroup processGroup{this, "process", "Process module group"};

  ProcessInfoModule watchdog{this, "watchdog", "Module monitoring the watchdog process"};

  ctk::ConfigReader config{this, "Configuration", "WatchdogServerConfig.xml"};
#ifdef ENABLE_LOGGING
  LogFileModule watchdogLogFile{this, "watchdogLog", "Logging module of all watchdog processes"};
  LoggingModule watchdogLog{this, "watchdogLog", "Logging module of the watchdog process"};
  LoggingModule systemInfoLog{this, "systeminfoLog", "Logging module of the system information module"};
#endif
  ctk::ControlSystemModule cs;
//  testWD():WatchdogServer(){ };

  testWD() :
      Application("WatchdogServer") {
    try{
      auto nProcesses = config.get<uint>("numberOfProcesses");
      for(size_t i = 0; i < nProcesses; i++) {
        std::string processName = std::to_string(i);
        std::cout << "Adding process: " << processName << std::endl;
        if(config.get<uint>("enableServerHistory") != 0){
          processGroup.processes.emplace_back(&processGroup, processName, "process", true);
        } else {
          processGroup.processes.emplace_back(&processGroup, processName, "process");
        }
        processGroup.processes.back().logStream = nullptr;
  #ifdef ENABLE_LOGGING
        processGroup.processesLog.emplace_back(&processGroup, processName, "process log");
        processGroup.processesLogExternal.emplace_back(&processGroup, processName , "process external log");
  #endif
      }
    } catch (std::out_of_range &e){
      std::cerr << "Error in the xml file 'WatchdogServerConfig.xml': " << e.what()
              << std::endl;
      std::cout << "I will create only one process named PROCESS..." << std::endl;
      if(config.get<uint>("enableServerHistory") != 0){
        processGroup.processes.emplace_back(&processGroup, "0", "Test process", true);
      } else {
        processGroup.processes.emplace_back(&processGroup, "0", "Test process");
      }
      processGroup.processes.back().logStream = nullptr;
  #ifdef ENABLE_LOGGING
      processGroup.processesLog.emplace_back(&processGroup, "0", "Process log");
      processGroup.processesLogExternal.emplace_back(&processGroup, "0", "Process external log");
  #endif
    }

    ProcessHandler::setupHandler();
 }

  ~testWD(){
    shutdown();
  }

  void defineConnections() override{
    systemInfo.findTag("CS").connectTo(cs[systemInfo.getName()]);
    cs("trigger")  >> systemInfo.trigger;

    watchdog.findTag("CS").connectTo(cs[watchdog.getName()]);
  #ifdef ENABLE_LOGGING
    // allow to set the logFile name -> other modules will use the same log file
    watchdogLog.findTag("CS_OPTIONAL").connectTo(cs[watchdog.getName()]);
    watchdog.logging.connectTo(watchdogLog.input);
    watchdogLog.findTag("CS").connectTo(cs[watchdog.getName()]);

    watchdogLog.config.logFile >> watchdogLogFile.config.logFile;
    systemInfo.trigger >> watchdogLogFile.trigger;
    watchdogLogFile.findTag("CS").connectTo(cs[watchdog.getName()]);

    systemInfo.logging.connectTo(systemInfoLog.input);
    watchdogLog.config.logFile >> systemInfoLog.config.logFile;
    systemInfoLog.findTag("CS").connectTo(cs[systemInfo.getName()]);


    //ToDo: Include after bug in OPC UA Adapter was fixed
    // publish configuration
  //  config.connectTo(cs["Configuration"]);

    auto log = processGroup.processesLog.begin();
    auto logExternal = processGroup.processesLogExternal.begin();
  #endif

    systemInfo.info.ticksPerSecond >> watchdog.input.ticksPerSecond;
    systemInfo.status.uptime_secTotal >> watchdog.input.sysUpTime;
    systemInfo.status.startTime >> watchdog.input.sysStartTime;

    systemInfo.trigger >> watchdog.trigger;

    processGroup.findTag("CS").connectTo(cs["processes"]);
    for(auto &item : processGroup.processes) {
      systemInfo.info.ticksPerSecond >> item.input.ticksPerSecond;
      systemInfo.status.uptime_secTotal >> item.input.sysUpTime;
      systemInfo.status.startTime >> item.input.sysStartTime;
      systemInfo.trigger  >> item.trigger;

  #ifdef ENABLE_LOGGING
      item.logging.connectTo((*log).input);
      watchdogLog.config.logFile >> (*log).config.logFile;
      item.config.externalLogfile >> (*logExternal).config.logFile;
      systemInfo.trigger >> (*logExternal).trigger;
      log++;
      logExternal++;
  #endif
    }
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
  auto writeTrigger = tf.getScalar<uint64_t>("trigger/");
#ifdef ENABLE_LOGGING
  auto logFile = tf.getScalar<std::string>("watchdog/config/logFile");
  ChimeraTK::ScalarRegisterAccessor<uint> targetStream[10] = {
  tf.getScalar<uint>("processes/0/config/targetStream"),
  tf.getScalar<uint>("processes/1/config/targetStream"),
  tf.getScalar<uint>("processes/2/config/targetStream"),
  tf.getScalar<uint>("processes/3/config/targetStream"),
  tf.getScalar<uint>("processes/4/config/targetStream"),
  tf.getScalar<uint>("processes/5/config/targetStream"),
  tf.getScalar<uint>("processes/6/config/targetStream"),
  tf.getScalar<uint>("processes/7/config/targetStream"),
  tf.getScalar<uint>("watchdog/config/targetStream"),
  tf.getScalar<uint>("system/config/targetStream")};
#endif
  tf.runApplication();
#ifdef ENABLE_LOGGING
  for(int i = 0; i < 10; i++){
    targetStream[i] = 1;
    targetStream[i].write();
  }

  logFile = std::string("test_watchdog.log");
  logFile.write();
#endif
  for(size_t i = 1; i < 5; i++){
    writeTrigger = i;
    writeTrigger.write();
    tf.stepApplication();
    sleep(1);
  }
}
