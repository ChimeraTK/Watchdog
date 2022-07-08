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
#include <ChimeraTK/ApplicationCore/DataLossCounter.h>
#include <ChimeraTK/ApplicationCore/MicroDAQ.h>
#include <ChimeraTK/ApplicationCore/Logging.h>

#include "SystemInfoModule.h"
#include "ProcessModule.h"

#include "LoggingModule.h"

namespace ctk = ChimeraTK;

struct WatchdogModuleGroup : ctk::ModuleGroup {
  using ctk::ModuleGroup::ModuleGroup;
  ProcessInfoModule process{
      this, "watchdog", "Module monitoring the watchdog process", ctk::HierarchyModifier::hideThis};
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
  LogFileModule logFile{this, "watchdogLogFile", "Logging module reading the watchdog logfile", "/Trigger/tick",
      "/watchdog/config/logFile", ctk::HierarchyModifier::hideThis};
};

struct SystemInfoGroup : ctk::ModuleGroup {
  using ctk::ModuleGroup::ModuleGroup;

  SystemInfoModule info{this, "system", "Module reading system information", ctk::HierarchyModifier::hideThis};
};

/**
 * \brief The watchdog application
 *
 * WatchdogServerConfig.xml file is used to read the number of processes and to enable/disable
 * - microDAQ
 * - history
 */
struct WatchdogServer : public ctk::Application {
  WatchdogServer();
  WatchdogServer(const WatchdogServer&) = delete;
  WatchdogServer& operator=(WatchdogServer const&) = delete;
  WatchdogServer(WatchdogServer&&) = delete;
  WatchdogServer& operator=(WatchdogServer&&) = delete;
  ~WatchdogServer() { shutdown(); }

  ctk::PeriodicTrigger trigger{this, "Trigger", "Trigger used for other modules"};

  ctk::ConfigReader config{this, "Configuration", "WatchdogServerConfig.xml", ctk::HierarchyModifier::hideThis};

  SystemInfoGroup systemInfo{this, "system", "Module reading system information"};

  ProcessGroup processGroup{this, "processes", "Process module group"};

  FileSystemGroup filesystemGroup{this, "filesystem", "File system module group"};

  NetworkGroup networkGroup{this, "network", "Network module group"};

  WatchdogModuleGroup watchdog{this, "watchdog", "Module monitoring the watchdog process"};

  ctk::DataLossCounter<uint64_t> dataLossCounter{
      this, "DataLossCounter", "Statistics on lost data within this watchdog server", ctk::HierarchyModifier::none};

  logging::LoggingModule logging;

  ctk::MicroDAQ<uint64_t> daq;
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
