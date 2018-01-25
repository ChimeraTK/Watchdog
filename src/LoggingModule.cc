/*
 * LoggingModule.cc
 *
 *  Created on: Jan 18, 2018
 *      Author: zenker
 */

#include "LoggingModule.h"

void LogFileModule::mainLoop(){
  logFile.read();
  std::string currentFile = (std::string)logFile;
  std::filebuf logFileBuffer;
  logFileBuffer.open(currentFile,std::ios::in);
  std::stringstream messageTail;
  while(1){
    readAll();
    messageTail.str("");
    if(((std::string)logFile).compare(currentFile) != 0){
      logFileBuffer.close();
      currentFile = (std::string)logFile;
      messageTail << "New log file is opened: " << currentFile << std::endl;
      logFileBuffer.open(currentFile,std::ios::in);
    }

    if(logFileBuffer.is_open()){
      std::istream i(&logFileBuffer);
      formatLogTail(i, messageTail, tailLength);
    } else {
      if(currentFile.empty()){
       messageTail << "No log file is set. Try starting the process and setting a LogFile." << std::endl;
      } else {
        messageTail << "Can not open file: " << currentFile << std::endl;
      }
    }
    logTailExtern = messageTail.str();
    logTailExtern.write();
  }
}

void LoggingModule::mainLoop(){
  while(1){
    readAll();
    LogLevel level = static_cast<LogLevel>((uint)messageLevel);
    LogLevel setLevel = static_cast<LogLevel>((uint)logLevel);
    std::stringstream ss;
    ss << level << (std::string)message << std::endl;
    if(level >= setLevel){
      std::string tmpLog  = (std::string)logTail;
      if( tailLength == 0 && messageCounter > 20){
        messageCounter--;
        tmpLog = tmpLog.substr(tmpLog.find_first_of("\n")+1, tmpLog.length());
      } else {
        while(messageCounter >= tailLength){
          messageCounter--;
          tmpLog = tmpLog.substr(tmpLog.find_first_of("\n")+1, tmpLog.length());
        }
      }
      if(level <= LogLevel::ERROR)
        std::cout << ss.str();
      else
        std::cerr << ss.str();

      tmpLog.append(ss.str());
      messageCounter++;
      logTail = tmpLog;
      logTail.write();
    }
  }
}


