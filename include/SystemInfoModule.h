// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

/*
 * SystemInfo.h
 *
 *  Created on: Sep 6, 2017
 *      Author: Klaus Zenker (HZDR)
 */

#include "sys_stat.h"

#include <ChimeraTK/ApplicationCore/ApplicationCore.h>
#include <ChimeraTK/ApplicationCore/Logging.h>

#include <unordered_set>

namespace ctk = ChimeraTK;

/**
 * \brief This module is used to read system parameter.
 *
 * Some of them are static and only read once (e.g. processor model).
 * Others are watched continuously (e.g. uptime, work load...).
 * The "SYS" tag is used for all variables that are updated in the main loop.
 * \todo Implement proper data types instead of using int for all of them!
 */
class SystemInfoModule : public ctk::ApplicationModule {
 private:
  SysInfo sysInfo;

  /**
   * CPU usage parameters needed to calculate the cpu usage.
   * These vales are read from \c /proc/stat
   */
  struct cpu {
    unsigned long long totalUser{0};
    unsigned long long totalUserLow{0};
    unsigned long long totalSys{0};
    unsigned long long totalIdle{0};
    cpu(unsigned long long totUser = 0, unsigned long long totUserLow = 0, unsigned long long TotSys = 0,
        unsigned long long TotIdle = 0)
    : totalUser(totUser), totalUserLow(totUserLow), totalSys(TotSys), totalIdle(TotIdle) {}
  };
#ifdef WITH_PROCPS
  /**
   * CPU usage parameters (see cpu) for the total system and the individual cores.
   * Therefore, the size of this vector is nCores + 1
   */
  std::vector<cpu> lastInfo;

  /**
   * Read values from the \c /proc/stat for all cpu cores (cpux) and overall values (cpu).
   */
  void readCPUInfo(std::vector<cpu>& vcpu);
#else
  struct stat_info* infoptr{nullptr};
#endif

  /**
   * Calculates the percentage of cpu usage.
   * This is done in total and for core found on the system.
   * If cpu usage is set to -1 there was an overflow in the \c /proc/stat file.
   */
  void calculatePCPU();

 public:
  SystemInfoModule(ctk::ModuleGroup* owner, const std::string& name, const std::string& description,
      const std::unordered_set<std::string>& tags = {}, const std::string& pathToTrigger = "/Trigger/tick");

  ctk::ScalarPushInput<uint64_t> trigger;

  /**
   * \name Static system information (read only once)
   * @{
   */
  struct Info : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    std::map<std::string, ctk::ScalarOutput<std::string>> strInfos;
    ctk::ScalarOutput<uint> ticksPerSecond{this, "ticksPerSecond", "Hz", "Number of clock ticks per second",
        {"ProcessModuleInput"}}; ///< Number of clock ticks per second
    ctk::ScalarOutput<uint> nCPU{this, "nCPU", "", "Number of CPUs"};
  } info{this, "info", "Static system information"};
  /** @} */
  /**
   * \name Non static system information
   * @{
   */
  struct Status : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;

    //\todo: Implement the following as unsigned long!
    ctk::ScalarOutput<uint64_t> maxMem{this, "maxMem", "kB", "Maximum available memory", {"ProcessModuleInput"}};
    ctk::ScalarOutput<uint64_t> freeMem{this, "freeMem", "kB", "Free memory", {"DAQ", "history"}};
    ctk::ScalarOutput<uint64_t> cachedMem{this, "cachedMem", "kB", "Cached memory"};
    ctk::ScalarOutput<uint64_t> usedMem{this, "usedMem", "kB", "Used memory", {"DAQ", "history"}};
    ctk::ScalarOutput<uint64_t> maxSwap{this, "maxSwap", "kB", "Swap size"};
    ctk::ScalarOutput<uint64_t> freeSwap{this, "freeSwap", "kB", "Free swap", {"DAQ"}};
    ctk::ScalarOutput<uint64_t> usedSwap{this, "usedSwap", "kB", "Used swap", {"DAQ", "history"}};
    ctk::ScalarOutput<double> memoryUsage{this, "memoryUsage", "%", "Relative memory usage", {"DAQ", "history"}};
    ctk::ScalarOutput<double> swapUsage{this, "swapUsage", "%", "Relative swap usage", {"DAQ", "history"}};
    //\todo: Implement the following as long!
    ctk::ScalarOutput<uint64_t> startTime{
        this, "startTime", "s", "start time of system with respect to EPOCH", {"ProcessModuleInput"}};
    ctk::ScalarOutput<std::string> startTimeStr{this, "startTimeStr", "", "startTimeStr"};
    ctk::ScalarOutput<uint64_t> uptime_secTotal{
        this, "uptimeSecTotal", "s", "Total uptime", {"DAQ", "ProcessModuleInput"}};
    ctk::ScalarOutput<uint> uptime_day{this, "uptimeDays", "day", "Days up"};
    ctk::ScalarOutput<uint> uptime_hour{this, "uptimeHours", "h", "Hours up"};
    ctk::ScalarOutput<uint> uptime_min{this, "uptimeMin", "min", "Minutes up"};
    ctk::ScalarOutput<uint> uptime_sec{this, "uptimeSec", "s", "Seconds up"};
    std::unique_ptr<ctk::ArrayOutput<double>> cpu_use;
    ctk::ScalarOutput<double> cpu_useTotal{this, "cpuTotal", "%", "Total CPU usage", {"DAQ", "history"}};
    ctk::ArrayOutput<double> loadAvg{
        this, "loadAvg", "", 3, "Average load within last min, 5min, 15min", {"DAQ", "history"}};
  } status{this, "status", "status of the system"};
  /** @} */

  /**
   * \name Logging
   * @{
   */
  boost::shared_ptr<logging::Logger> logger{new logging::Logger(this, "logging")};
  /** @} */

  /**
   * Main loop function.
   * Reads number of cores and system clock ticks and other static parameter only once before the loop.
   */
  void mainLoop() override;
};

