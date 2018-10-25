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
  using ctk::ApplicationModule::ApplicationModule;

  ctk::ScalarPushInput<uint64_t> trigger { this, "trigger", "",
    "Trigger used to update the watchdog" };

  ctk::ScalarPollInput<std::string> logFile { this, "logFile", "",
    "Name of the external logfile, e.g. produced by a program started by the watchdog." };

  ctk::ScalarPollInput<uint> tailLength { this, "maxLength", "",
    "Maximum number of messages to be shown in the lofgile tail." };

  ctk::ScalarOutput<std::string> logTailExtern { this, "LogfileTailExternal", "",
    "Tail of an external log file, e.g. produced by a program started by the watchdog.",
    { "CS", "PROCESS", getName() } };

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

  using ctk::ApplicationModule::ApplicationModule;

  ctk::ScalarPushInput<std::string> message { this, "message", "",
      "Message to be logged." };

  ctk::ScalarPushInput<uint> messageLevel { this, "messageLevel", "",
        "Message log level." };

  ctk::ScalarPollInput<uint> targetStream { this, "targetStream", "",
          "Set the tagret stream: 0 (cout/cerr+logfile), 1 (logfile), 2 (cout/cerr), 3 (none)" };

  ctk::ScalarPollInput<std::string> logFile { this, "Logfile", "",
    "Name of the external logfile. If empty messages are pushed to cout/cerr" };

  ctk::ScalarPollInput<uint> tailLength { this, "maxLength", "",
      "Maximum number of messages to be shown in the logging stream tail." };

  ctk::ScalarPollInput<uint> logLevel { this, "logLevel", "",
      "Current log level used for messages." };

  ctk::ScalarOutput<std::string> logTail { this, "LogTail", "", "Tail of the logging stream.",
      { "CS", "PROCESS", getName() } };

  std::unique_ptr<std::ofstream> file; ///< Log file where to write log messages

  /**
   * Application core main loop.
   */
  virtual void mainLoop();

  void terminate();

};
#endif /* INCLUDE_LOGGINGMODULE_H_ */
