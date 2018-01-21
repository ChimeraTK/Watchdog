/*
 * WatchdogServer.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "WatchdogServer.h"

#include <libxml++/libxml++.h>

void TimerModule::mainLoop() {
  while(true) {
    /**
     * \internal
     *  Setting an interruption point is included in read() methods of ChimeraTK but not in write()!
     *  Thus set it by hand here!
     */
    boost::this_thread::interruption_point();
    trigger = trigger + 1;
    trigger.write();
    sleep(2);
  }
}

WatchdogServer::WatchdogServer() :
    Application("WatchdogServer") {
  std::string fileName("WatchdogServer_processes.xml");
  // parse the file into a DOM structure
  xmlpp::DomParser parser;
  try {
    parser.parse_file(fileName);
    // get root element
    const auto root = parser.get_document()->get_root_node();
    if(root->get_name() != "watchdog") {
      throw xmlpp::exception(
          "Expected 'watchdog' tag instead of: " + root->get_name());
    }

    // parsing loop
    for(const auto& child : root->get_children()) {
      // cast into element, ignore if not an element (e.g. comment)
      const xmlpp::Element *element = dynamic_cast<const xmlpp::Element*>(child);
      if(!element)
        continue;
      // parse the element
      if(element->get_name() == "process") {
        auto nameAttr = element->get_attribute("name");
        if(!nameAttr) {
          std::cerr
              << "Missing name attribute of 'process' tag. Going to skip one the process elements in the xml file: "
              << fileName << std::endl;
        } else {
          processes.emplace_back(this, nameAttr->get_value().data(), "process");
          processesLog.emplace_back(this, nameAttr->get_value().data(), "process log");
          processesLogExternal.emplace_back(this, nameAttr->get_value().data(), "process external log");
        }
      }

    }

  } catch(xmlpp::exception &e) {
    std::cerr << "Error opening the xml file '" + fileName + "': " + e.what()
        << std::endl;
    std::cout << "I will create only one process named PROCESS..." << std::endl;
    processes.emplace_back(this, "PROCESS", "Test process");
    processesLog.emplace_back(this, "PROCESS", "Test process log");
    processesLogExternal.emplace_back(this, "PROCESS", "Test process external log");
  }
}

void WatchdogServer::defineConnections() {
  std::cout << "Map size is: " << systemInfo.strInfos.size() << std::endl;
  for(auto it = systemInfo.strInfos.begin(), ite = systemInfo.strInfos.end();
      it != ite; it++) {
#ifdef DEBUG
    std::cout << "Adding system info: " << space2underscore(it->first)
        << std::endl;
#endif
    it->second >> cs["SYS"](space2underscore(it->first));
  }
  systemInfo.findTag("CS").connectTo(cs["SYS"]);
  timer.connectTo(systemInfo);

	watchdog.findTag("CS").connectTo(cs[watchdog.getName()]);

	watchdogLog.findTag("CS").connectTo(cs[watchdog.getName()]);
	cs[watchdog.getName()]("LogLevel") >> watchdogLog.logLevel;
	cs[watchdog.getName()]("LogFile") >> watchdogLog.logFile;
	cs[watchdog.getName()]("LogfileTailLength") >> watchdogLog.tailLength;
	watchdog.findTag("Logging").connectTo(watchdogLog);

	systemInfo.ticksPerSecond >> watchdog.ticksPerSecond;
  systemInfo.uptime_secTotal >> watchdog.sysUpTime;
  systemInfo.startTime >> watchdog.sysStartTime;
  timer.trigger >> watchdog.trigger;

  std::cout << "Adding " << processes.size() << " processes..." << std::endl;
  auto log = processesLog.begin();
  auto logExternal = processesLogExternal.begin();
  for(auto &item : processes) {
    cs[item.getName()]("enableProcess") >> item.enableProcess;
    cs[item.getName()]("SetCMD") >> item.processSetCMD;
    cs[item.getName()]("SetPath") >> item.processSetPath;
    cs[item.getName()]("SetLogfile") >> item.processSetLogfile;
    cs[item.getName()]("killSig") >> item.killSig;
    cs[item.getName()]("pidOffset") >> item.pidOffset;

    item.findTag("Logging").connectTo(*log);
    cs[item.getName()]("LogLevel") >> (*log).logLevel;
    cs[item.getName()]("LogFile") >> (*log).logFile;
    cs[item.getName()]("LogfileTailLength") >> (*log).tailLength;
    cs[item.getName()]("LogfileExternal") >> (*logExternal).logFile;
    cs[item.getName()]("LogfileExternalTailLength") >> (*logExternal).tailLength;
    (*log).findTag("CS").connectTo(cs[item.getName()]);

    item.findTag("CS").connectTo(cs[item.getName()]);
    systemInfo.ticksPerSecond >> item.ticksPerSecond;
    systemInfo.uptime_secTotal >> item.sysUpTime;
    systemInfo.startTime >> item.sysStartTime;
    timer.trigger >> item.trigger;
    log++;
    logExternal++;
  }

  dumpConnections();

}

