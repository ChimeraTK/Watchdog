/*
 * SystemInfo.h
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#ifndef INCLUDE_SYSTEMINFOMODULE_H_
#define INCLUDE_SYSTEMINFOMODULE_H_

#undef GENERATE_XML
#include <ChimeraTK/ApplicationCore/ApplicationCore.h>

#include "sys_stat.h"

namespace ctk = ChimeraTK;

class SystemInfoModule : public ctk::ApplicationModule {
private:
	SysInfo sysInfo;

public:
	SystemInfoModule(EntityOwner *owner, const std::string &name, const std::string &description,
            bool eliminateHierarchy=false, const std::unordered_set<std::string> &tags={});
    std::map<std::string,ctk::ScalarOutput<std::string> > strInfos;
    //ToDo: Implement the following as unsigned long!
    ctk::ScalarOutput<int> maxMem{this, "maxMem", "kB", "Maximum available memory"};
    ctk::ScalarOutput<int> freeMem{this, "freeMem", "kB", "Free memory"};
    ctk::ScalarOutput<int> cachedMem{this, "cachedMem", "kB", "Cached memory"};
    ctk::ScalarOutput<int> usedMem{this, "usedMem", "kB", "Used memory"};
    ctk::ScalarOutput<int> maxSwap{this, "maxSwap", "kB", "Swap size"};
    ctk::ScalarOutput<int> freeSwap{this, "freeSwap", "kB", "Free swap"};
    ctk::ScalarOutput<int> usedSwap{this, "usedSwap", "kB", "Used swap"};
    //ToDo: Implement the following as long!
	ctk::ScalarOutput<int> uptime_sec{this, "uptimeSec", "s", "Uptime"};
	ctk::ScalarOutput<int> uptime_days{this, "uptimeDays", "day", "Days up"};
	ctk::ScalarOutput<int> uptime_day_hour{this, "uptimeHours", "h", "Hours up"};
	ctk::ScalarOutput<int> uptime_day_mins{this, "uptimeMin", "min", "Minutes up"};
//    ctk::ScalarOutput<float>              cpu_use{this, "cpuUsage", "", "CPU usage"};
    ctk::ScalarOutput<double>             loadAvg{this, "loadAvg", "", "Average load within last min"};
    ctk::ScalarOutput<double>             loadAvg5{this, "loadAvg5", "", "Average load within last 5min"};
	ctk::ScalarOutput<double>             loadAvg15{this, "loadAvg15", "", "Average load within last 15min"};

    void mainLoop();
};


#endif /* INCLUDE_SYSTEMINFOMODULE_H_ */
