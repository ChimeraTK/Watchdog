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

/**
 * This module is used to read system parameter.
 * Some of them are static and only read once (e.g. processor model).
 * Others are watched continuously (e.g. uptime, work load...).
 * \todo Implement proper data types instead of using int for all of them!
 */
class SystemInfoModule: public ctk::ApplicationModule {
private:
  SysInfo sysInfo;
  // needed for cpu usage calculation
  struct cpu{
    unsigned long long totalUser;
    unsigned long long totalUserLow;
    unsigned long long totalSys;
    unsigned long long totalIdle;
    cpu(unsigned long long totUser = 0, unsigned long long totUserLow = 0,
        unsigned long long TotSys = 0, unsigned long long TotIdle = 0){
      totalUser = totUser;
      totalUserLow = totUserLow;
      totalSys = TotSys;
      totalIdle = TotIdle;
    }
  };
  std::vector<cpu> lastInfo;
public:
  SystemInfoModule(EntityOwner *owner, const std::string &name,
      const std::string &description, bool eliminateHierarchy = false,
      const std::unordered_set<std::string> &tags = { });

  ctk::ScalarPushInput<int> trigger{this, "trigger", "", "Trigger used to update the watchdog"};
  /**
   * \name Static system information (read only once)
   * @{
   */
  std::map<std::string, ctk::ScalarOutput<std::string> > strInfos;
  ctk::ScalarOutput<int> ticksPerSecond{this, "ticksPerSecond", "Hz" ,"Number of clock ticks per second"}; ///< Number of clock ticks per second
  ctk::ScalarOutput<int> nCPU{this, "nCPU", "" , "Number of CPUs", {"CS", "SYS"}};
  /** @} */
  /**
   * \name Non static system information
   * @{
   */
  //\todo: Implement the following as unsigned long!
  ctk::ScalarOutput<int> maxMem { this, "maxMem", "kB",
      "Maximum available memory" , {"CS", "SYS"}};
  ctk::ScalarOutput<int> freeMem { this, "freeMem", "kB", "Free memory" , {"CS", "SYS"}};
  ctk::ScalarOutput<int> cachedMem { this, "cachedMem", "kB", "Cached memory" , {"CS", "SYS"}};
  ctk::ScalarOutput<int> usedMem { this, "usedMem", "kB", "Used memory" , {"CS", "SYS"}};
  ctk::ScalarOutput<int> maxSwap { this, "maxSwap", "kB", "Swap size" , {"CS", "SYS"}};
  ctk::ScalarOutput<int> freeSwap { this, "freeSwap", "kB", "Free swap" , {"CS", "SYS"}};
  ctk::ScalarOutput<int> usedSwap { this, "usedSwap", "kB", "Used swap" , {"CS", "SYS"}};
  //\todo: Implement the following as long!
  ctk::ScalarOutput<int> uptime_sec { this, "uptimeSec", "s", "Uptime" , {"CS", "SYS"}};
  ctk::ScalarOutput<int> uptime_days { this, "uptimeDays", "day", "Days up" , {"CS", "SYS"}};
  ctk::ScalarOutput<int> uptime_day_hour { this, "uptimeHours", "h", "Hours up" , {"CS", "SYS"}};
  ctk::ScalarOutput<int> uptime_day_mins { this, "uptimeMin", "min",
      "Minutes up" , {"CS", "SYS"}};
  std::vector<std::unique_ptr<ctk::ScalarOutput<double> > > cpu_use;
  ctk::ScalarOutput<double> loadAvg { this, "loadAvg", "",
      "Average load within last min" , {"CS", "SYS"}};
  ctk::ScalarOutput<double> loadAvg5 { this, "loadAvg5", "",
      "Average load within last 5min" , {"CS", "SYS"}};
  ctk::ScalarOutput<double> loadAvg15 { this, "loadAvg15", "",
      "Average load within last 15min" , {"CS", "SYS"}};
  /** @} */
  void mainLoop();

  void calculatePCPU();

  void readCPUInfo(std::vector<cpu> &vcpu);
};

#endif /* INCLUDE_SYSTEMINFOMODULE_H_ */
