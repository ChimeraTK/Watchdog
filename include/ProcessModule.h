/**
 * ProcessModule.h
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#ifndef INCLUDE_PROCESSMODULE_H_
#define INCLUDE_PROCESSMODULE_H_

#undef GENERATE_XML
#include <ChimeraTK/ApplicationCore/ApplicationCore.h>

#include <memory>

namespace ctk = ChimeraTK;

#ifdef BOOST_1_64
#include <boost_process/process/child.hpp>
namespace bp = boost_process;
#else
#include "sys_stat.h"
#endif

/**
 * \brief
 * This module is used to start and stop subprocess controlled by the watchdog.
 * It also collects information about the running job.
 * \todo Implement proper data types instead of using int for all of them!
 */
struct ProcessInfoModule : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

#ifdef BOOST_1_64
  std::shared_ptr<boost_process::process::child> process;
#else
  ProcessHandler process; ///< The process handler used to get information about the subprocess
#endif
  ctk::ScalarPushInput<int> trigger { this, "trigger", "",
      "Trigger used to update the watchdog" };
  /**
   * \name Process parameter and status
   * @{
   */
  /** PID of the process */
  ctk::ScalarOutput<int> processPID { this, "PID", "", "PID of the process",
    { "CS", "PROCESS", getName() } };
  /** Time since process is running */
  ctk::ScalarOutput<int> runtime { this, "runtime", "s", "Time since process is running",
    { "CS", "PROCESS", getName() } };
  /** @} */

  /**
   * Store an internal time stamp that is used to calculate the cpu usage.
   */
  boost::posix_time::ptime time_stamp;

  /**
   * \name Information from the SystemInfoModule
   * @{
   */
  /** Number of clock ticks per second */
  ctk::ScalarPollInput<int> ticksPerSecond { this, "tickPerSecond", "Hz",
      "Number of clock ticks per second" };
  /** Uptime of the system */
  ctk::ScalarPollInput<int> sysUpTime { this, "sysUpTime", "s", "Uptime of the system" };
  ctk::ScalarPollInput<int> sysStartTime { this, "sysStartTime", "s", "System start time of the system since EPOCH" };
  /** @} */

  /**
   * \name Parameters read by proc
   * @{
   */

  //\todo Use unsigned long long int
  /** user-mode CPU time accumulated by process */
  ctk::ScalarOutput<int> utime { this, "utime", "clock ticks", "user-mode CPU time accumulated by process",
    { "CS", "PROCESS", getName() } };
  /** kernel-mode CPU time accumulated by process */
  ctk::ScalarOutput<int> stime { this, "stime", "clock ticks", "kernel-mode CPU time accumulated by process",
    { "CS", "PROCESS", getName() } };
  /** cumulative utime of process and reaped children */
  ctk::ScalarOutput<int> cutime { this, "cutime", "clock ticks",
      "cumulative utime of process and reaped children",
    { "CS", "PROCESS", getName() } };
  /** cumulative stime of process and reaped children */
  ctk::ScalarOutput<int> cstime { this, "cstime", "clock ticks", "cumulative stime of process and reaped children",
    { "CS", "PROCESS", getName() } };
  /** start time of process -- seconds since 1-1-70 */
  ctk::ScalarOutput<int> startTime { this, "startTime", "s", "start time of process with respect to system start [s]",
    { "CS", "PROCESS", getName() } };

  ctk::ScalarOutput<std::string> startTimeStr { this, "startTimeStr", "", "start time string",
      { "CS", "PROCESS", getName() } };

  //\todo Use long
  /** kernel scheduling priority */
  ctk::ScalarOutput<int> priority { this, "priority", "", "kernel scheduling priority",
    { "CS", "PROCESS", getName() } };
  /** standard unix nice level of process */
  ctk::ScalarOutput<int> nice { this, "nice", "", "standard unix nice level of process",
    { "CS", "PROCESS", getName() } };
  /** resident set size from /proc/#/stat (pages) */
  ctk::ScalarOutput<int> rss { this, "rss", "", "resident set size from /proc/#/stat (pages)",
    { "CS", "PROCESS", getName() } };

  /**
   * CPU usage for measured between two control system loops.
   * The process time includes utime, stime, sutime, sctime.
   */
  ctk::ScalarOutput<double> pcpu { this, "pcpu", "%", "Actual CPU usage",
    { "CS", "PROCESS", getName() } };
  /**
   * CPU usage averaged over the whole runtime of the process.
   * The process time includes utime, stime, sutime, sctime.
   */
  ctk::ScalarOutput<double> avpcpu { this, "avpcpu", "%", "Average CPU usage",
    { "CS", "PROCESS", getName() } };
  /** @} */

  /**
   * Application core main loop.
   */
  virtual void mainLoop();

  /**
   * Fill process information read via proc interface.
   * \remark When changing the pidOffset to get information of another child the
   * cpu usage value will be wrong for the first reading!
   */
  void FillProcInfo(const std::shared_ptr<proc_t> &info);
};

struct ProcessControlModule : public ProcessInfoModule{

  using ProcessInfoModule::ProcessInfoModule;

  /**
   * \name Process control parameter and status
   * @{
   */
  /** Path where to execute the command used to start the process */
  ctk::ScalarPollInput<std::string> processPath { this, "path", "",
      "Path where to execute the command used to start the process",
      { "PROCESS", getName() } };
  /** Start the process */
  ctk::ScalarPollInput<int> enableProcess { this, "startProcess", "", "Start the process",
    { "PROCESS", getName() } };
  /** Command used to start the process */
  ctk::ScalarPollInput<std::string> processCMD { this, "cmd", "", "Command used to start the process",
    { "PROCESS", getName() } };
  /** Signal used to kill the process (2: SIGINT, 9: SIGKILL) */
  ctk::ScalarPollInput<int> killSig { this, "killSig", "", "Signal used to kill the process (2: SIGINT, 9: SIGKILL)",
    { "PROCESS", getName() } };
  /** PID offset used when monitoring the started process */
  ctk::ScalarPollInput<int> pidOffset { this, "pidOffset", "", "PID offset used when monitoring the started process",
    { "PROCESS", getName() } };
  /** Process status 0: not running, 1: running */
  ctk::ScalarOutput<int> processIsRunning { this, "IsRunning", "", "Process status 0: not running, 1: running",
      { "CS", "PROCESS", getName() } };
  /** Number of failed restarts */
  ctk::ScalarOutput<int> processNFailed { this, "Failed", "", "Number of failed restarts",
    { "CS", "PROCESS", getName() } };   
  /** Number of started processes */
  ctk::ScalarOutput<int> processNChilds { this, "nChilds", "", "Number of started processes",
    { "CS", "PROCESS", getName() } };
  /** Number of time the process was automatically */
  ctk::ScalarOutput<int> processRestarts { this, "Restarts", "", "Number of time the process was automatically "
          "restarted by the watchdog since server start.",
    { "CS", "PROCESS", getName() } };
  /** @} */

  /**
   * Set the PID and set status to running.
   * \param pid PID of the process that was started.
   */
  void SetOnline(const int &pid);

  /**
   * Reset the PID to -1 and the status to not running.
   */
  void SetOffline();

  /**
   * Calls SetOffset and increases the failed counter.
   * In addition a sleep of 1s is added to have some delay between different attempts to start a process.
   */
  void Failed();

  /**
   * Application core main loop.
   */
  void mainLoop();
};

#endif /* INCLUDE_PROCESSMODULE_H_ */
