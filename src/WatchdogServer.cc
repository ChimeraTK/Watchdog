/*
 * WatchdogServer.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "WatchdogServer.h"

#include "boost/filesystem.hpp"

namespace bfs = boost::filesystem;

/*
 * Select all mount points that mount a physical hard drive on the local machine.
 * The boot partition is not selected.
 * Only devices that include /dev/sd are considered.
 */
std::set<std::pair<std::string, std::string> > findMountPoints(){
  std::set<std::pair<std::string, std::string> > out;
  std::ifstream in("/proc/mounts");
  std::string strin[4];
  int iin[2];
  while(in.good()){
    in >> strin[0] >> strin[1] >> strin[2] >> strin[3] >> iin[0] >> iin[1];
    bfs::path p(strin[0]);
    if(strin[0].substr(0,7).compare("/dev/sd") == 0){
      if(strin[1].substr(0,6).compare("/boot/") != 0){
        out.insert(std::make_pair(p.filename().string(),strin[1]));
      }
    }
  }
  in.close();
  return out;
}

/*
 * Select all non virtual network adapter.
 */
std::set<std::string> findNetworkDevices(){
  std::set<std::string> out;
  bfs::path p("/sys/class/net");
  try {
    if (exists(p)){    // does p actually exist?
      if (is_directory(p)){      // is p a directory?
        for(auto i = bfs::directory_iterator(p); i != bfs::directory_iterator(); i++){
          if(bfs::canonical(i->path()).string().find("virtual") == std::string::npos)
            out.insert(i->path().filename().string());
        }
      }
    }
  } catch (const bfs::filesystem_error& ex) {
    std::cout << ex.what() << '\n';
  }
  return out;
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
    std::cerr << "Error in the xml file 'WatchdogServerConfig.xml': " << e.what()
            << std::endl;
    std::cout << "I will create only one process named PROCESS..." << std::endl;
    processes.emplace_back(this, "PROCESS", "Test process");
    processes.back().logging = nullptr;
#ifdef ENABLE_LOGGING
    processesLog.emplace_back(this, "PROCESS", "Process log");
    processesLogExternal.emplace_back(this, "PROCESS", "Process external log");
#endif
  }
  auto fs = findMountPoints();
  for(auto &mountPoint : fs){
    std::string name = std::string("fs.")+mountPoint.first;
    std::cout << "Adding filesystem monitor for: " << mountPoint.first << " mounted at: " << mountPoint.second << " -->" << name << std::endl;
    fsMonitors.emplace_back(mountPoint.first, mountPoint.second, this, name, "Filesystem monitor");
#ifdef ENABLE_LOGGING
    processesLog.emplace_back(this, "fsLogging", "File system monitor log");
#endif
  }
  auto net = findNetworkDevices();
  for(auto &dev : net){
    std::string name = std::string("net.")+dev;
    std::cout << "Adding network monitor for device: " << dev << " -->" << name << std::endl;
    networkMonitors.emplace_back(dev, this, name, "Network monitor");
  #ifdef ENABLE_LOGGING
    processesLog.emplace_back(this, "netLogging", "Network monitor log");
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
  trigger.tick >> systemInfo.trigger;
  cs["configuration"]("UpdateTime") >> trigger.timeout;

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
  trigger.tick >> watchdogLogFile.trigger;
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
  trigger.tick >> watchdog.trigger;


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
    trigger.tick >> item.trigger;

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
    trigger.tick >> (*logExternal).trigger;
    (*logExternal).findTag("CS").connectTo(cs[item.getName()]);

    log++;
    logExternal++;
#endif
  }
  for(auto &item : fsMonitors){
    item.findTag("CS").connectTo(cs[item.getName()]);
    trigger.tick >> item.trigger;
#ifdef ENABLE_LOGGING
    item.message >> (*log).message;
    item.messageLevel >> (*log).messageLevel;
    cs[watchdog.getName()]("SetLogFile") >> (*log).logFile;
    cs[item.getName()]("SetLogLevel") >> (*log).logLevel;
    cs[item.getName()]("SetLogTailLength") >> (*log).tailLength;
    cs[item.getName()]("SetTargetStream") >> (*log).targetStream;
    (*log).findTag("CS").connectTo(cs[item.getName()]);
    log++;
#endif
  }

  for(auto &item : networkMonitors){
    item.findTag("CS").connectTo(cs[item.getName()]);
    trigger.tick >> item.trigger;
#ifdef ENABLE_LOGGING
    item.message >> (*log).message;
    item.messageLevel >> (*log).messageLevel;
    cs[watchdog.getName()]("SetLogFile") >> (*log).logFile;
    cs[item.getName()]("SetLogLevel") >> (*log).logLevel;
    cs[item.getName()]("SetLogTailLength") >> (*log).tailLength;
    cs[item.getName()]("SetTargetStream") >> (*log).targetStream;
    (*log).findTag("CS").connectTo(cs[item.getName()]);
    log++;
#endif
  }

  //\FixMe: Remove once MicroDAQ is using uint
  trigger.tick >> conversion.triggerIn;
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
    conversion.triggerOut >> microDAQ.trigger;
    microDAQ.findTag("MicroDAQ.CONFIG").connectTo(cs["MicroDAQ"]);

    cs["MicroDAQ"]("nMaxFiles") >> microDAQ.nMaxFiles;
    cs["MicroDAQ"]("nTriggersPerFile") >> microDAQ.nTriggersPerFile;
    cs["MicroDAQ"]("enable") >> microDAQ.enable;

  }
  dumpConnections();

}

