/*
 * LoggingModule.cc
 *
 *  Created on: Jan 18, 2018
 *      Author: zenker
 */

#include "LoggingModule.h"

void LogFileModule::mainLoop() {
  logFileGroup.logFile.read();
  //  std::string currentFile = (std::string)logFile;
  logFileBuffer.reset(new std::filebuf);
  logFileBuffer->open((std::string)logFileGroup.logFile, std::ios::in);
  std::stringstream messageTail;
  while(1) {
    readAll();
    messageTail.clear();
    messageTail.str("");
    if(config.tailLength < 1) {
      messageTail << "Tail length is <1. No messages from the log file read." << std::endl;
    }
    else {
      if(((std::string)logFileGroup.logFile).empty()) {
        messageTail << "No log file is set. Try starting the process and setting a LogFile." << std::endl;
      }
      else {
        /**
         * Always reopen the file buffer.
         * If the file buffer is only opened in case the log file name changes the following problem arises:
         * When the process is stopped the buffer is not updated anymore, which is ok. But if the log file is deleted
         * and the process is started again a new log file is created. The buffer will not recognize this and still
         * show the last update (of the log file that was deleted). Updates of the new log file would not be
         * updated here.
         */
        logFileBuffer->open((std::string)logFileGroup.logFile, std::ios::in);
        if(logFileBuffer->is_open()) {
          std::istream i(&(*logFileBuffer));
          logging::formatLogTail(i, messageTail, config.tailLength);
        }
        else {
          messageTail << "Can not open file: " << (std::string)logFileGroup.logFile << std::endl;
        }
        logFileBuffer->close();
      }
    }
    status.logTailExtern = messageTail.str();
    status.logTailExtern.write();
  }
}

void LogFileModule::terminate() {
  if((logFileBuffer.get() != nullptr) && (logFileBuffer->is_open())) logFileBuffer->close();
  ApplicationModule::terminate();
}

void LoggingModule::mainLoop() {
  file.reset(new std::ofstream());
  messageCounter = 0;
  while(1) {
    if(config.targetStream == 3) {
      readAll();
      continue;
    }
    if(((std::string)input.message).empty()) {
      readAll();
      continue;
    }
    logging::LogLevel level = static_cast<logging::LogLevel>((uint)input.messageLevel);
    logging::LogLevel setLevel = static_cast<logging::LogLevel>((uint)config.logLevel);
    std::stringstream ss;
    ss << level << (std::string)input.message;
    if(config.targetStream == 0 || config.targetStream == 1) {
      if(((std::string)logFileGroup.logFile).compare(currentFile.c_str()) != 0 && file->is_open()) {
        // new log file name was set during runtime
        file->close();
      }
      if(!((std::string)logFileGroup.logFile).empty() && !file->is_open()) {
        file->open((std::string)logFileGroup.logFile, std::ofstream::out | std::ofstream::app);
        if(!file->is_open() && level <= logging::LogLevel::ERROR)
          std::cerr << logging::LogLevel::ERROR << this->getName()
                    << "LoggingModule failed to open log file for writing: " << (std::string)logFileGroup.logFile
                    << std::endl;
        else if(file->is_open() && level <= logging::LogLevel::INFO) {
          std::cout << logging::LogLevel::INFO << this->getName()
                    << " Opened log file for writing: " << (std::string)logFileGroup.logFile << std::endl;
          currentFile = (std::string)logFileGroup.logFile;
        }
      }
    }
    if(level >= setLevel) {
      std::string tmpLog = (std::string)status.logTail;
      if(config.tailLength == 0 && messageCounter > 20) {
        messageCounter--;
        tmpLog = tmpLog.substr(tmpLog.find_first_of("\n") + 1, tmpLog.length());
      }
      else if(config.tailLength > 0) {
        while(messageCounter >= config.tailLength) {
          messageCounter--;
          tmpLog = tmpLog.substr(tmpLog.find_first_of("\n") + 1, tmpLog.length());
        }
      }
      if(config.targetStream == 0 || config.targetStream == 2) {
        if(level <= logging::LogLevel::ERROR)
          std::cout << ss.str();
        else
          std::cerr << ss.str();
      }
      if(config.targetStream == 0 || config.targetStream == 1) {
        if(file->is_open()) {
          (*file) << ss.str().c_str();
          file->flush();
        }
      }

      tmpLog.append(ss.str());
      messageCounter++;
      status.logTail = tmpLog;
      status.logTail.write();
    }
    readAll();
  }
}

void LoggingModule::terminate() {
  if((file.get() != nullptr) && (file->is_open())) file->close();
  ApplicationModule::terminate();
}
