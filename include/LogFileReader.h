/*
 * LoggingModule.h
 *
 *  Created on: Jan 18, 2018
 *      Author: zenker
 */

#ifndef INCLUDE_LOGFILEREADER_H_
#define INCLUDE_LOGFILEREADER_H_

#undef GENERATE_XML

#include <fstream>
#include <ChimeraTK/ApplicationCore/ApplicationCore.h>
#include <ChimeraTK/ApplicationCore/HierarchyModifyingGroup.h>
#include "Logging.h"

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
  LogFileModule(EntityOwner* owner, const std::string& name, const std::string& description,
      const std::string& pathToTrigger, const std::string& pathToLogFile,
      ctk::HierarchyModifier hierarchyModifier = ctk::HierarchyModifier::none,
      const std::unordered_set<std::string>& tags = {})
  : ctk::ApplicationModule(owner, name, description, hierarchyModifier, tags),
    triggerGroup(this, pathToTrigger, {"CS"}), logFileGroup(this, pathToLogFile, {"CS"}) {}

  struct TriggerGroup : ctk::HierarchyModifyingGroup {
    TriggerGroup(EntityOwner* owner, const std::string& pathToTrigger, const std::unordered_set<std::string>& tags = {})
    : ctk::HierarchyModifyingGroup(owner, ctk::HierarchyModifyingGroup::getPathName(pathToTrigger), "", tags),
      trigger{this, HierarchyModifyingGroup::getUnqualifiedName(pathToTrigger), "", "Trigger input"} {}

    TriggerGroup() {}

    ctk::ScalarPushInput<uint64_t> trigger;
  } triggerGroup;

  struct Config : ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarPollInput<uint> tailLength{
        this, "logTailLengthExternal", "", "Maximum number of messages to be shown in the lofgile tail.", {"CS"}};
  } config{this, "config", "Configuration parameters of the process"};

  struct Status : ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarOutput<std::string> logTailExtern{this, "logTailExternal", "",
        "Tail of an external log file, e.g. produced by a program started by the watchdog.",
        {"CS", "PROCESS", getName()}};
  } status{this, "status", "Status parameter of the process"};

  struct LogFileGroup : ctk::HierarchyModifyingGroup {
    LogFileGroup(EntityOwner* owner, const std::string& pathToLogFile, const std::unordered_set<std::string>& tags = {})
    : ctk::HierarchyModifyingGroup(owner, ctk::HierarchyModifyingGroup::getPathName(pathToLogFile), "", tags),
      logFile{this, HierarchyModifyingGroup::getUnqualifiedName(pathToLogFile), "", "Trigger input"} {}

    LogFileGroup() {}

    ctk::ScalarPollInput<std::string> logFile;
  } logFileGroup;

  std::unique_ptr<std::filebuf> logFileBuffer;

  /**
   * Application core main loop.
   */
  void mainLoop() override;

  void terminate();
};

#endif /* INCLUDE_LOGFILEREADER_H_ */
