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

void formatLogTail(std::istream  &data, std::ostream &os, size_t numberOfLines, size_t maxCharacters){
  int nLines = std::count(std::istreambuf_iterator<char>(data),
               std::istreambuf_iterator<char>(), '\n');
  data.seekg(0, std::ios::beg);
  char s[maxCharacters];
  int line = 0;

  /* Don't use while(data.getline(2,25), because lines exceeding 256 characters would be dropped */
  while(data.good()){
    data.getline(s, maxCharacters);
    if(data.eof()){
      break;
    }
    line++;
    // also allow negative values here in case there are less lines in the data stream compared to the number of lines to be written.
    if((nLines-line) < (int)numberOfLines){
      if(data.fail()){
        os << s << "|\n The above line was cut by logger!" << std::endl;
        data.clear();
      } else {
        os << s << std::endl;
      }
    } else {
      if(data.fail())
        data.clear();
    }
  }
}

void LogFileModule::mainLoop(){
  logFile.read();
  std::string currentFile = (std::string)logFile;
  std::filebuf logFileBuffer;
  logFileBuffer.open(currentFile,std::ios::in);
  std::stringstream messageTail;
  while(1){
    trigger.read();
    logFile.read();
    messageTail.str("");
    if(((std::string)logFile).compare(currentFile) != 0){
      logFileBuffer.close();
      currentFile = (std::string)logFile;
      messageTail << "New log file is opened: " << currentFile << std::endl;
      logFileBuffer.open(currentFile,std::ios::in);
    }

    if(logFileBuffer.is_open()){
      std::istream i(&logFileBuffer);
      tailLength.read();
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
    Message msg((std::string)message, static_cast<LogLevel>((uint)messageLevel));
    if(msg.logLevel >= static_cast<LogLevel>((uint)logLevel)){
      messageList.emplace(msg);
      std::string tmpLog  = (std::string)logTail;
      if((tailLength == 0 && messageList.size() > 20) ||
          (tailLength > 0 && messageList.size() > tailLength)){
        messageList.pop();
        tmpLog = tmpLog.substr(tmpLog.find_first_of("\n")+1, tmpLog.length());
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