/*
 * Return the current time in a formatted way:
 * WATCHDOG_SERVER: 2018-Oct-25 11:22:29.411611  -> "module name" ->
 * This is used to be prepended to logging messages.
 */
std::string getTime(ctk::ApplicationModule* mod);

extern std::mutex fs_mutex; ///< This mutex is used when reading filesystem information via statfs

/**
 * \brief Module reading file system information (size, available space)
 *
 * Hard disk devices are mounted at a certain location. The statfs that is used
 * internally to read information needs the mount point and not the device itself.
 * E.g. /dev/sdb1 mounted at /media/data -> statfs works on /media/data.
 * Therefore the module might be used multiple times for different mount point to
 * be monitored.
 * The "SYS" tag is used for all variables that are updated in the main loop.
 */
struct FileSystemModule : public ctk::ApplicationModule {
  /**
   * Constructor.
   * \param devName The name of the device (e.g. /dev/sdb1 -> sdb1)
   * \param mntPoint The mount point of the device (e.g. / for the root directory)
   */
  FileSystemModule(const std::string& devName, const std::string& mntPoint, ctk::ModuleGroup* owner,
      const std::string& name, const std::string& description, const std::unordered_set<std::string>& tags = {},
      const std::string& pathToTrigger = "/Trigger/tick");

  ctk::ScalarOutput<std::string> deviceName{this, "deviceName", "", "Name of the device"};

  struct Config : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;

    ctk::ScalarPollInput<double> warningLevel{this, "warningLevel", "%",
        "Set the level when disc usage state is set to warning. If set to 0 90% is used instead.",
        {"PROCESS", getName()}};
    ctk::ScalarPollInput<double> errorLevel{this, "errorLevel", "%",
        "Set the level when disc usage state is set to error. If set to 0 95% is used instead.",
        {"PROCESS", getName()}};
  } config{this, "config", "File system module configuration"};

  struct Status : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;

    ctk::ScalarOutput<std::string> mountPoint{this, "mountPoint", "", "Mount point of the device"};
    ctk::ScalarOutput<double> disk_size{this, "size", "GiB", "Mount point of the device", {"DAQ"}};
    ctk::ScalarOutput<double> disk_free{this, "free", "GiB", "Free disk space", {"DAQ"}};
    ctk::ScalarOutput<double> disk_user{this, "freeUser", "GiB", "Free disk space available for the user", {"DAQ"}};
    ctk::ScalarOutput<double> disk_usage{
        this, "usage", "%", "Disk usage with respect to the space available to the user", {"DAQ", "history"}};
    ctk::ScalarOutput<uint> disk_status{this, "usageStatus", "",
        "Status of the disk usage-> 0:ok, 1:warning, 2:error. "
        "Levels can be set in the config section.",
        {"DAQ", "history"}};
  } status{this, "status", "Information about the mounted device"};

  ctk::ScalarPushInput<uint64_t> trigger;

  const double GiB = 1. / 1024 / 1024 / 1024; ///< Conversion to GiB (not to be mixed up with GB!)

  std::string tmp[2]; ///< Temporary store device name and mount point
  /**
   * \name Logging
   * @{
   */
  boost::shared_ptr<logging::Logger> logger{new logging::Logger(this, "logging")};
  /** @} */

  /**
   * Main loop function.
   */
  void mainLoop() override;

  /**
   * Use statfs to read information about the device.
   * Since this might not be thread safe a mutex is used here.
   */
  bool read();
};

