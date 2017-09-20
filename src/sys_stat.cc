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

#include <stdlib.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <signal.h>
#include <iterator>
#include <boost/algorithm/string.hpp>

std::string space2underscore(std::string text){
	std::transform(text.begin(), text.end(), text.begin(), [](char ch) {
	    return ch == ' ' ? '_' : ch;
	});
	return text;
}

std::vector<std::string> split_arguments(const std::string &arguments){
	std::vector<std::string> strs;
	boost::split(strs, arguments, boost::is_any_of("\t "));
	for(auto st : strs){
		std::cout << st.c_str() << std::endl;
	}
	return strs;
}



int ProcessHandler::startProcess(const std::string &path, const std::string &cmd){
	if(path.empty() || cmd.empty()){
		throw std::runtime_error("Path or command not set before starting a proccess!");
	}
	pid_t p = fork();
	pid_t child;
	std::stringstream ss;
	int pid;
	if(p == 0){
		// Don't throw in the child since the parent will not catch it
		child = (int)getpid();
		printf("child running: %d\n", (int)child);
		std::ofstream pidFile;
		pidFile.open("pid");
		if(!pidFile.is_open()){
			pidFile.close();
			throw std::runtime_error("Failed to create PID file.");
		} else {
			pidFile << child;
			pidFile.close();
		}
		std::string path_copy = path;
		if(path.back() != '/')
			path_copy.append(std::string("/").c_str());
		if(chdir(path.c_str())){
			std::stringstream ss;
			ss << "Failed to change to directory: " << path;
			std::cout << ss.str() << std::endl;
			_exit(0);
		}
		auto args = split_arguments(cmd);
		char * exec_args[1024];
		int arg_count = 0;
		std::cout << "Going to call: execv(\"" << (path_copy+args.at(0)).c_str();
		for (size_t x = 0; x < args.size(); x++) {
		   exec_args[arg_count++] = strdup(args[x].c_str());
		   std::cout << "\", \"" << exec_args[x];
		}
		exec_args[arg_count++] = 0; // tell it when to stop!
		std::cout << "\", \"NULL\")" << std::endl;
		execv((path_copy+args.at(0)).c_str(), exec_args);
//		execl((path + std::string("/") + cmd).c_str(), cmd.c_str(), NULL);
		_exit(0);
	} else {
		// Ignore the signal SIGCHLD by the parent since after killing the child it will hang in defunct status since the kernel
		// thinks that the parent expects a status
		signal(SIGCHLD, SIG_IGN);
		sleep(1);
		if (checkStatus(pid))
		{
			std::cout << "PID was read:" << pid <<std::endl;
			remove("pid");
		} else {
			throw std::runtime_error("Process is not started!");
		}
	}
	return pid;
}

bool ProcessHandler::checkStatus(int &PID){
	std::ifstream testFile;
	testFile.open("pid");
	if(testFile.is_open()){
		std::string line;
		getline (testFile,line);
		PID = atoi(line.c_str());
		testFile.close();
		return true;
	} else {
		testFile.close();
		return false;
	}
}

void ProcessHandler::killProcess(const int &PID){
	std::cout << "killing children" << std::endl;
	if(!isProcessRunning(PID)){
		std::cout << "No process with PID " << PID << " running!" << std::endl;
		return;
	}
	std::cout << "Going to kill process: " << PID << std::endl;
	std::stringstream ss;
	// use minus sign for the PID in order to kill also possible sub processes belonging to the same PGID!
	ss << "/bin/kill -SIGINT -" <<  PID;
	std::cout << "String: " << ss.str() << std::endl;
	// kill returns 0 even if the process is not found, so no need to return the result
	system(ss.str().c_str());
}

bool ProcessHandler::isProcessRunning(const int &PID){
	PROCTAB* proc = openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLSTATUS);
	proc_t* proc_info;
	while (proc_info = readproc(proc, NULL)) {
		if(PID == proc_info->tid){
		  freeproc(proc_info);
		  return true;
		}
	}
	freeproc(proc_info);
	return false;
}

std::shared_ptr<proc_t> ProcessHandler::getInfo(const size_t &PID){
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

