/*
 * Logging.cc
 *
 *  Created on: Jan 22, 2018
 *      Author: zenker
 */
#include "Logging.h"
#include <iostream>
#include <algorithm>

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

void logging::formatLogTail(std::istream  &data, std::ostream &os, size_t numberOfLines){
  // move to the end
  data.seekg(-1,std::ios_base::end);
  size_t line = 0;
  // find position to start reading lines
  while(line < numberOfLines) {
   data.seekg(-2,std::ios_base::cur);
   char ch;
   data.get(ch);

   if((int)data.tellg() <= 1) {             // If the data was at or before the 0th byte
     data.seekg(0);                         // The first line is the last line
     break;
   }
   else if(ch == '\n') {
     line++;
   }
  }
  std::string strline;
  // read lines
  for(size_t i = 0; i < numberOfLines; i++){
    if(!getline(data,strline))
        break;
    os << strline << std::endl;
  }
}

std::vector<Message> logging::stripMessages(std::stringstream &msg, size_t maxCharacters){
  std::vector<Message> l;
  char s[maxCharacters];
  while(msg.good()){
    Message singleMsg;
    msg.getline(s, maxCharacters);
    if(std::string(s).empty())
      continue;
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

#include "boost/date_time/posix_time/posix_time.hpp"
std::string logging::getTime(){
  return boost::posix_time::to_simple_string(boost::posix_time::microsec_clock::local_time()) + " ";
}

