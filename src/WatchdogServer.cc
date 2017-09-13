/*
 * WatchdogServer.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "WatchdogServer.h"

void WatchdogServer::defineConnections(){
	std::cout << "Map size is: " <<   systemInfo.strInfos.size() << std::endl;
	for(auto it = systemInfo.strInfos.begin(), ite =  systemInfo.strInfos.end(); it != ite; it++){
		std::cout << "Adding system info: " << space2underscore(it->first) << std::endl;
		it->second >> cs["SYS"](space2underscore(it->first));
	}
	systemInfo.processor >> cs["SYS"]("processor");

	cs["PROCESS"]("enableProcess") >> process.startProcess;
	cs["PROCESS"]("CMD") >> process.processCMD;
#ifdef BOOST_1_64
	process.processPID >> cs["PROCESS"]("processPID");
#else
	cs["PROCESS"]("Path") >> process.processPath;
#endif

	process.processRunning >> cs["PROCESS"]("Status");
	process.processNFailed >> cs["PROCESS"]("NFails");
	dumpConnections();
}

