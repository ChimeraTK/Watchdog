/*
 * WatchdogServer.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "WatchdogServer.h"

//#include <libxml++/libxml++.h>

void TimerModule::mainLoop() {
  while(true) {
    update.read();
    trigger = trigger + 1;
    trigger.write();
    itrigger = trigger;
    itrigger.write();
    if(update < 1){
      sleep(3);
    } else {
      sleep(update);
    }
  }
}

WatchdogServer::WatchdogServer() :
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
    processes.emplace_back(this, "PROCESS", "Test process");
    processes.back().logging = nullptr;
#ifdef ENABLE_LOGGING
    processesLog.emplace_back(this, "PROCESS", "Test process log");
    processesLogExternal.emplace_back(this, "PROCESS", "Test process external log");
#endif
  }

  ProcessHandler::setupHandler();
}

void WatchdogServer::defineConnections() {
  for(auto it = systemInfo.strInfos.begin(), ite = systemInfo.strInfos.end();
      it != ite; it++) {
    it->second >> cs["SYS"](space2underscore(it->first));
  }
  systemInfo.findTag("CS").connectTo(cs[systemInfo.getName()]);
  timer.trigger >> systemInfo.trigger;
  cs[timer.getName()]("UpdateTime") >> timer.update;

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
  timer.trigger >> watchdogLogFile.trigger;
  watchdogLogFile.findTag("CS").connectTo(cs[watchdog.getName()]);

  systemInfo.findTag("Logging").connectTo(systemInfoLog);
  cs[watchdog.getName()]("SetLogFile") >> systemInfoLog.logFile;
  cs[systemInfo.getName()]("SetLogTailLength") >> systemInfoLog.tailLength;
  cs[systemInfo.getName()]("SetTargetStream") >> systemInfoLog.targetStream;
  cs[systemInfo.getName()]("SetLogLevel") >> systemInfoLog.logLevel;
  systemInfoLog.findTag("CS").connectTo(cs[systemInfo.getName()]);


  //ToDo: Include after bug in OPC UA Adapter was fixed
  // publish configuration
//  config.connectTo(cs["Configuration"]);

  auto log = processesLog.begin();
  auto logExternal = processesLogExternal.begin();
#endif

	systemInfo.ticksPerSecond >> watchdog.ticksPerSecond;
  systemInfo.uptime_secTotal >> watchdog.sysUpTime;
  systemInfo.startTime >> watchdog.sysStartTime;
  timer.trigger >> watchdog.trigger;


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
    timer.trigger >> item.trigger;

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
    timer.trigger >> (*logExternal).trigger;
    (*logExternal).findTag("CS").connectTo(cs[item.getName()]);

    log++;
    logExternal++;
#endif
  }
  /*
   *  Micro DAQ system
   */

  if(config.get<int>("enableMicroDAQ") != 0) {

    microDAQ = ctk::MicroDAQ(this, "MicroDAQ", "Local ringbuffer DAQ system");
    microDAQ.addSource(watchdog.findTag("DAQ"), watchdog.getName());
    microDAQ.addSource(systemInfo.findTag("DAQ"), systemInfo.getName());
    for(auto &item : processes) {
      microDAQ.addSource(item.findTag("DAQ"), item.getName());
    }

    // configuration of the DAQ system itself
    timer.itrigger >> microDAQ.trigger;
    microDAQ.findTag("MicroDAQ.CONFIG").connectTo(cs["MicroDAQ"]);

    cs["MicroDAQ"]("nMaxFiles") >> microDAQ.nMaxFiles;
    cs["MicroDAQ"]("nTriggersPerFile") >> microDAQ.nTriggersPerFile;
    cs["MicroDAQ"]("enable") >> microDAQ.enable;

  }
  dumpConnections();

}

