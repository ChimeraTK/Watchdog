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

#include "sys_stat.h"
#include "ProcessHandler.h"

#ifdef ENABLE_LOGGING
#include "LoggingModule.h"
#else
#include "Logging.h"
#endif

/**
 * \brief
 * This module is used to collect process information.
 *
 * It also collects information about the running job.
 * \todo Implement proper data types instead of using int for all of them!
 */
struct ProcessInfoModule : public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;

  ctk::ScalarPushInput<uint64_t> trigger { this, "trigger", "",
      "Trigger used to update the watchdog" };
  /**
   * \name Process parameter and status
   * @{
   */
  /** PID of the process */
  ctk::ScalarOutput<int> processPID { this, "PID", "", "PID of the process",
    { "CS", "PROCESS", getName() , "DAQ"} };

  /** Time since process is running */
  ctk::ScalarOutput<uint> runtime { this, "runtime", "s", "Time since process is running",
    { "CS", "PROCESS", getName(), "DAQ" } };
#ifdef ENABLE_LOGGING
  /** Message to be send to the logging module */
  ctk::ScalarOutput<std::string> message { this, "message", "", "Message of the module to the logging System",
      { "Logging", "PROCESS", getName() } };

  /** Message to be send to the logging module */
  ctk::ScalarOutput<uint> messageLevel { this, "messageLevel", "", "Logging level of the message",
      { "Logging", "PROCESS", getName() } };
#endif
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
  ctk::ScalarPollInput<uint> ticksPerSecond { this, "tickPerSecond", "Hz",
      "Number of clock ticks per second" };
  /** Uptime of the system */
  ctk::ScalarPollInput<uint> sysUpTime { this, "sysUpTime", "s", "Uptime of the system" };
  ctk::ScalarPollInput<uint> sysStartTime { this, "sysStartTime", "s", "System start time (seconds since EPOCH)" };
  ctk::ScalarPollInput<uint> maxMem { this, "maxMem", "kB", "Maximum available memory" };
  /** @} */

  /**
   * \name Parameters read by proc
   * @{
   */
  //\todo Use unsigned long long int
  /** user-mode CPU time accumulated by process */
  ctk::ScalarOutput<uint> utime { this, "utime", "clock ticks", "user-mode CPU time accumulated by process",
    { "CS", "PROCESS", getName() } };
  /** kernel-mode CPU time accumulated by process */
  ctk::ScalarOutput<uint> stime { this, "stime", "clock ticks", "kernel-mode CPU time accumulated by process",
    { "CS", "PROCESS", getName() } };
  /** cumulative utime of process and reaped children */
  ctk::ScalarOutput<uint> cutime { this, "cutime", "clock ticks",
      "cumulative utime of process and reaped children",
    { "CS", "PROCESS", getName() } };
  /** cumulative stime of process and reaped children */
  ctk::ScalarOutput<uint> cstime { this, "cstime", "clock ticks", "cumulative stime of process and reaped children",
    { "CS", "PROCESS", getName() } };
  /** start time of process -- seconds since 1-1-70 */
  ctk::ScalarOutput<uint> startTime { this, "startTime", "s", "start time of process with respect to EPOCH",
    { "CS", "PROCESS", getName() } };

  ctk::ScalarOutput<std::string> startTimeStr { this, "startTimeStr", "", "start time string",
      { "CS", "PROCESS", getName() } };

  //\ToDo Use unsigned long
  ctk::ScalarOutput<uint> mem { this, "mem", "kB", "Memory used by the process",
    { "CS", "PROCESS", getName(), "DAQ", "History" } };

  ctk::ScalarOutput<double> memoryUsage { this, "memoryUsage", "%", "Relative memory usage",
      { "CS", "SYS", "DAQ", "History" } };

  //\todo Use long
  /** kernel scheduling priority */
  ctk::ScalarOutput<uint> priority { this, "priority", "", "kernel scheduling priority",
    { "CS", "PROCESS", getName() } };
  /** standard unix nice level of process */
  ctk::ScalarOutput<uint> nice { this, "nice", "", "standard unix nice level of process",
    { "CS", "PROCESS", getName() } };
  /** resident set size from /proc/#/stat (pages) */
  ctk::ScalarOutput<uint> rss { this, "rss", "", "resident set size from /proc/#/stat (pages)",
    { "CS", "PROCESS", getName() } };

  /**
   * CPU usage for measured between two control system loops.
   * The process time includes utime, stime, sutime, sctime.
   */
  ctk::ScalarOutput<double> pcpu { this, "pcpu", "%", "Actual CPU usage",
    { "CS", "PROCESS", getName(), "DAQ", "History" } };
  /**
   * CPU usage averaged over the whole runtime of the process.
   * The process time includes utime, stime, sutime, sctime.
   */
  ctk::ScalarOutput<double> avpcpu { this, "avpcpu", "%", "Average CPU usage",
    { "CS", "PROCESS", getName(), "DAQ", "History" } };
  /** @} */

  /**
   * Application core main loop.
   */
  void mainLoop() override;

  /**
   * Fill process information read via proc interface.
   * \remark When changing the pidOffset to get information of another child the
   * cpu usage value will be wrong for the first reading!
   */
  void FillProcInfo(const std::shared_ptr<proc_t> &info);

  std::ostream *logging;
