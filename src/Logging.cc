/*
 * Logging.cc
 *
 *  Created on: Jan 22, 2018
 *      Author: zenker
 */
#include "Logging.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <ctime>

using namespace logging;

std::ostream& operator<<(std::ostream &os,const LogLevel &level){
  switch(level){
    case DEBUG:
      os << "DEBUG::";
      break;
    case INFO:
      os << "INFO::";
      break;
    case WARNING:
      os << "WARNING::";
      break;
    case ERROR:
      os << "ERROR::";
      break;
    default:
      break;
  }
  return os;
}

void logging::formatLogTail(std::istream  &data, std::ostream &os, size_t numberOfLines, size_t maxCharacters){
  data.seekg(0, std::ios::beg);
  int nLines = std::count(std::istreambuf_iterator<char>(data),
               std::istreambuf_iterator<char>(), '\n');
  data.seekg(0, std::ios::beg);
  char s[maxCharacters];
  int line = 0;

  /* Don't use while(data.getline(2,25), because lines exceeding 256 characters would be dropped */
  while(data.good()){
    data.getline(s, maxCharacters);
    if(data.eof()){
      if(line != nLines){
        os << "ERROR->When parsing the log file not all lines where parsed. " << line << "/" << nLines;
      }
      break;
    }
    line++;
    // Check if more lines than expected are parsed - this happens if the log file is updated during reading the log file here
    if((nLines-line) < 0){
      os << "WARNING->New lines where entered during parsing external log file.";
      break;
    }
    // also allow negative values here in case there are less lines in the data stream compared to the number of lines to be written.
    if((nLines-line) < (int)numberOfLines){
      if(data.fail()){
        os << s << "|\n The above line was cut by logger!" << std::endl;
      } else {
        os << s << std::endl;
      }
    }
    if(data.fail()){
      /* In case the line is longer than maxCharacters getline stopped at the maxCharacters character.
       * Now continue reading until end of line is reached.
       */
      while(data.fail()){
        data.clear();
        data.getline(s, maxCharacters);
      }
    }
  }
}

std::vector<Message> logging::stripMessages(std::stringstream &msg, size_t maxCharacters){
  std::vector<Message> l;
  char s[maxCharacters];
  while(msg.good()){
    Message singleMsg;
    msg.getline(s, maxCharacters);
    if(msg.eof()){
      break;
    }
    if(msg.fail()){
      singleMsg.message << s << "|\n The above line was cut by logger!" << std::endl;
    } else {
      singleMsg.message << s;
    }
    if(auto pos = singleMsg.message.str().find("INFO::") != std::string::npos){
      singleMsg.logLevel = LogLevel::INFO;
      singleMsg.message.str(singleMsg.message.str().substr(pos+5,singleMsg.message.str().length()));
    } else if(auto pos = singleMsg.message.str().find("WARNING::") != std::string::npos){
      singleMsg.logLevel = LogLevel::WARNING;
      singleMsg.message.str(singleMsg.message.str().substr(pos+8,singleMsg.message.str().length()));
    } else if(auto pos = singleMsg.message.str().find("ERROR::") != std::string::npos){
      singleMsg.message.str(singleMsg.message.str().substr(pos+6,singleMsg.message.str().length()));
    } else if(auto pos = singleMsg.message.str().find("SILENT::") != std::string::npos){
      singleMsg.logLevel = LogLevel::SILENT;
      singleMsg.message.str(singleMsg.message.str().substr(pos+7,singleMsg.message.str().length()));
    } else if(auto pos = singleMsg.message.str().find("DEBUG::") != std::string::npos){
      singleMsg.logLevel = LogLevel::DEBUG;
      singleMsg.message.str(singleMsg.message.str().substr(pos+6,singleMsg.message.str().length()));
    } else {
      singleMsg.logLevel = LogLevel::DEBUG;
    }

    l.push_back(std::move(singleMsg));
  }

  return l;
}

Message::Message(const std::string &msg, const LogLevel &level) :
    logLevel(level) {
  message.str("");
  message << level << msg << std::endl;
}

Message::Message() :
    logLevel(LogLevel::INFO) {
  message.str("");
}

Message::Message(Message &&msg){
  message << msg.message.str();
  logLevel = msg.logLevel;
}

Message& Message::operator<<(LogLevel level){
  message << level;
  logLevel = level;
  return *this;
}

std::string logging::getTime(){
  std::string str;
  try{
  std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
  time_t t = std::chrono::system_clock::to_time_t(tp);
  str.append(ctime(&t));
  str.pop_back();
  str.append(" ");
  } catch (std::exception &e){
    std::cerr << "ERROR: Weird things happend: " << e.what() << std::endl;
    return std::string("NO_TIME ");
  }
  return str;
}

