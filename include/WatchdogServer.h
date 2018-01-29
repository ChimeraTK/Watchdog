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
#include "LoggingModule.h"

namespace ctk = ChimeraTK;

/**
 * \brief This module is used to trigger the mainloops of the modules to go on.
 */
struct TimerModule: public ctk::ApplicationModule {
  using ctk::ApplicationModule::ApplicationModule;
  /**
   * \remark
   * Observe this vaiable by other modules to obtain a trigger
   */
  ctk::ScalarOutput<int> trigger { this, "trigger", "", "Trigger counter",
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
  std::vector<LoggingModule> processesLog;
  std::vector<LogFileModule> processesLogExternal;

  
  ProcessInfoModule watchdog{this, "watchdog", "Module monitoring the watchdog process"};

  LoggingModule watchdogLog{this, "watchdogLog", "Logging module of the watchdog process"};

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
