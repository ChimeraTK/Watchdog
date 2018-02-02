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
  logFileBuffer.reset(new std::filebuf);
  logFileBuffer->open(currentFile,std::ios::in);
  std::stringstream messageTail;
  while(1){
    readAll();
    messageTail.clear();
    messageTail.str("");
    if(tailLength < 1){
      messageTail << "Tail length is <1. No messages from the log file read." << std::endl;
    } else {
      if(((std::string)logFile).compare(currentFile) != 0){
        logFileBuffer->close();
        currentFile = (std::string)logFile;
        messageTail << "New log file is opened: " << currentFile << std::endl;
        logFileBuffer->open(currentFile,std::ios::in);
      }

      if(logFileBuffer->is_open()){
        std::istream i(&(*logFileBuffer));
        logging::formatLogTail(i, messageTail, tailLength);
      } else {
        if(currentFile.empty()){
         messageTail << "No log file is set. Try starting the process and setting a LogFile." << std::endl;
        } else {
          messageTail << "Can not open file: " << currentFile << std::endl;
        }
      }
    }
    logTailExtern = messageTail.str();
    logTailExtern.write();
  }
}

void LogFileModule::terminate(){
  if((logFileBuffer.get() != nullptr) && (logFileBuffer->is_open()))
    logFileBuffer->close();
  ApplicationModule::terminate();
}

void LoggingModule::mainLoop(){
  file.reset(new std::ofstream());
  messageCounter = 0;
  while(1){
    readAll();
    if(targetStream == 3)
      continue;
    logging::LogLevel level = static_cast<logging::LogLevel>((uint)messageLevel);
    logging::LogLevel setLevel = static_cast<logging::LogLevel>((uint)logLevel);
    std::stringstream ss;
    ss << level << (std::string)message;
    if(targetStream == 0 || targetStream == 1){
      if(!((std::string)logFile).empty() && !file->is_open()){
        file->open((std::string)logFile,  std::ofstream::out | std::ofstream::app);
        if(!file->is_open() && level <= logging::LogLevel::ERROR)
          std::cerr << logging::LogLevel::ERROR << this->getName() << "LoggingModule failed to open log file for writing: " << (std::string)logFile << std::endl;
        else if (file->is_open() && level <= logging::LogLevel::INFO)
          std::cout << logging::LogLevel::INFO << this->getName() << " Opened log file for writing: " << (std::string)logFile << std::endl;
      }
    }
    if(level >= setLevel){
      std::string tmpLog  = (std::string)logTail;
      if( tailLength == 0 && messageCounter > 20){
        messageCounter--;
        tmpLog = tmpLog.substr(tmpLog.find_first_of("\n")+1, tmpLog.length());
      } else if (tailLength > 0){
        while(messageCounter >= tailLength){
          messageCounter--;
          tmpLog = tmpLog.substr(tmpLog.find_first_of("\n")+1, tmpLog.length());
        }
      }
      if(targetStream == 0 || targetStream == 2){
        if(level <= logging::LogLevel::ERROR)
          std::cout << ss.str();
        else
          std::cerr << ss.str();
      }
      if(targetStream == 0 || targetStream == 1){
        if(file->is_open()){
          (*file) << ss.str().c_str();
          file->flush();
        }
      }

      tmpLog.append(ss.str());
      messageCounter++;
      logTail = tmpLog;
      logTail.write();
    }
  }
}

void LoggingModule::terminate(){
  if((file.get() != nullptr) && (file->is_open()))
    file->close();
  ApplicationModule::terminate();
}


