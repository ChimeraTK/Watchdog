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

#include "Logging.h"

#include "sys_stat.h"
#include <unordered_set>

namespace ctk = ChimeraTK;

/**
 * \brief This module is used to read system parameter.
 *
 * Some of them are static and only read once (e.g. processor model).
 * Others are watched continuously (e.g. uptime, work load...).
 * \todo Implement proper data types instead of using int for all of them!
 */
class SystemInfoModule: public ctk::ApplicationModule {
private:
  SysInfo sysInfo;

  /**
   * CPU usage parameters needed to calculate the cpu usage.
   * These vales are read from \c /proc/stat
   */
  struct cpu {
    unsigned long long totalUser;
    unsigned long long totalUserLow;
    unsigned long long totalSys;
    unsigned long long totalIdle;
    cpu(unsigned long long totUser = 0, unsigned long long totUserLow = 0,
        unsigned long long TotSys = 0, unsigned long long TotIdle = 0) {
      totalUser = totUser;
      totalUserLow = totUserLow;
      totalSys = TotSys;
      totalIdle = TotIdle;
    }
  };

  /**
   * CPU usage parameters (see cpu) for the total system and the individual cores.
   * Therefore, the size of this vector is nCores + 1
   */
  std::vector<cpu> lastInfo;

  /**
   * Calculates the percentage of cpu usage.
   * This is done in total and for core found on the system.
   * If cpu usage is set to -1 there was an overflow in the \c /proc/stat file.
   */
  void calculatePCPU();

  /**
   * Read values from the \c /proc/stat for all cpu cores (cpux) and overall values (cpu).
   */
  void readCPUInfo(std::vector<cpu> &vcpu);

public:
  SystemInfoModule(EntityOwner *owner, const std::string &name,
      const std::string &description, bool eliminateHierarchy = false,
      const std::unordered_set<std::string> &tags = { });

  ctk::ScalarPushInput<uint> trigger { this, "trigger", "",
      "Trigger used to update the watchdog" };
  /**
   * \name Static system information (read only once)
   * @{
   */
  std::map<std::string, ctk::ScalarOutput<std::string> > strInfos;
  ctk::ScalarOutput<int> ticksPerSecond { this, "ticksPerSecond", "Hz",
      "Number of clock ticks per second" }; ///< Number of clock ticks per second
  ctk::ScalarOutput<int> nCPU { this, "nCPU", "", "Number of CPUs",
    { "CS", "SYS" } };
  /** @} */
  /**
   * \name Non static system information
   * @{
   */
  //\todo: Implement the following as unsigned long!
  ctk::ScalarOutput<int> maxMem { this, "maxMem", "kB",
      "Maximum available memory", { "CS", "SYS" } };
  ctk::ScalarOutput<int> freeMem { this, "freeMem", "kB", "Free memory",
    { "CS", "SYS", "DAQ" } };
  ctk::ScalarOutput<int> cachedMem { this, "cachedMem", "kB", "Cached memory",
    { "CS", "SYS" } };
  ctk::ScalarOutput<int> usedMem { this, "usedMem", "kB", "Used memory",
    { "CS", "SYS", "DAQ"} };
  ctk::ScalarOutput<int> maxSwap { this, "maxSwap", "kB", "Swap size",
    { "CS", "SYS" } };
  ctk::ScalarOutput<int> freeSwap { this, "freeSwap", "kB", "Free swap",
    { "CS", "SYS", "DAQ" } };
  ctk::ScalarOutput<int> usedSwap { this, "usedSwap", "kB", "Used swap",
    { "CS", "SYS", "DAQ" } };
  //\todo: Implement the following as long!
  ctk::ScalarOutput<int> startTime { this, "startTime", "s", "start time of system with respect to EPOCH",
      { "CS", "SYS" } };
  ctk::ScalarOutput<std::string> startTimeStr { this, "startTimeStr", "", "startTimeStr",
        { "CS", "SYS" } };
  ctk::ScalarOutput<int> uptime_secTotal { this, "uptimeSecTotal", "s", "Total uptime",
      { "CS", "SYS" } };
  ctk::ScalarOutput<int> uptime_day { this, "uptimeDays", "day", "Days up",
    { "CS", "SYS" } };
  ctk::ScalarOutput<int> uptime_hour { this, "uptimeHours", "h", "Hours up",
    { "CS", "SYS" } };
  ctk::ScalarOutput<int> uptime_min { this, "uptimeMin", "min", "Minutes up",
    { "CS", "SYS" } };
  ctk::ScalarOutput<int> uptime_sec { this, "uptimeSec", "s", "Seconds up",
    { "CS", "SYS" } };
  std::unique_ptr<ctk::ArrayOutput<double> > cpu_use;
  ctk::ScalarOutput<double> cpu_useTotal {this, "cpuTotal", "%", "Total CPU usage",
    { "CS", "SYS", "DAQ" } };
  ctk::ArrayOutput<double> loadAvg{ this, "loadAvg", "", 3, "Average load within last min, 5min, 15min",
    { "CS", "SYS", "DAQ" } };
  /** @} */

  /**
   * \name Logging
   * @{
   */
  std::ostream *logging;
#ifdef ENABLE_LOGGING
  /** Message to be send to the logging module */
  ctk::ScalarOutput<std::string> message { this, "message", "", "Message of the module to the logging System",
      { "Logging", "PROCESS", getName() } };

  /** Message to be send to the logging module */
  ctk::ScalarOutput<uint> messageLevel { this, "messageLevel", "", "Logging level of the message",
      { "Logging", "PROCESS", getName() } };

  void sendMessage(const logging::LogLevel &level = logging::LogLevel::INFO);

#endif
  /** @} */

  /**
   * Main loop function.
   * Reads number of cores and system clock ticks and other static parameter only once before the loop.
   */
  void mainLoop();

  void terminate();

  std::string getTime();

};

#endif /* INCLUDE_SYSTEMINFOMODULE_H_ */