#ifdef ENABLE_LOGGING

  void sendMessage(const logging::LogLevel &level = logging::LogLevel::INFO);

  /**
   * Search for key words in the given stream (LogLevels like DEBUG, INFO...).
   * Splits the stream using Logging::stripMessages().
   * Then sends individual messages to the LoggingModule.
   */
  void evaluateMessage(std::stringstream &msg);
#endif
  /* Don't overload the stream operator of ProcessInfoModule -> will cause Segfaults. Use getTime() instead. */
//  friend std::stringstream& operator<<(std::stringstream &ss, const ProcessInfoModule* module);

  /**
   * Print the watchdog server name, a time stamp and the module name:
   * Result is: 'WATCHDOG_SERVER: "day of week"  "month" "day" hh:mm:ss yyyy "module_name" -> '
   */
  std::string getTime();

  void terminate() override;
};


/**
 * \brief This module is used to start and stop subprocess controlled by the watchdog.
 */
struct ProcessControlModule : public ProcessInfoModule{
  std::unique_ptr<ProcessHandler> process; ///< The process handler used to get information about the subprocess

  /**
   * Default ApplicationModule constructor with an additional argument:
   * \param historyOn If true process parameters are filled with 0 periodically in case the process is off.
   * This is needed in order to have constant filling of the history buffer written by the History module of the watchdog.
   */
  ProcessControlModule(EntityOwner *owner, const std::string &name,
      const std::string &description, bool historyOn = false, bool eliminateHierarchy = false,
      const std::unordered_set<std::string> &tags = { }): ProcessInfoModule(owner, name, description, eliminateHierarchy, tags),
          _stop(false), _restartRequired(false), _historyOn(historyOn){};

  /* Use terminate function to delete the ProcessHandler, since it is using a local stringstream constructed in the main loop
   * which is not existing at the time the ProcessHandler destructor is called!
   */
  void terminate() override;

  /**
   * \name Process control parameter (static during process execution)
   * @{
   */
  /** Path where to execute the command used to start the process */
  ctk::ScalarOutput<std::string> processPath { this, "Path", "",
      "Path where to execute the command used to start the process",
    {  "CS", "PROCESS", getName() } };
  /** Command used to start the process */
  ctk::ScalarOutput<std::string> processCMD { this, "CMD", "", "Command used to start the process",
    {  "CS", "PROCESS", getName() } };
  /** Environment variable set for the process */
  ctk::ScalarOutput<std::string> processENV { this, "ENV", "", "Environment variables of the process",
    {  "CS", "PROCESS", getName() } };
#ifdef ENABLE_LOGGING
  /** Log file name. It will be created in the given processPath */
  ctk::ScalarOutput<std::string> processExternalLogfile { this, "ExternalLogfile", "",
    "Name of the logfile created in the given path (the process controlled by the module will "
    "put its output here. Module messages go to cout/cerr",
    {  "CS", "PROCESS", getName() } };
#endif
  /** @} */
  /**
     * \name Process control parameter (dynamic process execution)
     * @{
     */
  /** Path where to execute the command used to start the process */
  ctk::ScalarPollInput<std::string> processSetPath { this, "SetPath", "",
      "Set the path where to execute the command used to start the process",
    { "CS", "PROCESS", getName() } };
  /** Command used to start the process */
  ctk::ScalarPollInput<std::string> processSetCMD { this, "SetCMD", "", "Set the command used to start the process",
    { "CS", "PROCESS", getName() } };

