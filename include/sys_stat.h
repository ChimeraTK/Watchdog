/*
 * sys_stst.h
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#ifndef INCLUDE_SYS_STAT_H_
#define INCLUDE_SYS_STAT_H_

#include <map>
#include <string.h>
#include <memory>
#include <proc/readproc.h>

#undef GENERATE_XML
#include <ChimeraTK/ApplicationCore/ScalarAccessor.h>

struct ProcessHandler{
private:
	bool checkStatus(int &PID);
public:
	int startProcess(const std::string &path, const std::string &cmd);
	void killProcess(const int &PID);
	bool isProcessRunning(const int &PID);
	std::shared_ptr<proc_t> getInfo(const size_t &PID);
};

std::string space2underscore(std::string text);

/**
 * Struct used to store system information that changes during
 * runtime, e.g. average system load, uptime ...
 */
struct load_mem {

    double             loadavg [3];
    unsigned long long freemem;
    unsigned long long maxmem;
	unsigned long long cachedmem;
    unsigned long long usedmem;
    unsigned long long freeswap;
    unsigned long long maxswap;
    unsigned long long usedswap;
	long               uptime_sec;
	long               uptime_days;
	long               uptime_day_hour;
	long               uptime_day_mins;
    float              cpu_use;
};

/**
 * Struct used to store static system information that should not change
 * during runtime.
 */
struct cpu_info {
	std::map<std::string,std::string> sysData;
	std::map<std::string,std::string>::iterator ibegin;
	std::map<std::string,std::string>::iterator iend;
	int count;
	cpu_info(){
		sysData["vendor"] = "";
		sysData["family"] = "";
		sysData["cpu family"] = "";
		sysData["model"] = "";
		sysData["model name"] = "";
		sysData["stepping"] = "";
		sysData["cpu MHz"] = "";
		sysData["bogomips"] = "";
		ibegin = sysData.begin();
		iend = sysData.end();
		count = -1;
	};
	void fill(const std::string &val, const std::string &pattern){
		if(sysData.count(pattern))
			sysData.at(pattern) = val;
	}
	std::string getInfo(const std::string &pattern){
		return sysData.at(pattern);
	}

};

class SysInfo{
private:
	bool lookup(const std::string &line, const std::string &pattern);
	/**
	 * Read information about the system.
	 * \throws runtime_error Exception is thrown in case one of the /proc/ files could not be read.
	 */
	void cpu_info_read();
	/**
	 * Read memory information and system load.
	 * This is done using the libprocps library.
	 */
	void mem_info_read();
public:
	cpu_info nfo;
	load_mem mem_;

	SysInfo();
	/**
	 * Update information stored in the load_mem object.
	 * Data stored in the cpu_info object are static and do not need to
	 * be updated.
	 */
	void Update(){mem_info_read();}
};


#endif /* INCLUDE_SYS_STAT_H_ */
