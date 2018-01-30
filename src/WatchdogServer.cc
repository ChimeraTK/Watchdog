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
    update.read();
    trigger = trigger + 1;
    trigger.write();
    if(update < 1){
      sleep(3);
    } else {
      sleep(update);
    }
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
#ifdef ENABLE_LOGGING
          processesLog.emplace_back(this, (nameAttr->get_value() + "-Log").data(), "process log");
          processesLogExternal.emplace_back(this, (nameAttr->get_value() + "-LogExternal").data(), "process external log");
#endif
        }
      }

    }

  } catch(xmlpp::exception &e) {
    std::cerr << "Error opening the xml file '" + fileName + "': " + e.what()
        << std::endl;
    std::cout << "I will create only one process named PROCESS..." << std::endl;
    processes.emplace_back(this, "PROCESS", "Test process");
#ifdef ENABLE_LOGGING
    processesLog.emplace_back(this, "PROCESS", "Test process log");
    processesLogExternal.emplace_back(this, "PROCESS", "Test process external log");
#endif
  }
}

void WatchdogServer::defineConnections() {
  for(auto it = systemInfo.strInfos.begin(), ite = systemInfo.strInfos.end();
      it != ite; it++) {
    it->second >> cs["SYS"](space2underscore(it->first));
  }
  systemInfo.findTag("CS").connectTo(cs["SYS"]);
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
  cs[systemInfoLog.getName()]("SetLogFile") >> systemInfoLog.logFile;
  cs[systemInfoLog.getName()]("SetLogTailLength") >> systemInfoLog.tailLength;
  cs[systemInfoLog.getName()]("SetTargetStream") >> systemInfoLog.targetStream;
  cs[systemInfoLog.getName()]("SetLogLevel") >> systemInfoLog.logLevel;
  systemInfoLog.findTag("CS").connectTo(cs[systemInfoLog.getName()]);


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

  dumpConnections();

}

