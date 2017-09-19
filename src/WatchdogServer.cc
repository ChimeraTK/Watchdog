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
	systemInfo.maxMem >> cs["SYS"]("maxMem");
	systemInfo.freeMem >> cs["SYS"]("freeMem");
	systemInfo.cachedMem >> cs["SYS"]("cachedMem");
	systemInfo.usedMem >> cs["SYS"]("usedMem");
	systemInfo.maxSwap >> cs["SYS"]("maxSwap");
	systemInfo.freeSwap >> cs["SYS"]("freeSwap");
	systemInfo.usedSwap >> cs["SYS"]("usedSwap");
	systemInfo.uptime_sec >> cs["SYS"]("uptime_s");
	systemInfo.uptime_days >> cs["SYS"]("uptime_d");
	systemInfo.uptime_day_hour >> cs["SYS"]("uptime_h");
	systemInfo.uptime_day_mins >> cs["SYS"]("uptime_min");
	systemInfo.loadAvg >> cs["SYS"]("loadAvg");
	systemInfo.loadAvg5 >> cs["SYS"]("loadAvg5");
	systemInfo.loadAvg15 >> cs["SYS"]("loadAvg15");
//	systemInfo.cpu_use >> cs["SYS"]("cpuUsage");


	cs["PROCESS"]("enableProcess") >> process.startProcess;
	cs["PROCESS"]("CMD") >> process.processCMD;
#ifdef BOOST_1_64
	process.processPID >> cs["PROCESS"]("processPID");
#else
	cs["PROCESS"]("Path") >> process.processPath;
#endif

	process.processRunning >> cs["PROCESS"]("Status");
	process.processNFailed >> cs["PROCESS"]("NFails");
	process.processPID >> cs["PROCESS"]("PID");
	process.processRestarts >> cs["PROCESS"]("Restarts Number of unexpected restarts.");
	dumpConnections();
}