  /** Command used to start the process */
  ctk::ScalarPollInput<std::string> processSetENV { this, "SetEnvironment", "", "Set environment variables needed by the process",
    { "CS", "PROCESS", getName() } };
  ctk::ScalarPollInput<int> processOverwriteENV { this, "OverwriteEnvironment", "",
    "If true the environmet variables are overwritten. Else they are extended.",
    { "CS", "PROCESS", getName() } };

  ctk::ScalarPollInput<uint> processMaxFails { this, "SetMaxFails", "",
    "Set the maximum number of allowed fails.",
    { "CS", "PROCESS", getName() } };
  ctk::ScalarPollInput<uint> processMaxRestarts { this, "SetMaxRestarts", "",
    "Set the maximum number of allowed restarts.",
    { "CS", "PROCESS", getName() } };
#ifdef ENABLE_LOGGING
  /** Log file name. It will be created in the given processPath */
  ctk::ScalarPollInput<std::string> processSetExternalLogfile { this, "SetLogfileExternal", "", "Set the name of the logfile"
      " used by the process to be started. It is created in the given path.",
    { "CS", "PROCESS", getName() } };
#endif
  /** Start the process */
  ctk::ScalarPollInput<uint> enableProcess { this, "enableProcess", "", "Start the process",
    { "CS", "PROCESS", getName() } };
  /** Signal used to kill the process (2: SIGINT, 9: SIGKILL) */
  ctk::ScalarPollInput<uint> killSig { this, "killSig", "", "Signal used to kill the process (2: SIGINT, 9: SIGKILL)",
    { "CS", "PROCESS", getName() } };
  /** PID offset used when monitoring the started process */
  ctk::ScalarPollInput<uint> pidOffset { this, "pidOffset", "", "PID offset used when monitoring the started process",
    { "CS", "PROCESS", getName() } };
  /** Process status 0: not running, 1: running */
  ctk::ScalarOutput<uint> processIsRunning { this, "IsRunning", "", "Process status 0: not running, 1: running",
      { "CS", "PROCESS", getName(), "DAQ" } };
  /** Number of failed restarts */
  ctk::ScalarOutput<uint> processNFailed { this, "Failed", "", "Number of failed starts/restarts",
    { "CS", "PROCESS", getName(), "DAQ" } };
  /** Number of started processes */
  ctk::ScalarOutput<uint> processNChilds { this, "nChilds", "", "Number of started processes",
    { "CS", "PROCESS", getName(), "DAQ" } };
  /** Number of time the process was automatically */
  ctk::ScalarOutput<uint> processRestarts { this, "Restarts", "", "Number of time the process was automatically "
          "restarted by the watchdog since server start.",
    { "CS", "PROCESS", getName(), "DAQ" } };
  ctk::ScalarPollInput<uint> bootDelay { this, "BootDelay", "s", "This delay is used at server start. Use this to delay the process start with"
      "respect to other processes.",
    { "CS", "PROCESS", getName(), "DAQ"} };
  ctk::ScalarPollInput<uint> killTimeout { this, "killTimeout", "s", "This is the maximum time waited for the process to exit after stopping. After, it is"
        " stopped using SIGKILL.",
    { "CS", "PROCESS", getName(), "DAQ"} };
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
   * Check if the process with PID is running.
   * If not update status variables (processPID, processIsRunning, processRestarts)
   * \param pid PID of the process that was started.
   * \ToDo: If using a const reference here, starting a process always results in a positive result here.
   * Even if starting the process failed. See test_processModule.cc/testFailed, which can not even reproduce this
   * behavior, but shows what to test witch a real server.
   */
  void CheckIsOnline(const int pid);

  /**
   * Set kill signal according to user setting set in killSig. After reset the ProcessHandler.
   */
  void resetProcessHandler(std::stringstream* handlerMessage);

  /**
   * Application core main loop.
   */
  void mainLoop() override;
private:
  /**
   * Stop is used to enter the idle state.
   * To escape the idle state enableProcess needs to be set false.
   */
  bool _stop;

  /**
   * This flag is used to know if a process is started because a client enabled the process or
   * if it is started because the process terminated without client interaction.
   */
  bool _restartRequired;

  /**
   * This flags is  used decide if empty data is written in case the process is not running.
   * This is needed to end up with a meaningful history buffer in case server based history is enabled.
   */
  bool _historyOn;
};

#endif /* INCLUDE_PROCESSMODULE_H_ */
