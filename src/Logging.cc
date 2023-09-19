// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

/*
 * Logging.cc
 *
 *  Created on: Jan 22, 2018
 *      Author: Klaus Zenker (HZDR)
 */
#include "Logging.h"

#include "boost/date_time/posix_time/posix_time.hpp"

#include <algorithm>
#include <iostream>

namespace logging {

  std::ostream& operator<<(std::ostream& os, const logging::LogLevel& level) {
    switch(level) {
      case logging::LogLevel::DEBUG:
        os << "DEBUG::";
        break;
      case logging::LogLevel::INFO:
        os << "INFO::";
        break;
      case logging::LogLevel::WARNING:
        os << "WARNING::";
        break;
      case logging::LogLevel::ERROR:
        os << "ERROR::";
        break;
      default:
        break;
    }
    return os;
  }

  void formatLogTail(std::istream& data, std::ostream& os, size_t numberOfLines) {
    // move to the end
    data.seekg(-1, std::ios_base::end);
    size_t line = 0;
    // find position to start reading lines
    while(line < numberOfLines) {
      data.seekg(-2, std::ios_base::cur);
      char ch;
      data.get(ch);

      if((int)data.tellg() <= 1) { // If the data was at or before the 0th byte
        data.seekg(0);             // The first line is the last line
        break;
      }
      else if(ch == '\n') {
        line++;
      }
    }
    std::string strline;
    // read lines
    for(size_t i = 0; i < numberOfLines; i++) {
      if(!getline(data, strline)) break;
      os << strline << std::endl;
    }
  }

  std::vector<Message> stripMessages(std::stringstream& msg, const size_t maxCharacters) {
    std::vector<Message> messages;
    char* s = new char[maxCharacters];
    while(msg.good()) {
      Message singleMsg;
      msg.getline(s, maxCharacters);
      if(std::string(s).empty()) continue;
      if(msg.eof()) {
        break;
      }
      if(msg.fail()) {
        singleMsg.message << s << "|\n The above line was cut by logger!" << std::endl;
      }
      else {
        singleMsg.message << s;
      }

      std::vector<LogLevel> levelNames = {LogLevel::INFO, LogLevel::WARNING, LogLevel::ERROR, LogLevel::DEBUG};
      for(auto it = levelNames.begin(); it != levelNames.end(); ++it) {
        std::stringstream ss;
        ss << (*it);
        size_t pos = singleMsg.message.str().find(ss.str());
        if(pos != std::string::npos) {
          singleMsg.logLevel = (*it);
          singleMsg.message.str(
              singleMsg.message.str().substr(pos + ss.str().length() - 1, singleMsg.message.str().length()));
          break;
        }
      }

      messages.push_back(std::move(singleMsg));
    }
    delete[] s;
    return messages;
  }

  Message::Message(const std::string& msg, const LogLevel& level) : logLevel(level) {
    message.str("");
    message << level << msg << std::endl;
  }

  Message::Message() : logLevel(LogLevel::INFO) {
    message.str("");
  }

  Message::Message(Message&& msg) {
    message << msg.message.str();
    logLevel = msg.logLevel;
  }

  Message& Message::operator<<(LogLevel level) {
    message << level;
    logLevel = level;
    return *this;
  }

  std::string getTime() {
    return boost::posix_time::to_simple_string(boost::posix_time::microsec_clock::local_time()) + " ";
  }
} // namespace logging
