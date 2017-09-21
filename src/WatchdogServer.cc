/*
 * WatchdogServer.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "WatchdogServer.h"

#include <libxml++/libxml++.h>

void TimerModule::mainLoop(){
  while(true){
    /**
     *  Setting an interruption point is included in read() methods of ChimeraTK but not in write()!
     *  Thus set it by hand here!
     */
    boost::this_thread::interruption_point();
    trigger = trigger + 1;
    trigger.write();
    sleep(2);
  }
}

WatchdogServer::WatchdogServer(): Application("WatchdogServer") {
	std::string fileName("watchdog_server_processes.xml");
	// parse the file into a DOM structure
	xmlpp::DomParser parser;
	try {
	  parser.parse_file(fileName);
	  // get root element
      const auto root = parser.get_document()->get_root_node();
	  if(root->get_name() != "watchdog") {
		  throw xmlpp::exception("Expected 'watchdog' tag instead of: "+root->get_name());
	  }

	  // parsing loop
	  for(const auto& child : root->get_children()) {
		// cast into element, ignore if not an element (e.g. comment)
		const xmlpp::Element *element = dynamic_cast<const xmlpp::Element*>(child);
		if(!element) continue;
		// parse the element
		if(element->get_name() == "process") {
			auto nameAttr = element->get_attribute("name");
			if(!nameAttr) {
				std::cerr << "Missing name attribute of 'process' tag. Going to skip one the process elements in the xml file: " << fileName << std::endl;
			} else {
				processes[nameAttr->get_value().data()].reset(new ProcessModule{this, nameAttr->get_value().data(), "process"});
				for(const auto&cchild : element->get_children()){
					const xmlpp::Element *eelement = dynamic_cast<const xmlpp::Element*>(cchild);
					if(!eelement) continue;
					if(eelement->get_name() == "path"){
						std::cout << "Children path: " << eelement->get_child_text()->get_content().c_str() << std::endl;
					}
					if(eelement->get_name() == "cmd"){
						std::cout << "Children cmd: " << eelement->get_child_text()->get_content().c_str() << std::endl;
					}
				}
			}
		}

	  }

	} catch(xmlpp::exception &e) {
	  std::cerr << "Error opening the xml file '"+fileName+"': "+e.what() << std::endl;
	  std::cout << "I will create only one process named PROCESS..." << std::endl;
	  processes["PROCESS"].reset(new ProcessModule{this, "PROCESS", "Test process"});
	}
}

void WatchdogServer::defineConnections(){
	std::cout << "Map size is: " <<   systemInfo.strInfos.size() << std::endl;
	for(auto it = systemInfo.strInfos.begin(), ite =  systemInfo.strInfos.end(); it != ite; it++){
		std::cout << "Adding system info: " << space2underscore(it->first) << std::endl;
		it->second >> cs["SYS"](space2underscore(it->first));
	}
	systemInfo.findTag("CS").connectTo(cs["SYS"]);
//	timer.trigger >> systemInfo.trigger;
	timer.connectTo(systemInfo);

//	systemInfo.cpu_use >> cs["SYS"]("cpuUsage");

	std::cout << "Adding " << processes.size() << " processes..." << std::endl;
	for(auto item : processes){
		cs[item.first]("enableProcess") >> item.second->startProcess;
		cs[item.first]("CMD") >> item.second->processCMD;
		cs[item.first]("Path") >> item.second->processPath;
		cs[item.first]("killSig") >> item.second->killSig;
		cs[item.first]("pidOffset") >> item.second->pidOffset;
		item.second->findTag("CS").connectTo(cs[item.first]);
		systemInfo.ticksPerSecond >> item.second->ticksPerSecond;
		systemInfo.uptime_sec >> item.second->uptime;
		timer.trigger >> item.second->trigger;
	}
	dumpConnections();
}

