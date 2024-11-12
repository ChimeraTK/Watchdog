// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

/**
 * ProcessModule.h
 *
 *  Created on: Sep 6, 2017
 *      Author: Klaus Zenker (HZDR)
 */

#include <ChimeraTK/ApplicationCore/ApplicationCore.h>
#include <ChimeraTK/ApplicationCore/Logging.h>

#include <memory>

namespace ctk = ChimeraTK;

#include "LogFileReader.h"
#include "ProcessHandler.h"
#include "sys_stat.h"

/**
 * \brief
 * This module is used to collect process information.
 *
 * It also collects information about the running job.
 * \todo Implement proper data types instead of using int for all of them!
 */
struct ProcessInfoModule : public ctk::ApplicationModule {
  ProcessInfoModule(ctk::ModuleGroup* owner, const std::string& name, const std::string& description,
      const std::unordered_set<std::string>& tags = {}, const std::string& pathToTrigger = "/Trigger/tick")
  : ctk::ApplicationModule(owner, name, description, tags), trigger(this, pathToTrigger, "", "Trigger input"){};

  ctk::ScalarPushInput<uint64_t> trigger;

  struct Status : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    /** PID of the process */
    ctk::ScalarOutput<int> processPID{this, "PID", "", "PID of the process", {"PROCESS", getName(), "DAQ"}};
  } info{this, "status", "Status parameter of the process"};

  boost::shared_ptr<logging::Logger> logger{new logging::Logger(this, "logging")};

  boost::posix_time::ptime time_stamp;
  /**
   * \name Information from the SystemInfoModule
   * @{
   */
  struct System : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    struct Status : public ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;
      /** Uptime of the system */
      ctk::ScalarPollInput<uint> sysStartTime{this, "startTime", "s", "System start time (seconds since EPOCH)"};
      ctk::ScalarPollInput<uint> sysUpTime{this, "uptimeSecTotal", "s", "Uptime of the system"};
      ctk::ScalarPollInput<uint> maxMem{this, "maxMem", "kB", "Maximum available memory"};
    } status{this, "status", ""};

    struct Info : public ctk::VariableGroup {
      using ctk::VariableGroup::VariableGroup;
      /** Number of clock ticks per second */
      ctk::ScalarPollInput<uint> ticksPerSecond{this, "ticksPerSecond", "Hz", "Number of clock ticks per second"};
    } info{this, "info", ""};

  } system{this, "/system", ""};
  /** @} */

  struct Statistics : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    /** Time since process is running */
    ctk::ScalarOutput<uint> runtime{
        this, "runtime", "s", "Time since process is running", {"PROCESS", getName(), "DAQ"}};

    /**
     * \name Parameters read by proc
     * @{
     */
    //\todo Use unsigned long long int
    /** user-mode CPU time accumulated by process */
    ctk::ScalarOutput<uint> utime{
        this, "utime", "clock ticks", "user-mode CPU time accumulated by process", {"PROCESS", getName()}};
    /** kernel-mode CPU time accumulated by process */
    ctk::ScalarOutput<uint> stime{
        this, "stime", "clock ticks", "kernel-mode CPU time accumulated by process", {"PROCESS", getName()}};
    /** cumulative utime of process and reaped children */
    ctk::ScalarOutput<uint> cutime{
        this, "cutime", "clock ticks", "cumulative utime of process and reaped children", {"PROCESS", getName()}};
    /** cumulative stime of process and reaped children */
    ctk::ScalarOutput<uint> cstime{
        this, "cstime", "clock ticks", "cumulative stime of process and reaped children", {"PROCESS", getName()}};
    /** start time of process -- seconds since 1-1-70 */
    ctk::ScalarOutput<uint> startTime{
        this, "startTime", "s", "start time of process with respect to EPOCH", {"PROCESS", getName()}};

    ctk::ScalarOutput<std::string> startTimeStr{this, "startTimeStr", "", "start time string", {"PROCESS", getName()}};

    //\ToDo Use unsigned long
    ctk::ScalarOutput<uint> mem{
        this, "mem", "kB", "Memory used by the process", {"PROCESS", getName(), "DAQ", "history"}};

    ctk::ScalarOutput<double> memoryUsage{this, "memoryUsage", "%", "Relative memory usage", {"DAQ", "history"}};

    //\todo Use long
    /** kernel scheduling priority */
    ctk::ScalarOutput<uint> priority{this, "priority", "", "kernel scheduling priority", {"PROCESS", getName()}};
    /** standard unix nice level of process */
    ctk::ScalarOutput<uint> nice{this, "nice", "", "standard unix nice level of process", {"PROCESS", getName()}};
    /** resident set size from /proc/#/stat (pages) */
    ctk::ScalarOutput<uint> rss{this, "rss", "", "resident set size from /proc/#/stat (pages)", {"PROCESS", getName()}};

    /**
     * CPU usage for measured between two control system loops.
     * The process time includes utime, stime, sutime, sctime.
     */
    ctk::ScalarOutput<double> pcpu{this, "pcpu", "%", "Actual CPU usage", {"PROCESS", getName(), "DAQ", "history"}};
    /**
     * CPU usage averaged over the whole runtime of the process.
     * The process time includes utime, stime, sutime, sctime.
     */
    ctk::ScalarOutput<double> avgcpu{
        this, "avgcpu", "%", "Average CPU usage", {"PROCESS", getName(), "DAQ", "history"}};
    /** @} */
  } statistics{this, "statistics", "Process statistics read from the operating system"};

  /**
   * Application core main loop.
   */
  void mainLoop() override;

};

