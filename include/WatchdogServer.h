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
#include <ChimeraTK/ApplicationCore/ConfigReader.h>
#include <ChimeraTK/ApplicationCore/PeriodicTrigger.h>
#include <ChimeraTK/ApplicationCore/ServerHistory.h>

#include "SystemInfoModule.h"
#include "ProcessModule.h"

#ifdef WITHDAQ
#include <ChimeraTK/ApplicationCore/MicroDAQ.h>
#endif

#ifdef ENABLE_LOGGING
#include "LoggingModule.h"
#endif

namespace ctk = ChimeraTK;

struct WatchdogModuleGroup : ctk::ModuleGroup{
  using ctk::ModuleGroup::ModuleGroup;
  ProcessInfoModule process{this, "watchdog", "Module monitoring the watchdog process", ctk::HierarchyModifier::hideThis};
#ifdef ENABLE_LOGGING
  /**
   * This module is used to read the watchdog log file, which includes messages from the
   * watchdog process and all other processes controlled by the watchdog.
   * This allows to show all watchdog related messages via the tail (\c status/logTailExternal).
   * The LogFileModule is used differently for all the processes, since in case of the processes
   * the  massages produced by the started processes are read and shown in the corresponding
   * tail (\c status/logTailExternal). Thus in that case the postfix 'External' makes more sense. Here
   * the postfix 'External' can be a bit misleading, because the tail (\c status/logTailExternal) includes
   * only messages produced by the watchdog itself.
   *
   * \remark It would be nice to connect the message output variable of each process to its LoggingModule
   * AND the watchdog LoggingModule. But this is not possible, since it is not possible to connect multiple
   * outputs to a single push input variable.
   */
  LogFileModule logFile{this, "watchdogLogFile", "Logging module reading the watchdog logfile", "/Configuration/tick", "/watchdog/config/logfile", ctk::HierarchyModifier::hideThis};

  /**
   * This module is used to handle messages from the watchdog process it self.
   */
  LoggingModule logging{this, "watchdogLog", "Logging module of the watchdog process", ctk::HierarchyModifier::hideThis};
#endif
};

struct SystemInfoGroup : ctk::ModuleGroup{
  using ctk::ModuleGroup::ModuleGroup;

  SystemInfoModule info{this, "system", "Module reading system information", ctk::HierarchyModifier::hideThis};
#ifdef ENABLE_LOGGING
  LoggingModule logging{this, "systeminfoLog", "Logging module of the system information module", ctk::HierarchyModifier::hideThis};
#endif
};

/**
 * \brief The watchdog application
 *
 * WatchdogServerConfig.xml file is used to read the number of processes and to enable/disable
 * - microDAQ
 * - history
 */
struct WatchdogServer: public ctk::Application {
  WatchdogServer();
  ~WatchdogServer() {
    shutdown();
  }
  ctk::ControlSystemModule cs;

  ctk::PeriodicTrigger trigger{this, "Trigger", "Trigger used for other modules"};

  ctk::ConfigReader config{this, "Configuration", "WatchdogServerConfig.xml"};

  SystemInfoGroup systemInfo { this, "system", "Module reading system information" };

  ProcessGroup processGroup{this, "processes", "Process module group"};

  FileSystemGroup filesystemGroup{this, "filesystem", "File system module group"};

  NetworkGroup networkGroup{this, "network", "Network module group"};

  WatchdogModuleGroup watchdog{this, "watchdog", "Module monitoring the watchdog process"};

#ifdef WITHDAQ
  ctk::MicroDAQ<uint64_t> daq;
#endif
  /*
   * History module if history is enabled in the config file.
   * In th config file use:
   * <variable name="enableServerHistory" type="int32" value="1" />
   * <variable name="serverHistoryLength" type="int32" value="1200" />
   */
  ctk::history::ServerHistory history;

  void defineConnections() override;

};

#endif /* INCLUDE_WATCHDOGSERVER_H_ */
