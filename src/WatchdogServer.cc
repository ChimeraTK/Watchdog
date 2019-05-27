/*
 * WatchdogServer.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "WatchdogServer.h"

#include "boost/filesystem.hpp"

#include <fstream>
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
    auto nProcesses = config.get<uint>("numberOfProcesses");
    std::cout << "Adding " << nProcesses << " processes." << std::endl;
    for(size_t i = 0; i < nProcesses; i++) {
      std::string processName = std::to_string(i);
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
  size_t i =0;
  auto fs = findMountPoints();
  for(auto &mountPoint : fs){
    std::string name = std::to_string(i);
    std::cout << "Adding filesystem monitor for: " << mountPoint.first << " mounted at: " << mountPoint.second << " -->" << name << std::endl;
    filesystemGroup.fsMonitors.emplace_back(mountPoint.first, mountPoint.second, &filesystemGroup, name, "Filesystem monitor");
#ifdef ENABLE_LOGGING
    filesystemGroup.loggingModules.emplace_back(&filesystemGroup, name, "File system monitor log");
#endif
    i++;
  }
  i = 0;
  auto net = findNetworkDevices();
  for(auto &dev : net){
    std::string name = std::to_string(i);
    std::cout << "Adding network monitor for device: " << dev << " -->" << name << std::endl;
    networkGroup.networkMonitors.emplace_back(dev, &networkGroup, name, "Network monitor");
  #ifdef ENABLE_LOGGING
    networkGroup.loggingModules.emplace_back(&networkGroup, name, "Network monitor log");
  #endif
    i++;
  }
  ProcessHandler::setupHandler();
}

void WatchdogServer::defineConnections() {
  systemInfo.findTag("CS").connectTo(cs[systemInfo.getName()]);
  trigger.tick >> systemInfo.trigger;
  cs["configuration"]("UpdateTime") >> trigger.period;

	watchdog.findTag("CS").connectTo(cs[watchdog.getName()]);

#ifdef ENABLE_LOGGING
  // allow to set the logFile name -> other modules will use the same log file
  watchdogLog.findTag("CS_OPTIONAL").connectTo(cs[watchdog.getName()]);
	watchdog.logging.connectTo(watchdogLog.input);
  watchdogLog.findTag("CS").connectTo(cs[watchdog.getName()]);

  watchdogLog.config.logFile >> watchdogLogFile.config.logFile;
  trigger.tick >> watchdogLogFile.trigger;
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
  systemInfo.status.maxMem >> watchdog.input.maxMem;
  trigger.tick >> watchdog.trigger;

  processGroup.findTag("CS").connectTo(cs["processes"]);
  for(auto &item : processGroup.processes) {
    systemInfo.info.ticksPerSecond >> item.input.ticksPerSecond;
    systemInfo.status.uptime_secTotal >> item.input.sysUpTime;
    systemInfo.status.startTime >> item.input.sysStartTime;
    systemInfo.status.maxMem >> item.input.maxMem;
    trigger.tick >> item.trigger;

#ifdef ENABLE_LOGGING
    item.logging.connectTo((*log).input);
    watchdogLog.config.logFile >> (*log).config.logFile;
    item.config.externalLogfile >> (*logExternal).config.logFile;
    trigger.tick >> (*logExternal).trigger;
    log++;
    logExternal++;
#endif
  }
#ifdef ENABLE_LOGGING
  log = filesystemGroup.loggingModules.begin();
#endif
  filesystemGroup.findTag("CS").connectTo(cs["filesystem"]);
  for(auto &item : filesystemGroup.fsMonitors){
    trigger.tick >> item.trigger;
#ifdef ENABLE_LOGGING
    item.logging.connectTo((*log).input);
    watchdogLog.config.logFile >> (*log).config.logFile;
    log++;
#endif
  }
#ifdef ENABLE_LOGGING
  log = networkGroup.loggingModules.begin();
#endif
  networkGroup.findTag("CS").connectTo(cs["network"]);
  for(auto &item : networkGroup.networkMonitors){
    trigger.tick >> item.trigger;
#ifdef ENABLE_LOGGING
    item.logging.connectTo((*log).input);
    watchdogLog.config.logFile >> (*log).config.logFile;
    log++;
#endif
  }

  /*
   *  Micro DAQ system
   */
  if(config.get<int>("enableMicroDAQ") != 0) {
    microDAQ = ctk::MicroDAQ<uint64_t>(this, "MicroDAQ", "Local ringbuffer DAQ system");
    microDAQ.addSource(watchdog.findTag("DAQ"), watchdog.getName());
    microDAQ.addSource(systemInfo.findTag("DAQ"), systemInfo.getName());
    for(auto &item : processGroup.processes) {
      microDAQ.addSource(item.findTag("DAQ"), "processes/"+item.getName());
    }
    for(auto &item : networkGroup.networkMonitors) {
      microDAQ.addSource(item.findTag("DAQ"), "network/" + item.getName());
    }
    for(auto &item : filesystemGroup.fsMonitors) {
      microDAQ.addSource(item.findTag("DAQ"), "filesystem/" + item.getName());
    }
    // configuration of the DAQ system itself
    trigger.tick >> microDAQ.trigger;
    microDAQ.findTag("MicroDAQ.CONFIG").connectTo(cs["microDAQ"]);
  }

  if(config.get<uint>("enableServerHistory") != 0){
    int serverHistroyLength = config.get<int>("serverHistoryLength");
    if(serverHistroyLength != 0)
      history = ctk::history::ServerHistory{this, "History", "History", serverHistroyLength};
    else
      history = ctk::history::ServerHistory{this, "History", "History", 100};
    history.addSource(systemInfo.findTag("History"), "history/" + systemInfo.getName());
    history.addSource(watchdog.findTag("History"), "history/" + watchdog.getName());
    for(auto &item : processGroup.processes) {
      history.addSource(item.findTag("History"), "history/processes/" + item.getName());
    }
    for(auto &item : filesystemGroup.fsMonitors){
      history.addSource(item.findTag("History"), "history/filesystem/" + item.getName());
    }
    for(auto &item : networkGroup.networkMonitors){
      history.addSource(item.findTag("History"), "history/network/" + item.getName());
    }
    history.findTag("CS").connectTo(cs);
  }
//  dumpConnections();
}

