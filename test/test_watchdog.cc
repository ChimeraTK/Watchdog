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
#include "LoggingModule.h"
#include "WatchdogServer.h"
#include "ChimeraTK/ApplicationCore/TestFacility.h"
#include <ChimeraTK/ApplicationCore/ConfigReader.h>
#include <ChimeraTK/RegisterPath.h>
#include <libxml++/libxml++.h>

#include <boost/test/unit_test.hpp>
using namespace boost::unit_test_framework;

struct testWD: public ctk::Application {
  SystemInfoModule systemInfo { this, "SYS",
      "Module reading system information" };

  std::vector<ProcessControlModule> processes;

  ProcessInfoModule watchdog{this, "watchdog", "Module monitoring the watchdog process"};

  ctk::ConfigReader config{this, "Configuration", "WatchdogServer_processes.xml"};

  std::vector<LoggingModule> processesLog;
  std::vector<LogFileModule> processesLogExternal;
  LogFileModule watchdogLogFile{this, "watchdogLog", "Logging module of all watchdog processes"};
  LoggingModule watchdogLog{this, "watchdogLog", "Logging module of the watchdog process"};
  LoggingModule systemInfoLog{this, "systeminfoLog", "Logging module of the system information module"};

  ctk::ControlSystemModule cs;
//  testWD():WatchdogServer(){ };

  testWD() :
      Application("WatchdogServer") {
    try{
      auto strProcesses = config.get<std::vector<std::string>>("processes");
      for(auto &processName : strProcesses) {
        std::cout << "Adding process: " << processName << std::endl;
        processes.emplace_back(this, processName, "process");
        processes.back().logging = nullptr;
  #ifdef ENABLE_LOGGING
        processesLog.emplace_back(this, processName + "-Log", "process log");
        processesLogExternal.emplace_back(this, processName + "-LogExternal", "process external log");
  #endif
      }
    } catch (std::out_of_range &e){
      std::cerr << "Error in the xml file 'WatchdogServer_processes.xml': " << e.what()
              << std::endl;
      std::cout << "I will create only one process named PROCESS..." << std::endl;
      processes.emplace_back(this, "PROCESS", "Test process", "PROCESS");
      processes.back().logging = nullptr;
  #ifdef ENABLE_LOGGING
      processesLog.emplace_back(this, "PROCESS", "Test process log");
      processesLogExternal.emplace_back(this, "PROCESS", "Test process external log");
  #endif
    }

    ProcessHandler::setupHandler();
 }

  ~testWD(){
    shutdown();
  }

