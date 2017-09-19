/*
 * SystemInfo.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "SystemInfoModule.h"
#include <proc/readproc.h>
#include <proc/sysinfo.h>

SystemInfoModule::SystemInfoModule(EntityOwner *owner, const std::string &name, const std::string &description,
        bool eliminateHierarchy, const std::unordered_set<std::string> &tags):
		ctk::ApplicationModule(owner, name, description, eliminateHierarchy, tags){
	std::cout << "Init SystemInfoModule" << std::endl;
	for(auto it = sysInfo.nfo.ibegin; it != sysInfo.nfo.iend; it++){
		std::cout << "Adding sysInfor: " << space2underscore(it->first) << std::endl;
		strInfos[it->first].replace( ctk::ScalarOutput<std::string>{this, space2underscore(it->first), "" ,space2underscore(it->first)});
	}
}

void SystemInfoModule::mainLoop(){
	for(auto it = sysInfo.nfo.ibegin; it != sysInfo.nfo.iend; it++){
		strInfos.at(it->first) = it->second;
		strInfos.at(it->first).write();
	}
	double tmp[3];
	while(true){
		/**
		 *  Setting an interruption point is included in read() methods of ChimeraTK but not in write()!
		 *  Thus set it by hand here!
		 */
		boost::this_thread::interruption_point();
		meminfo ();

		maxMem            = kb_main_total;
		freeMem           = kb_main_free;
		cachedMem         = kb_main_cached;
		usedMem           = maxMem - freeMem;
		maxSwap           = kb_swap_total;
		freeSwap          = kb_swap_free;
		usedSwap          = maxSwap - freeSwap;

		maxMem.write();
		freeMem.write();
		cachedMem.write();
		usedMem.write();
		maxSwap.write();
		freeSwap.write();
		usedSwap.write();

//		// get system uptime
		double    uptime_secs;
		double    idle_secs;
		uptime (&uptime_secs, &idle_secs);
		uptime_sec        = (long) uptime_secs;
		uptime_days       = uptime_sec / 86400 ;
		uptime_day_hour   = (uptime_sec - (uptime_days * 86400)) / 3600;
		uptime_day_mins   = (uptime_sec - (uptime_days * 86400) -
		                    (uptime_day_hour * 3600)) / 60;
//		cpu_use =
		loadavg (&tmp[0], &tmp[1], &tmp[2]);
		loadAvg = tmp[0];
		loadAvg5 = tmp[1];
		loadAvg15 = tmp[2];

		uptime_sec.write();
		uptime_days.write();
		uptime_day_hour.write();
		uptime_day_mins.write();
		loadAvg.write();
		loadAvg5.write();
		loadAvg15.write();

		usleep(200000);
	}
}
