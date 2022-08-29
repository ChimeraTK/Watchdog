// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

/*
 * WatchdogServer.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "WatchdogServer.h"

#include "boost/filesystem.hpp"
#include "version.h"

#include <fstream>
namespace bfs = boost::filesystem;

/*
 * Select all mount points that mount a physical hard drive on the local machine.
 * The boot partition is not selected.
 * Only devices that include /dev/sd are considered.
 */
std::set<std::pair<std::string, std::string>> findMountPoints() {
  std::set<std::pair<std::string, std::string>> out;
  std::ifstream in("/proc/mounts");
  std::string strin[4];
  int iin[2];
  while(in.good()) {
    in >> strin[0] >> strin[1] >> strin[2] >> strin[3] >> iin[0] >> iin[1];
    bfs::path p(strin[0]);
    if(strin[0].substr(0, 7).compare("/dev/sd") == 0) {
      if(strin[1].substr(0, 6).compare("/boot/") != 0) {
        out.insert(std::make_pair(p.filename().string(), strin[1]));
      }
    }
  }
  in.close();
  return out;
}

/*
 * Select all non virtual network adapter.
 */
std::set<std::string> findNetworkDevices() {
  std::set<std::string> out;
  bfs::path p("/sys/class/net");
  try {
    if(exists(p)) {         // does p actually exist?
      if(is_directory(p)) { // is p a directory?
        for(auto i = bfs::directory_iterator(p); i != bfs::directory_iterator(); i++) {
          if(bfs::canonical(i->path()).string().find("virtual") == std::string::npos)
            out.insert(i->path().filename().string());
        }
      }
    }
  }
  catch(const bfs::filesystem_error& ex) {
    std::cout << ex.what() << '\n';
  }
  return out;
}

WatchdogServer::WatchdogServer() : Application("WatchdogServer") {
  try {
    auto nProcesses = config.get<uint>("Configuration/numberOfProcesses");
    std::cout << "Adding " << nProcesses << " processes." << std::endl;
    for(size_t i = 0; i < nProcesses; i++) {
      std::string processName = std::to_string(i);
      if(config.get<uint>("Configuration/enableServerHistory") != 0) {
        processGroup.processes.emplace_back(&processGroup, processName, "process", true);
      }
      else {
        processGroup.processes.emplace_back(&processGroup, processName, "process");
      }
      processGroup.processesLogExternal.emplace_back(&processGroup, processName, "process external log",
          "/Trigger/tick", "/processes/" + processName + "/config/logfileExternal");
    }
  }
  catch(std::out_of_range& e) {
    std::cerr << "Error in the xml file 'WatchdogServerConfig.xml': " << e.what() << std::endl;
    std::cout << "I will create only one process named PROCESS..." << std::endl;
    if(config.get<uint>("Configuration/enableServerHistory") != 0) {
      processGroup.processes.emplace_back(&processGroup, "0", "Test process", true);
    }
    else {
      processGroup.processes.emplace_back(&processGroup, "0", "Test process");
    }
    processGroup.processesLogExternal.emplace_back(
        &processGroup, "0", "Process external log", "/Trigger/tick", "/processes/0/config/logfileExternal");
  }

  ProcessHandler::setupHandler();
  size_t i = 0;
  auto fs = findMountPoints();
  for(auto& mountPoint : fs) {
    std::string name = std::to_string(i);
    std::cout << "Adding filesystem monitor for: " << mountPoint.first << " mounted at: " << mountPoint.second << " -->"
              << name << std::endl;
    filesystemGroup.fsMonitors.emplace_back(
        mountPoint.first, mountPoint.second, &filesystemGroup, name, "Filesystem monitor");
    i++;
  }
  i = 0;
  auto net = findNetworkDevices();
  for(auto& dev : net) {
    std::string name = std::to_string(i);
    std::cout << "Adding network monitor for device: " << dev << " -->" << name << std::endl;
    networkGroup.networkMonitors.emplace_back(dev, &networkGroup, name, "Network monitor");
    i++;
  }

  logging = logging::LoggingModule{this, "logging", "LoggingModule logging watchdog internal messages"};

  /*
   *  Server DAQ
   */

  if(config.get<ChimeraTK::Boolean>("Configuration/MicroDAQ/enable") == true) {
    daq = ctk::MicroDAQ<uint64_t>{
        this, "microDAQ", "DAQ module", "DAQ", "/Trigger/tick", ctk::HierarchyModifier::none, {"MicroDAQ"}};
  }

  /*
   *  Server history
   */

  if(config.get<uint>("Configuration/enableServerHistory") != 0) {
    uint serverHistroyLength = config.get<uint>("Configuration/serverHistoryLength", (uint)100);
    history = ctk::history::ServerHistory{this, "history", "History", serverHistroyLength, false};
  }
}

void WatchdogServer::initialise() {
  Application::initialise();
  /**
   * Server information
   */
  std::cout << "****************************************************************" << std::endl;
  std::cout << "*** Watchdog server version " << AppVersion::major << "." << AppVersion::minor << "."
            << AppVersion::patch << std::endl;
  warnUnconnectedVariables();
  dumpConnections();
}
