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

  ProcessGroup processGroup{this, "process", "Process module group"};

  ProcessInfoModule watchdog{this, "watchdog", "Module monitoring the watchdog process"};

  ctk::ConfigReader config{this, "Configuration", "WatchdogServer_processes.xml"};

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
        processGroup.processes.emplace_back(this, processName, "process");
        processGroup.processes.back().logStream = nullptr;
  #ifdef ENABLE_LOGGING
        processGroup.processesLog.emplace_back(this, processName + "-Log", "process log");
        processGroup.processesLogExternal.emplace_back(this, processName + "-LogExternal", "process external log");
  #endif
      }
    } catch (std::out_of_range &e){
      std::cerr << "Error in the xml file 'WatchdogServer_processes.xml': " << e.what()
              << std::endl;
      std::cout << "I will create only one process named PROCESS..." << std::endl;
      processGroup.processes.emplace_back(this, "PROCESS", "Test process", "PROCESS");
      processGroup.processes.back().logStream = nullptr;
  #ifdef ENABLE_LOGGING
      processGroup.processesLog.emplace_back(this, "PROCESS", "Test process log");
      processGroup.processesLogExternal.emplace_back(this, "PROCESS", "Test process external log");
  #endif
    }

    ProcessHandler::setupHandler();
 }

  ~testWD(){
    shutdown();
  }

  void defineConnections() override{
    for(auto it = systemInfo.info.strInfos.begin(), ite = systemInfo.info.strInfos.end();
        it != ite; it++) {
      it->second >> cs["SYS"](space2underscore(it->first));
    }
    systemInfo.findTag("CS").connectTo(cs[systemInfo.getName()]);
    cs("trigger")  >> systemInfo.trigger;

    watchdog.findTag("CS").connectTo(cs[watchdog.getName()]);
  #ifdef ENABLE_LOGGING
    watchdog.findTag("Logging").connectTo(watchdogLog);

    cs[watchdog.getName()]("logFile") >> watchdogLog.config.logFile;
    cs[watchdog.getName()]("logTailLength") >> watchdogLog.config.tailLength;
    cs[watchdog.getName()]("targetStream") >> watchdogLog.config.targetStream;
    cs[watchdog.getName()]("logLevel") >> watchdogLog.config.logLevel;
    watchdogLog.findTag("CS").connectTo(cs[watchdog.getName()]);

    cs[watchdog.getName()]("logFile") >> watchdogLogFile.config.logFile;
    cs[watchdog.getName()]("logTailLengthExternal") >> watchdogLogFile.config.tailLength;
    cs("trigger") >> watchdogLogFile.trigger;
    watchdogLogFile.findTag("CS").connectTo(cs[watchdog.getName()]);

    systemInfo.findTag("Logging").connectTo(systemInfoLog);
    cs[watchdog.getName()]("logFile") >> systemInfoLog.config.logFile;
    cs[systemInfo.getName()]("logTailLength") >> systemInfoLog.config.tailLength;
    cs[systemInfo.getName()]("targetStream") >> systemInfoLog.config.targetStream;
    cs[systemInfo.getName()]("logLevel") >> systemInfoLog.config.logLevel;
    systemInfoLog.findTag("CS").connectTo(cs[systemInfo.getName()]);


    auto log = processGroup.processesLog.begin();
    auto logExternal = processGroup.processesLogExternal.begin();
  #endif

    systemInfo.info.ticksPerSecond >> watchdog.input.ticksPerSecond;
    systemInfo.status.uptime_secTotal >> watchdog.input.sysUpTime;
    systemInfo.status.startTime >> watchdog.input.sysStartTime;

    cs("trigger") >> watchdog.trigger;


    for(auto &item : processGroup.processes) {
      cs[item.getName()]("enableProcess") >> item.enableProcess;
      cs[item.getName()]("SetCMD") >> item.config.cmd;
      cs[item.getName()]("SetPath") >> item.config.path;
      cs[item.getName()]("SetEnvironment") >> item.config.env;
      cs[item.getName()]("OverwriteEnvironment") >> item.config.overwriteEnv;
      cs[item.getName()]("SetMaxFails") >> item.config.maxFails;
      cs[item.getName()]("SetMaxRestarts") >> item.config.maxRestarts;
      cs[item.getName()]("killSig") >> item.config.killSig;
      cs[item.getName()]("pidOffset") >> item.config.pidOffset;

      item.findTag("CS").connectTo(cs[item.getName()]);
      systemInfo.info.ticksPerSecond >> item.input.ticksPerSecond;
      systemInfo.status.uptime_secTotal >> item.input.sysUpTime;
      systemInfo.status.startTime >> item.input.sysStartTime;
      cs("trigger")  >> item.trigger;

  #ifdef ENABLE_LOGGING
      cs[item.getName()]("SetLogfileExternal") >> item.config.externalLogfile;
      item.logging.message >> (*log).input.message;
      item.logging.messageLevel >> (*log).input.messageLevel;
      cs[watchdog.getName()]("SetLogFile") >> (*log).config.logFile;
      cs[item.getName()]("SetLogLevel") >> (*log).config.logLevel;
      cs[item.getName()]("SetLogTailLength") >> (*log).config.tailLength;
      cs[item.getName()]("SetTargetStream") >> (*log).config.targetStream;
      (*log).findTag("CS").connectTo(cs[item.getName()]);
      item.status.externalLogfile >> (*logExternal).config.logFile;
      cs[item.getName()]("SetLogTailLengthExternal") >> (*logExternal).config.tailLength;
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
