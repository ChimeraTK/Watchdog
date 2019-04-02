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
    auto strProcesses = config.get<std::vector<std::string>>("processes");
    size_t n = 0;
    for(auto &processName : strProcesses) {
      std::cout << "Adding process: " << processName << std::endl;
      if(config.get<uint>("enableServerHistory") != 0){
        processGroup.processes.emplace_back(&processGroup, std::to_string(n).c_str(), "process", true);
      } else {
        processGroup.processes.emplace_back(&processGroup, std::to_string(n).c_str(), "process");
      }
      processGroup.processes.back().logging = nullptr;
#ifdef ENABLE_LOGGING
      processesLog.emplace_back(this, processName + "-Log", "process log");
      processesLogExternal.emplace_back(this, processName + "-LogExternal", "process external log");
#endif
      n++;
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
    processGroup.processes.back().logging = nullptr;
#ifdef ENABLE_LOGGING
    processesLog.emplace_back(this, "PROCESS", "Process log");
    processesLogExternal.emplace_back(this, "PROCESS", "Process external log");
#endif
  }
  size_t i =0;
  auto fs = findMountPoints();
  for(auto &mountPoint : fs){
    std::string name = std::to_string(i);
    std::cout << "Adding filesystem monitor for: " << mountPoint.first << " mounted at: " << mountPoint.second << " -->" << name << std::endl;
    filesystemGroup.fsMonitors.emplace_back(mountPoint.first, mountPoint.second, &filesystemGroup, name, "Filesystem monitor");
#ifdef ENABLE_LOGGING
    processesLog.emplace_back(this, name + "-Log", "File system monitor log");
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
    processesLog.emplace_back(this, name + "-Log", "Network monitor log");
  #endif
    i++;
  }
  ProcessHandler::setupHandler();
}

void WatchdogServer::defineConnections() {
  for(auto it = systemInfo.strInfos.begin(), ite = systemInfo.strInfos.end();
      it != ite; it++) {
    it->second >> cs["system"](space2underscore(it->first));
  }
  systemInfo.findTag("CS").connectTo(cs[systemInfo.getName()]);
  trigger.tick >> systemInfo.trigger;
  cs["configuration"]("UpdateTime") >> trigger.period;

	watchdog.findTag("CS").connectTo(cs[watchdog.getName()]);
#ifdef ENABLE_LOGGING
  watchdog.findTag("Logging").connectTo(watchdogLog);

	cs[watchdog.getName()]("SetLogFile") >> watchdogLog.logFile;
  watchdogLog.findTag("CS").connectTo(cs[watchdog.getName()]);

  cs[watchdog.getName()]("SetLogFile") >> watchdogLogFile.logFile;
  trigger.tick >> watchdogLogFile.trigger;
  watchdogLogFile.findTag("CS").connectTo(cs[watchdog.getName()]);

  systemInfo.findTag("Logging").connectTo(systemInfoLog);
  cs[watchdog.getName()]("SetLogFile") >> systemInfoLog.logFile;
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
  systemInfo.maxMem >> watchdog.maxMem;
  trigger.tick >> watchdog.trigger;

  processGroup.findTag("CS").connectTo(cs["processes"]);
  for(auto &item : processGroup.processes) {
    systemInfo.ticksPerSecond >> item.ticksPerSecond;
    systemInfo.uptime_secTotal >> item.sysUpTime;
    systemInfo.startTime >> item.sysStartTime;
    systemInfo.maxMem >> item.maxMem;
    trigger.tick >> item.trigger;

#ifdef ENABLE_LOGGING
    item.message >> (*log).message;
    item.messageLevel >> (*log).messageLevel;
    cs[watchdog.getName()]("SetLogFile") >> (*log).logFile;
    (*log).findTag("CS").connectTo(cs[item.getName()]);
    item.processExternalLogfile >> (*logExternal).logFile;
    trigger.tick >> (*logExternal).trigger;
    (*logExternal).findTag("CS").connectTo(cs[item.getName()]);

    log++;
    logExternal++;
#endif
  }
  filesystemGroup.findTag("CS").connectTo(cs["filesystem"]);
  for(auto &item : filesystemGroup.fsMonitors){
    trigger.tick >> item.trigger;
#ifdef ENABLE_LOGGING
    item.message >> (*log).message;
    item.messageLevel >> (*log).messageLevel;
    cs[watchdog.getName()]("SetLogFile") >> (*log).logFile;
    (*log).findTag("CS").connectTo(cs[item.getName()]);
    log++;
#endif
  }
  networkGroup.findTag("CS").connectTo(cs["network"]);
  for(auto &item : networkGroup.networkMonitors){
    trigger.tick >> item.trigger;
#ifdef ENABLE_LOGGING
    item.message >> (*log).message;
    item.messageLevel >> (*log).messageLevel;
    cs[watchdog.getName()]("SetLogFile") >> (*log).logFile;
    (*log).findTag("CS").connectTo(cs[item.getName()]);
    log++;
#endif
  }

  /*
   *  Micro DAQ system
   */
  if(config.get<int>("enableMicroDAQ") != 0) {
    conversion = ConversionModule{this, "Conversion", "Module used to do the trigger conversion"};
    //\FixMe: Remove once MicroDAQ is using uint
    trigger.tick >> conversion.triggerIn;
    microDAQ = ctk::MicroDAQ(this, "MicroDAQ", "Local ringbuffer DAQ system");
    microDAQ.addSource(watchdog.findTag("DAQ"), watchdog.getName());
    microDAQ.addSource(systemInfo.findTag("DAQ"), systemInfo.getName());
    for(auto &item : processGroup.processes) {
      microDAQ.addSource(item.findTag("DAQ"), item.getName());
    }
    for(auto &item : networkGroup.networkMonitors) {
      microDAQ.addSource(item.findTag("DAQ"), item.getName());
    }
    for(auto &item : filesystemGroup.fsMonitors) {
      microDAQ.addSource(item.findTag("DAQ"), item.getName());
    }
    // configuration of the DAQ system itself
    conversion.triggerOut >> microDAQ.trigger;
    microDAQ.findTag("MicroDAQ.CONFIG").connectTo(cs["MicroDAQ"]);

    cs["MicroDAQ"]("nMaxFiles") >> microDAQ.nMaxFiles;
    cs["MicroDAQ"]("nTriggersPerFile") >> microDAQ.nTriggersPerFile;
    cs["MicroDAQ"]("enable") >> microDAQ.enable;
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
      history.addSource(item.findTag("History"), "history/" + item.getName());
    }
    for(auto &item : filesystemGroup.fsMonitors){
      history.addSource(item.findTag("History"), "history/" + item.getName());
    }
    for(auto &item : networkGroup.networkMonitors){
      history.addSource(item.findTag("History"), "history/" + item.getName());
    }
    history.findTag("CS").connectTo(cs);
  }
  dumpConnections();

  std::cout << systemInfo.getOwner() << std::endl;
  auto l = networkGroup.getSubmoduleList();
  std::cout << networkGroup.getOwner() << std::endl;
  for(auto mod : l){
    std::cout << mod->getOwner() << std::endl;
  }
  l = networkGroup.getSubmoduleList();
  std::cout << networkGroup.getOwner() << std::endl;
  for(auto mod : l){
    std::cout << mod->getOwner() << std::endl;
  }
}

