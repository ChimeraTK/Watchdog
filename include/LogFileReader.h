// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

/*
 * LoggingModule.h
 *
 *  Created on: Jan 18, 2018
 *      Author: Klaus Zenker (HZDR)
 */

#include "Logging.h"

#include <ChimeraTK/ApplicationCore/ApplicationCore.h>

#include <fstream>

namespace ctk = ChimeraTK;

/**
 * \brief Module used to read external log file in order to make messages available
 * to the control system.
 *
 * E.g. the ChimeraTk watchdog starts a process and its output is not available by the watchdog.
 * But the log file produced by the process can be read. If the log file is set
 * via \c logFileExternal it is parsed and the tail is published to \c LogFileTailExternal.
 */
struct LogFileModule : public ctk::ApplicationModule {
  LogFileModule(ctk::ModuleGroup* owner, const std::string& name, const std::string& description,
      const std::string& pathToTrigger, const std::string& pathToLogFile,
      const std::unordered_set<std::string>& tags = {})
  : ctk::ApplicationModule(owner, name, description, tags), trigger(this, pathToTrigger, "", "Trigger input"),
    logFile(this, pathToLogFile, "", "Logfile name") {}

  struct Config : ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarPollInput<uint> tailLength{
        this, "logTailLengthExternal", "", "Maximum number of messages to be shown in the lofgile tail."};
  } config{this, "config", "Configuration parameters of the process"};

  struct Status : ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarOutput<std::string> logTailExtern{this, "logTailExternal", "",
        "Tail of an external log file, e.g. produced by a program started by the watchdog.", {getName()}};
  } status{this, "status", "Status parameter of the process"};

  ctk::ScalarPushInput<uint64_t> trigger;
  ctk::ScalarPollInput<std::string> logFile;

  std::unique_ptr<std::filebuf> logFileBuffer;

  /**
   * Application core main loop.
   */
  void mainLoop() override;

  void terminate();
};
