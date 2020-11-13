/*
 * Logging.h
 *
 *  Created on: Jan 22, 2018
 *      Author: zenker
 */

#ifndef INCLUDE_LOGGING_H_
#define INCLUDE_LOGGING_H_
#include <string>
#include <sstream>
#include <ostream>
#include <vector>

namespace logging{

enum LogLevel { DEBUG, INFO, WARNING, ERROR, SILENT };
std::string getTime();

std::ostream& operator<<(std::ostream& os, const logging::LogLevel &level);

#ifdef ENABLE_LOGGING
struct Message {
  std::stringstream message;
  LogLevel logLevel;
  Message(const std::string &msg, const LogLevel &level);
  Message();
  Message(Message &&msg);

  template <class Data>
  Message& operator<<(Data val){
    message << val;
    return *this;
  }

  Message& operator<<(LogLevel level);
};

/**
 * Get data from an istream (e.g. filebuf or stringstream) and put a certain number of lines to the ostream (e.g. cout).
 * \param data The input data stream to be formated (select the requested number of lines + cut long lines).
 * \param os The ostream used to put the selected messages to.
 * \param numberOfLines The number of lines from the input \c data to be put to the output \c os.
 */
void formatLogTail(std::istream  &data, std::ostream &os, size_t numberOfLines = 10);

std::vector<Message> stripMessages(std::stringstream &msg, const size_t maxCharacters = 256);

#endif
}

#endif /* INCLUDE_LOGGING_H_ */