/**
 * \brief This module is used to start and stop subprocess controlled by the watchdog.
 */
struct ProcessControlModule : public ProcessInfoModule {
  std::unique_ptr<ProcessHandler> process; ///< The process handler used to get information about the subprocess

  /**
   * Default ApplicationModule constructor with an additional argument:
   * \param historyOn If true process parameters are filled with 0 periodically in case the process is off.
   * This is needed in order to have constant filling of the history buffer written by the History module of the watchdog.
   */
  ProcessControlModule(ctk::ModuleGroup* owner, const std::string& name, const std::string& description,
      bool historyOn = false, const std::unordered_set<std::string>& tags = {},
      const std::string& pathToTrigger = "/Trigger/tick")
  : ProcessInfoModule(owner, name, description, tags, pathToTrigger), _historyOn(historyOn){};

  /**
   * Search for key words in the given stream (LogLevels like DEBUG, INFO...).
   * Splits the stream using Logging::stripMessages().
   * Then sends individual messages to the LoggingModule.
   */
  void evaluateMessage(std::stringstream& msg);

  /* Use terminate function to delete the ProcessHandler, since it is using a local stringstream constructed in the main
   * loop which is not existing at the time the ProcessHandler destructor is called!
   */
  void terminate() override;

  struct ProcessStatus : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    /** Path where to execute the command used to start the process */
    ctk::ScalarOutput<std::string> path{
        this, "path", "", "Path where to execute the command used to start the process", {"PROCESS", getName()}};
    /** Command used to start the process */
    ctk::ScalarOutput<std::string> cmd{
        this, "command", "", "Command used to start the process", {"PROCESS", getName()}};
    /** Environment variable set for the process */
    ctk::ScalarOutput<std::string> env{
        this, "environment", "", "Environment variables of the process", {"PROCESS", getName()}};
    /** Process status */
    ctk::ScalarOutput<ctk::Boolean> isRunning{
        this, "isRunning", "", "Process status 0: not running, 1: running", {"PROCESS", getName(), "DAQ"}};
    /** Number of failed restarts */
    ctk::ScalarOutput<uint> nFailed{
        this, "nFailed", "", "Number of failed starts/restarts", {"PROCESS", getName(), "DAQ"}};
    /** Number of started processes */
    ctk::ScalarOutput<uint> nChilds{this, "nChilds", "", "Number of started processes", {"PROCESS", getName(), "DAQ"}};
    /** Number of time the process was automatically */
    ctk::ScalarOutput<uint> nRestarts{this, "nRestarts", "",
        "Number of time the process was automatically "
        "restarted by the watchdog since server start.",
        {"PROCESS", getName(), "DAQ"}};
    /** Log file name. It will be created in the given processPath */
    ctk::ScalarOutput<std::string> externalLogfile{this, "externalLogfile", "",
        "Name of the logfile created in the given path (the process controlled by the module will "
        "put its output here. Module messages go to cout/cerr",
        {"PROCESS", getName()}};
  } status{this, "status", "Status parameter of the process"};

  struct Config : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    /** Path where to execute the command used to start the process */
    ctk::ScalarPollInput<std::string> path{this, "path", "",
        "Set the path where to execute the command used to start the process", {"PROCESS", getName()}};
    /** Command used to start the process */
    ctk::ScalarPollInput<std::string> cmd{
        this, "command", "", "Set the command used to start the process", {"PROCESS", getName()}};
    ctk::ScalarPollInput<std::string> env{
        this, "environment", "", "Set environment variables needed by the process", {"PROCESS", getName()}};
    ctk::ScalarPollInput<int> overwriteEnv{this, "overwriteEnvironment", "",
        "If true the environmet variables are overwritten. Else they are extended.", {"PROCESS", getName()}};

    ctk::ScalarPollInput<uint> maxFails{
        this, "maxFails", "", "Set the maximum number of allowed fails.", {"PROCESS", getName()}};
    ctk::ScalarPollInput<uint> maxRestarts{
        this, "maxRestarts", "", "Set the maximum number of allowed restarts.", {"PROCESS", getName()}};
    /** Log file name. It will be created in the given processPath */
    ctk::ScalarPollInput<std::string> externalLogfile{this, "logfileExternal", "",
        "Set the name of the logfile"
        " used by the process to be started. It is created in the given path.",
        {"PROCESS", getName()}};
    /** Signal used to kill the process (2: SIGINT, 9: SIGKILL) */
    ctk::ScalarPollInput<uint> killSig{
        this, "killSig", "", "Signal used to kill the process (2: SIGINT, 9: SIGKILL)", {"PROCESS", getName()}};
    /** PID offset used when monitoring the started process */
    ctk::ScalarPollInput<uint> pidOffset{
        this, "pidOffset", "", "PID offset used when monitoring the started process", {"PROCESS", getName()}};
    ctk::ScalarPollInput<uint> bootDelay{this, "bootDelay", "s",
        "This delay is used at server start. Use this to delay the process start with"
        "respect to other processes.",
        {"PROCESS", getName(), "DAQ"}};
    ctk::ScalarPollInput<uint> killTimeout{this, "killTimeout", "s",
        "This is the maximum time waited for the process to exit after stopping. After, it is"
        " stopped using SIGKILL.",
        {"PROCESS", getName(), "DAQ"}};
  } config{this, "config", "Configuration parameters of the process"};

  /** Start the process */
  ctk::ScalarPollInput<ctk::Boolean> enableProcess{
      this, "enableProcess", "", "Start the process", {"PROCESS", getName()}};

  /**
   * Set the PID and set status to running.
   * \param pid PID of the process that was started.
   */
  void SetOnline(const int& pid);

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
   * \todo: If using a const reference here, starting a process always results in a positive result here.
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
  bool _stop{false};

  /**
   * This flag is used to know if a process is started because a client enabled the process or
   * if it is started because the process terminated without client interaction.
   */
  bool _restartRequired{false};

  /**
   * This flags is  used decide if empty data is written in case the process is not running.
   * This is needed to end up with a meaningful history buffer in case server based history is enabled.
   */
  bool _historyOn;
};

struct ProcessGroup : public ctk::ModuleGroup {
  using ctk::ModuleGroup::ModuleGroup;

  /**
   * \brief This module is used to start and stop subprocess controlled by the watchdog.
   */

  /**
   * vector storing processes
   * The vector is filled during construction using information from the input xml file called:
   * watchdog_server_processes.xml
   */
  std::vector<ProcessControlModule> processes;
  std::vector<LogFileModule> processesLogExternal;
};
