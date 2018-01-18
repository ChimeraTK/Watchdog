/*
 * LoggingModule.h
 *
 *  Created on: Jan 18, 2018
 *      Author: zenker
 */

#ifndef INCLUDE_LOGGINGMODULE_H_
#define INCLUDE_LOGGINGMODULE_H_

#include <queue>

#undef GENERATE_XML
#include <ChimeraTK/ApplicationCore/ApplicationCore.h>

namespace ctk = ChimeraTK;

enum LogLevel { DEBUG, INFO, WARNING, ERROR };

struct Message{
  std::string message;
  LogLevel logLevel;
  Message(const std::string &msg, const LogLevel &level):
   message(msg), logLevel(level) { }
  Message():
    message(""), logLevel(LogLevel::INFO) { }
  std::string getMessage();
};

struct LoggingModule: public ctk::ApplicationModule {

  std::queue<Message> messageList;

  using ctk::ApplicationModule::ApplicationModule;

  ctk::ScalarPushInput<std::string> message { this, "message", "",
      "Message to be logged." };

  ctk::ScalarPushInput<uint> messageLevel { this, "messageLevel", "",
        "Message log level." };

  ctk::ScalarPollInput<std::string> logFile { this, "logfile", "",
      "Name of the logfile" };

  ctk::ScalarPollInput<uint> tailLength { this, "maxLength", "",
      "Maximum number of messages to be shown in the lofgile tail." };

  ctk::ScalarPollInput<uint> logLevel { this, "loglevel", "",
      "Current log level used for messages." };

  ctk::ScalarOutput<std::string> logTail { this, "LogfileTail", "", "Tail of the logfile.",
      { "CS", "PROCESS", getName() } };

  /**
   * Application core main loop.
   */
  virtual void mainLoop();

};
#endif /* INCLUDE_LOGGINGMODULE_H_ */
