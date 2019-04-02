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
#include <ChimeraTK/ApplicationCore/MicroDAQ.h>
#include "ChimeraTK/ApplicationCore/PeriodicTrigger.h"
#include "ChimeraTK/ApplicationCore/ServerHistory.h"

#include "SystemInfoModule.h"
#include "ProcessModule.h"


#ifdef ENABLE_LOGGING
#include "LoggingModule.h"
#endif

namespace ctk = ChimeraTK;

/*
 * Module used to convert trigger signal from PeriodicTrigger for MicroDAQ
 * \FixMe: Remove once MicroDAQ is using uint
 */
struct ConversionModule : public ctk::ApplicationModule{
  using ctk::ApplicationModule::ApplicationModule;

  ctk::ScalarPushInput<uint64_t> triggerIn { this, "triggerIn", "",
      "Trigger from the PeriodicTrigger Module" };
  ctk::ScalarOutput<int> triggerOut { this, "triggerOut", "",
      "Trigger for the MicroDAQ module" };
  /**
   * Application core main loop.
   */
  virtual void mainLoop(){
    while(1){
      triggerIn.read();
      triggerOut = triggerIn;
      triggerOut.write();
    }
  }
};


/**
 * \brief The watchdog application
 *
 * WatchdogServerConfig.xml file is used to read process names and microDAQ enable/disable is used.
 */
struct WatchdogServer: public ctk::Application {
  WatchdogServer();
  ~WatchdogServer() {
    shutdown();
  }

  SystemInfoModule systemInfo { this, "system",
      "Module reading system information" };

  ProcessGroup processGroup{this, "process", "Process module group"};

  FileSystemGroup filesystemGroup{this, "filesystem", "File system module group"};

  NetworkGroup networkGroup{this, "network", "Network module group"};


  ProcessInfoModule watchdog{this, "watchdog", "Module monitoring the watchdog process"};

  ctk::ConfigReader config{this, "Configuration", "WatchdogServerConfig.xml"};

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

  /*
   * Module used to convert trigger signal from PeriodicTrigger for MicroDAQ
   * \FixMe: Remove once MicroDAQ is using uint
   */
  ConversionModule conversion;

  ctk::MicroDAQ microDAQ;

  /*
   * History module if history is enabled in the config file.
   * In th config file use:
   * <variable name="enableServerHistory" type="int32" value="1" />
   * <variable name="serverHistoryLength" type="int32" value="1200" />
   */
  ctk::history::ServerHistory history;

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

  ctk::PeriodicTrigger trigger{this, "Trigger", "Trigger used for other modules"};

  void defineConnections() override;

};

#endif /* INCLUDE_WATCHDOGSERVER_H_ */
