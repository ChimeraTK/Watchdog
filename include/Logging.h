// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

/*
 * Logging.h
 *
 *  Created on: Jan 22, 2018
 *      Author: zenker
 */

#include <ChimeraTK/ApplicationCore/Logging.h>

#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace logging {

  //  enum class Level { DEBUG, INFO, WARNING, ERROR, SILENT };
  std::string getTime();

  std::ostream& operator<<(std::ostream& os, const logging::LogLevel& level);

  struct Message {
    std::stringstream message;
    logging::LogLevel logLevel;
    Message(const std::string& msg, const LogLevel& level);
    Message();
    Message(Message&& msg);

    template<class Data>
    Message& operator<<(Data val) {
      message << val;
      return *this;
    }

    Message& operator<<(logging::LogLevel level);
  };

  /**
   * Get data from an istream (e.g. filebuf or stringstream) and put a certain number of lines to the ostream (e.g. cout).
   * \param data The input data stream to be formated (select the requested number of lines + cut long lines).
   * \param os The ostream used to put the selected messages to.
   * \param numberOfLines The number of lines from the input \c data to be put to the output \c os.
   */
  void formatLogTail(std::istream& data, std::ostream& os, size_t numberOfLines = 10);

  std::vector<Message> stripMessages(std::stringstream& msg, const size_t maxCharacters = 256);
} // namespace logging