  void defineConnections() override{
    for(auto it = systemInfo.strInfos.begin(), ite = systemInfo.strInfos.end();
        it != ite; it++) {
      it->second >> cs["SYS"](space2underscore(it->first));
    }
    systemInfo.findTag("CS").connectTo(cs[systemInfo.getName()]);
    cs("trigger")  >> systemInfo.trigger;

    watchdog.findTag("CS").connectTo(cs[watchdog.getName()]);
  #ifdef ENABLE_LOGGING
    watchdog.findTag("Logging").connectTo(watchdogLog);

    cs[watchdog.getName()]("SetLogFile") >> watchdogLog.logFile;
    cs[watchdog.getName()]("SetLogTailLength") >> watchdogLog.tailLength;
    cs[watchdog.getName()]("SetTargetStream") >> watchdogLog.targetStream;
    cs[watchdog.getName()]("SetLogLevel") >> watchdogLog.logLevel;
    watchdogLog.findTag("CS").connectTo(cs[watchdog.getName()]);

    cs[watchdog.getName()]("SetLogFile") >> watchdogLogFile.logFile;
    cs[watchdog.getName()]("SetLogTailLengthExternal") >> watchdogLogFile.tailLength;
    cs("trigger") >> watchdogLogFile.trigger;
    watchdogLogFile.findTag("CS").connectTo(cs[watchdog.getName()]);

    systemInfo.findTag("Logging").connectTo(systemInfoLog);
    cs[watchdog.getName()]("SetLogFile") >> systemInfoLog.logFile;
    cs[systemInfo.getName()]("SetLogTailLength") >> systemInfoLog.tailLength;
    cs[systemInfo.getName()]("SetTargetStream") >> systemInfoLog.targetStream;
    cs[systemInfo.getName()]("SetLogLevel") >> systemInfoLog.logLevel;
    systemInfoLog.findTag("CS").connectTo(cs[systemInfo.getName()]);


    auto log = processesLog.begin();
    auto logExternal = processesLogExternal.begin();
  #endif

    systemInfo.ticksPerSecond >> watchdog.ticksPerSecond;
    systemInfo.uptime_secTotal >> watchdog.sysUpTime;
    systemInfo.startTime >> watchdog.sysStartTime;

    cs("trigger") >> watchdog.trigger;


    for(auto &item : processes) {
      cs[item.getName()]("enableProcess") >> item.enableProcess;
      cs[item.getName()]("SetCMD") >> item.processSetCMD;
      cs[item.getName()]("SetPath") >> item.processSetPath;
      cs[item.getName()]("SetEnvironment") >> item.processSetENV;
      cs[item.getName()]("OverwriteEnvironment") >> item.processOverwriteENV;
      cs[item.getName()]("SetMaxFails") >> item.processMaxFails;
      cs[item.getName()]("SetMaxRestarts") >> item.processMaxRestarts;
      cs[item.getName()]("killSig") >> item.killSig;
      cs[item.getName()]("pidOffset") >> item.pidOffset;

      item.findTag("CS").connectTo(cs[item.getName()]);
      systemInfo.ticksPerSecond >> item.ticksPerSecond;
      systemInfo.uptime_secTotal >> item.sysUpTime;
      systemInfo.startTime >> item.sysStartTime;
      cs("trigger")  >> item.trigger;

  #ifdef ENABLE_LOGGING
      cs[item.getName()]("SetLogfileExternal") >> item.processSetExternalLogfile;
      item.message >> (*log).message;
      item.messageLevel >> (*log).messageLevel;
      cs[watchdog.getName()]("SetLogFile") >> (*log).logFile;
      cs[item.getName()]("SetLogLevel") >> (*log).logLevel;
      cs[item.getName()]("SetLogTailLength") >> (*log).tailLength;
      cs[item.getName()]("SetTargetStream") >> (*log).targetStream;
      (*log).findTag("CS").connectTo(cs[item.getName()]);
      item.processExternalLogfile >> (*logExternal).logFile;
      cs[item.getName()]("SetLogTailLengthExternal") >> (*logExternal).tailLength;
      cs("trigger") >> (*logExternal).trigger;
      (*logExternal).findTag("CS").connectTo(cs[item.getName()]);

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
  auto writeTrigger = tf.getScalar<uint>("trigger/");
  auto logFile = tf.getScalar<std::string>("watchdog/SetLogFile");
  ChimeraTK::ScalarRegisterAccessor<uint> targetStream[8] = {
  tf.getScalar<uint>("IN1-MB01/SetTargetStream"),
  tf.getScalar<uint>("IN1-MB02/SetTargetStream"),
  tf.getScalar<uint>("LA1-RC01/SetTargetStream"),
  tf.getScalar<uint>("LA1-RC11/SetTargetStream"),
  tf.getScalar<uint>("LA2-RC01/SetTargetStream"),
  tf.getScalar<uint>("LA2-RC11/SetTargetStream"),
  tf.getScalar<uint>("watchdog/SetTargetStream"),
  tf.getScalar<uint>("SYS/SetTargetStream")};

  tf.runApplication();

  for(int i = 0; i < 8; i++){
    targetStream[i] = 2;
    targetStream[i].write();
  }
  logFile = std::string("test_watchdog.log");
  logFile.write();

  for(size_t i = 1; i < 20; i++){
    writeTrigger = i;
    writeTrigger.write();
    tf.stepApplication();
    sleep(5);
  }
}
