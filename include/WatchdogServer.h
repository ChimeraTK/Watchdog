/*
 * WatchdogServer.h
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#ifndef INCLUDE_WATCHDOGSERVER_H_
#define INCLUDE_WATCHDOGSERVER_H_

#undef GENERATE_XML
#include <ChimeraTK/ApplicationCore/ApplicationCore.h>

#include "SystemInfoModule.h"
#include "ProcessModule.h"

#ifdef ENABLE_LOGGING
#include "LoggingModule.h"
#endif

namespace ctk = ChimeraTK;

/**
 * \brief This module is used to trigger the mainloops of the modules to go on.
 */
struct TimerModule: public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  /**
   * \remark
   * Observe this variable by other modules to obtain a trigger
   */
  ctk::ScalarOutput<int> trigger { this, "trigger", "", "Trigger counter",
    { "Timer" }};

  ctk::ScalarPollInput<int> update {this , "update", "", "Specify the amount of time given in seconds between update triggers.",
    { "Timer" }};

  /**
   * Application core main loop.
   */
  void mainLoop();
};

/**
 * \brief The watchdog application
 *
 * So far from the watchdog_server_processes.xml file only the process name is used.
 * The initial values are not used so far, since these values can not be set in the constructor.
 * They need to be set at a later stage, which requires rephrasing the xml.
 * \todo Check if initial values are needed?
 */
struct WatchdogServer: public ctk::Application {
  WatchdogServer();
  ~WatchdogServer() {
    shutdown();
  }

  SystemInfoModule systemInfo { this, "systeminfo",
      "Module reading system information" };

  /**
   * vector storing processes
   * The vector is filled during construction using information from the input xml file called:
   * watchdog_server_processes.xml
   * If that file is not found only one process named PROCESS is added.
   */
  std::vector<ProcessControlModule> processes;

  ProcessInfoModule watchdog{this, "watchdog", "Module monitoring the watchdog process"};

#ifdef ENABLE_LOGGING
  std::vector<LoggingModule> processesLog;
  std::vector<LogFileModule> processesLogExternal;

  /**
   * This module is used to read the watchdog log file, which includes messages from the
   * watchdog process and all other processes controlled by the watchdog.
   * This allows to show all watchdog related messages via the tail (LogfileTailExternal).
   * The LogFileModule is used differently for all the processes, since in case of the processes
   * the  massages produced by the started processes are read and shown in the corresponding
   * tail (LogfileTailExternal). Thus in that case the postfix 'External' makes more sense. Here
   * the postfix 'External' can be a bit misleading, because the tail (LogfileTailExternal) includes
   * only messages produced by the watchdog itself.
   *
   * \remark It would be nice to connect the message output variable of each process to its LoggingModule
   * AND the watchdog LoggingModule. But this is not possible, since it is not possible to connect multiple
   * outputs to a single push input variable.
   */
  LogFileModule watchdogLogFile{this, "watchdogLog", "Logging module of all watchdog processes"};

  /**
   * This module is used to handle messages from the watchdog process it self.
   */
  LoggingModule watchdogLog{this, "watchdogLog", "Logging module of the watchdog process"};

  LoggingModule systemInfoLog{this, "systeminfoLog", "Logging module of the system information module"};
#endif
  /**
   * Use either
   * - ctk::ControlSystemModule cs;
   *   in combinartion with
   *   cs["SYS"]("val")
   * or
   * - ctk::ControlSystemModule cs{"SYS"};
   *   in combination with
   *   cs("val")
   * In any case SYS defines the location in DOOCS.
   * SYS/TEST would create another hierarchy that would result
   * in SYS/TEST.val in DOOCS.
   *
   */
  ctk::ControlSystemModule cs;

  TimerModule timer { this, "timer",
      "Module used to trigger the watchdog update" };

  void defineConnections();

};

#endif /* INCLUDE_WATCHDOGSERVER_H_ */
