/*
 * LoggingModule.cc
 *
 *  Created on: Jan 18, 2018
 *      Author: zenker
 */

#include "LoggingModule.h"

std::string Message::getMessage(){
  switch(logLevel){
    case LogLevel::DEBUG:
      return std::string("DEBUG::")+message;
      break;
    case LogLevel::INFO:
      return std::string("INFO::")+message;
      break;
    case LogLevel::WARNING:
      return std::string("WARNING::")+message;
      break;
    case LogLevel::ERROR:
      return std::string("ERROR::")+message;
      break;
    default:
      return message;
      break;
  }
}

void LoggingModule::mainLoop(){
  while(1){
    readAll();
    Message msg((std::string)message, static_cast<LogLevel>((uint)messageLevel));
    if(msg.logLevel >= static_cast<LogLevel>((uint)logLevel)){
      messageList.emplace(msg);
      std::string tmpLog  = (std::string)logTail;
      if(tailLength > 0 && messageList.size() > tailLength){
        messageList.pop();
        tmpLog = tmpLog.substr(tmpLog.find_first_of("\n"), tmpLog.length());
      }
      if(msg.logLevel == LogLevel::ERROR)
        std::cout << msg.getMessage() << std::endl;
      else
        std::cerr << msg.getMessage() << std::endl;

      tmpLog.append(msg.getMessage() + std::string("\n"));
      logTail = tmpLog;
      logTail.write();
    }
  }
}


