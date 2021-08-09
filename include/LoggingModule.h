/*
 * LoggingModule.h
 *
 *  Created on: Jan 18, 2018
 *      Author: zenker
 */

#ifndef INCLUDE_LOGGINGMODULE_H_
#define INCLUDE_LOGGINGMODULE_H_

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
struct LogFileModule: public ctk::ApplicationModule {
  LogFileModule(EntityOwner *owner, const std::string &name,
          const std::string &description,
          const std::string& pathToTrigger,
          const std::string& pathToLogFile,
          ctk::HierarchyModifier hierarchyModifier = ctk::HierarchyModifier::none,
          const std::unordered_set<std::string> &tags = { }
          ):
            ctk::ApplicationModule(owner, name, description, hierarchyModifier, tags),
              triggerGroup(this, pathToTrigger, {"CS"}),
              logFileGroup(this, pathToLogFile, {"CS"}){}

  struct TriggerGroup : ctk::HierarchyModifyingGroup {
    TriggerGroup(EntityOwner* owner, const std::string& pathToTrigger,
                 const std::unordered_set<std::string>& tags = {})
    : ctk::HierarchyModifyingGroup(owner, ctk::HierarchyModifyingGroup::getPathName(pathToTrigger), "", tags),
      trigger{this, HierarchyModifyingGroup::getUnqualifiedName(pathToTrigger), "", "Trigger input"} {}

    TriggerGroup() {}

    ctk::ScalarPushInput<uint64_t> trigger;
  } triggerGroup;

  struct Config: ctk::VariableGroup{
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarPollInput<uint> tailLength { this, "logTailLengthExternal", "",
      "Maximum number of messages to be shown in the lofgile tail.", {"CS"}};
  } config {this, "config", "Configuration parameters of the process"};

  struct Status: ctk::VariableGroup{
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarOutput<std::string> logTailExtern { this, "logTailExternal", "",
      "Tail of an external log file, e.g. produced by a program started by the watchdog.",
      { "CS", "PROCESS", getName() } };
  } status {this, "status", "Status parameter of the process"};

  struct LogFileGroup : ctk::HierarchyModifyingGroup {
    LogFileGroup(EntityOwner* owner, const std::string& pathToLogFile,
                 const std::unordered_set<std::string>& tags = {})
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

/**
 * \brief Module used to handle logging messages.
 *
 * A ChimeraTK module is producing messages, that are send to the LoggingModule
 * via the \c message variable. The message is then put into the logfile ring buffer
 * and published in the \c LogFileTail. In addidtion the message can be put to an ostream.
 * Available streams are:
 * - file stream
 * - cout/cerr
 *
 * You can control which stream is used by setting the targetStream varibale:
 * 0: cout/cerr and logfile
 * 1: logfile
 * 2: cout/cerr
 * 3: none
 *
 * The logfile is given by the client using the logFile variable.
 */
struct LoggingModule: public ctk::ApplicationModule {

  /** Number of messages stored in the tail */
  size_t messageCounter;

  LoggingModule(EntityOwner *owner, const std::string &name,
      const std::string &description, ctk::HierarchyModifier hierarchyModifier = ctk::HierarchyModifier::none,
      const std::unordered_set<std::string> &tags = { },
      const std::string &pathToLogFile="/watchdog/config/logFile"):
        ctk::ApplicationModule(owner, name, description, hierarchyModifier, tags),
        messageCounter(0), logFileGroup(this, pathToLogFile, {"CS"}) {};

  struct Input : ctk::VariableGroup{
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarPushInput<std::string> message { this, "message", "",
        "Message to be logged." };

    ctk::ScalarPushInput<uint> messageLevel { this, "messageLevel", "",
          "Message log level." };
  } input {this, "input", "Input to the LoggingModule"};

  struct Config : public ctk::VariableGroup {
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarPollInput<uint> targetStream { this, "targetStream", "",
            "Set the tagret stream: 0 (cout/cerr+logfile), 1 (logfile), 2 (cout/cerr), 3 (none)",
    {"CS"}};

    ctk::ScalarPollInput<uint> tailLength { this, "logTailLength", "",
        "Maximum number of messages to be shown in the logging stream tail.",
    {"CS"}};

    ctk::ScalarPollInput<uint> logLevel { this, "logLevel", "",
        "Current log level used for messages.", {"CS"} };

  } config {this, "config", "Configuration parameters of the process"};

  struct Status : ctk::VariableGroup{
    using ctk::VariableGroup::VariableGroup;
    ctk::ScalarOutput<std::string> logTail { this, "logTail", "", "Tail of the logging stream.",
        { "CS", "PROCESS", getName() } };
  } status {this, "status", "Status parameter of the process"};


  struct LogFileGroup : ctk::HierarchyModifyingGroup {
    LogFileGroup(EntityOwner* owner, const std::string& pathToLogFile,
                 const std::unordered_set<std::string>& tags = {})
    : ctk::HierarchyModifyingGroup(owner, ctk::HierarchyModifyingGroup::getPathName(pathToLogFile), "", tags),
      logFile{this, HierarchyModifyingGroup::getUnqualifiedName(pathToLogFile), "", "Logfile input"} {}

      LogFileGroup() {}
    /** Log file name. It will be created in the given processPath */
    ctk::ScalarPollInput<std::string> logFile;
  } logFileGroup;

  std::unique_ptr<std::ofstream> file; ///< Log file where to write log messages

  std::string currentFile; ///<Name of the current log file

  /**
   * Application core main loop.
   */
  virtual void mainLoop();

  void terminate();

};
#endif /* INCLUDE_LOGGINGMODULE_H_ */