/**
 * \brief This group includes one FileSystemModule per hard drive.
 */
struct FileSystemGroup : public ctk::ModuleGroup {
  using ctk::ModuleGroup::ModuleGroup;

  /**
   * Modules monitoring disks usage of system drives.
   */
  std::vector<FileSystemModule> fsMonitors;
};

/**
 * \brief Module reading information of a network adapter.
 *
 * Parameters that are observed are:
 * - received data (byte, packates)
 * - transmitted data (byte, packates)
 * - dropped data (transmitted and received)
 * - collisions
 *
 *
 * Hard disk devices are mounted at a certain location. The statfs that is used
 * internally to read information needs the mount point and not the device itself.
 * E.g. /dev/sdb1 mounted at /media/data -> statfs works on /media/data.
 * Therefore the module might be used multiple times for different mount point to
 * be monitored.
 * The "SYS" tag is used for all variables that are updated in the main loop.
 */
struct NetworkModule : public ctk::ApplicationModule {
  NetworkModule(const std::string& device, ctk::ModuleGroup* owner, const std::string& name,
      const std::string& description, const std::unordered_set<std::string>& tags = {},
      const std::string& pathToTrigger = "/Trigger/tick");

  std::string networkDeviceName;

  /**
   * Publish the device name although it is also encoded in the path by
   * the watchdog  sever.
   */
  ctk::ScalarOutput<std::string> deviceName{this, "deviceName", "", "Name of the device"};
  struct Status : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    /*
     * Use a vector to handle all output variables of the module for easy
     * filling.
     */
    std::vector<ctk::ScalarOutput<double>> data;
  } status{this, "status", "Status of the network device"};

  ctk::ScalarPushInput<uint64_t> trigger;

  const double MiB = 1. / 1024 / 1024; ///< Conversion to MiB (not to be mixed up with MB!)

  struct raw {
    std::vector<unsigned long long> data;       ///< Used to store raw data read from sys files
    std::vector<std::string> files;             ///< Names of the sys files used to get networking information
    std::vector<boost::posix_time::ptime> time; ///< Safe individual time stamps of reading a certain sys file
    raw(const std::string& device) {
      data = std::vector<unsigned long long>(7);
      std::string src_base("/sys/class/net/");
      files.push_back(src_base + device + "/statistics/rx_packets");
      files.push_back(src_base + device + "/statistics/tx_packets");
      files.push_back(src_base + device + "/statistics/rx_bytes");
      files.push_back(src_base + device + "/statistics/tx_bytes");
      files.push_back(src_base + device + "/statistics/rx_dropped");
      files.push_back(src_base + device + "/statistics/tx_dropped");
      files.push_back(src_base + device + "/statistics/collisions");
      time = std::vector<boost::posix_time::ptime>(7);
    }
    raw() = default;
  };

  raw previousData; ///< Last data collected

  /**
   * \name Logging
   * @{
   */
  boost::shared_ptr<logging::Logger> logger{new logging::Logger(this, "logging")};
  /** @} */

  /**
   * Main loop function.
   */
  void mainLoop() override;

  /**
   * Read network statistics.
   */
  void read();
};

/**
 * \brief This group includes one NetworkModule per network device.
 */
struct NetworkGroup : public ctk::ModuleGroup {
  using ctk::ModuleGroup::ModuleGroup;

  /**
   * Modules monitoring disks usage of system drives.
   */
  std::vector<NetworkModule> networkMonitors;
};
