// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

/*
 * LoggingModule.cc
 *
 *  Created on: Jan 18, 2018
 *      Author: Klaus Zenker (HZDR)
 */

#include "LogFileReader.h"

void LogFileModule::mainLoop() {
  logFile.read();
  //  std::string currentFile = (std::string)logFile;
  logFileBuffer.reset(new std::filebuf);
  logFileBuffer->open((std::string)logFile, std::ios::in);
  std::stringstream messageTail;
  while(1) {
    readAll();
    messageTail.clear();
    messageTail.str("");
    if(config.tailLength < 1) {
      messageTail << "Tail length is <1. No messages from the log file read." << std::endl;
    }
    else {
      if(((std::string)logFile).empty()) {
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
        logFileBuffer->open((std::string)logFile, std::ios::in);
        if(logFileBuffer->is_open()) {
          std::istream i(&(*logFileBuffer));
          logging::formatLogTail(i, messageTail, config.tailLength);
        }
        else {
          messageTail << "Can not open file: " << (std::string)logFile << std::endl;
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
