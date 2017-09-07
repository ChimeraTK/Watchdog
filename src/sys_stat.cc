/*
 * sys_stat.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include <fstream>
#include <string>
#include <stdlib.h>

#include <proc/readproc.h>
#include <proc/sysinfo.h>

#include "sys_stat.h"

bool Helper::checkIsRunning(const std::string &cmd, int &PID, proc_t* data){
	PROCTAB* proc = openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLSTATUS);
	PID = -1;
	while (proc_t* proc_info = readproc(proc, NULL)) {
		std::string tmp(proc_info->cmd);
		if(tmp.find(cmd) != std::string::npos){
			PID = proc_info->tid;
			if(data != nullptr)
				data = proc_info;
	  }
	  freeproc(proc_info);
	}
	closeproc(proc);
	if(PID < 0)
		return false;
	else
		return true;
}

int Helper::findPID(const std::string& cmd){
	int PID;
	checkIsRunning(cmd, PID);
	return PID;
}

std::shared_ptr<proc_t> Helper::getInfo(const size_t &PID){
	PROCTAB* proc = openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLSTATUS);
	std::shared_ptr<proc_t> result(nullptr);
	while (proc_t* proc_info = readproc(proc, NULL)) {
		std::string tmp(proc_info->cmd);
		if(proc_info->tid == PID){
			result.reset(new proc_t(*proc_info));
	  }
	  freeproc(proc_info);
	}
	closeproc(proc);
	return result;
}

SysInfo::SysInfo(){
	cpu_info_read();
	mem_info_read();
}

bool SysInfo::lookup(const std::string &line, const std::string &pattern){
	std::size_t found = line.find(pattern);
	if (found == std::string::npos)
		return false;

	std::string pat = line.substr(0,line.find(":"));
	std::string val = line.substr(line.find(":")+1,line.length()-1);

	nfo.fill(val,pattern);
	return true;
}

void SysInfo::cpu_info_read(){
	std::ifstream procfile("/proc/cpuinfo");
	if (!procfile.is_open())
		throw std::runtime_error("Failed to open /proc/cpuinfo");
	std::string line;
	for(auto pat = nfo.ibegin; pat != nfo.iend; pat++){
		while (std::getline (procfile,line)) {
			if(lookup(line, pat->first)) break;
		}
		procfile.seekg (0, std::ios::beg);
	}
	procfile.close();

	std::ifstream cpufile("/sys/devices/system/cpu/present");
	if(!cpufile.is_open())
		throw std::runtime_error("Failed to open /sys/devices/system/cpu/present");
	std::getline (cpufile,line);
	nfo.count = std::atoi(line.substr(2,2).c_str());
}

void SysInfo::mem_info_read(){
	loadavg (&mem_.loadavg [0], &mem_.loadavg [1], &mem_.loadavg [2]);

	// get system wide memory usage

	meminfo ();

	mem_.maxmem     = kb_main_total;
	mem_.freemem    = kb_main_free;
	mem_.cachedmem  = kb_main_cached;
	mem_.usedmem    = mem_.maxmem - mem_.freemem;
	mem_.maxswap    = kb_swap_total;
	mem_.freeswap   = kb_swap_free;
	mem_.usedswap   = mem_.maxswap - mem_.freeswap;

	// get system uptime
	double    uptime_secs;
	double    idle_secs;
	uptime (&uptime_secs, &idle_secs);

	mem_.uptime_sec      = (long) uptime_secs;
	mem_.uptime_days     = mem_.uptime_sec / 86400 ;
	mem_.uptime_day_hour = (mem_.uptime_sec - (mem_.uptime_days * 86400)) / 3600;
	mem_.uptime_day_mins = (mem_.uptime_sec - (mem_.uptime_days * 86400) -
						   (mem_.uptime_day_hour * 3600)) / 60;
}

