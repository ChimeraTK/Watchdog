// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

/*
 * sys_stst.h
 *
 *  Created on: Sep 6, 2017
 *      Author: Klaus Zenker (HZDR)
 */

#ifdef WITH_PROCPS
#  include <proc/readproc.h>
#else
#  include <libproc2/pids.h>
#endif
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string.h>
#include <vector>

/**
 * \brief This namespace contains functions is used to read /proc information via libproc.
 *
 * The libproc library is not thread safe. Therefore a mutex is used here.
 */
namespace proc_util {
  extern std::mutex proc_mutex;

#ifdef WITH_PROCPS
  /**
   * Use system folder \c /proc to search for a process with the given process ID.
   * If a directory with the given PID is found the process is running.
   * \param PID Process ID to look for
   * \return True if the process is running and registered in the \c /proc folder
   */
  bool isProcessRunning(const int& PID);

  /**
   * Read procps information of a certain process.
   * \param PID pid of the process to be considered.
   * \throws std::runtime_error In case no process with the given PID exists.
   */
  std::shared_ptr<proc_t> getInfo(const size_t& PID);

  /**
   * Read the number of processes that belong to the same process group id (PGID).
   * \param PGID The process group id used to look for processes
   * \param os Stream used to print messages.
   */
  size_t getNChilds(const size_t& PGID, std::ostream& os = std::cout);
#else
  /**
   * Use system folder \c /proc to search for a process with the given process ID.
   * If a directory with the given PID is found the process is running.
   * \param PID Process ID to look for
   * \param infoptr Expected is a prepared info pointer that has two entries: PIDS_ID_PID, PIDS_ID_PGRP
   * \return True if the process is running and registered in the \c /proc folder
   */
  bool isProcessRunning(const int& PID, pids_info* infoptr);

  /**
   * Read the number of processes that belong to the same process group id (PGID).
   * \param PGID The process group id used to look for processes
   * \param infoptr Expected is a prepared info pointer that has two entries: PIDS_ID_PID, PIDS_ID_PGRP
   * \param os Stream used to print messages.
   */
  size_t getNChilds(const size_t& PGID, pids_info* infoptr, std::ostream& os = std::cout);
#endif

} // namespace proc_util

/**
 * \return New string where spaces are replaced by underscores.
 */
std::string space2underscore(std::string text);

/**
 * Split a string in tokens. Considered delimiters are tabs and spaces.
 * If there is a sequence of delimiters they are treated as one delimiter.
 * \param arguments The string to be split
 * \param token The token used to split the string
 * \return Tokens of the split string.
 */
std::vector<std::string> split_arguments(const std::string& arguments, const std::string& token = "\t ");

/**
 * \brief Class that reads static system information from the system by reading
 * \c /proc/cpuinfo and \c /sys/devices/system/cpu/present.
 *
 * \throws runtime_error Exception is thrown in case one of the /proc/ files could not be read.
 */
class SysInfo {
 private:
  /**
   * System data are stored in this map. The keys are equivalent to the keys found
   * in \c /proc/cpuinfo.
   */
  std::map<std::string, std::string> sysData;

  unsigned int CPUcount; ///< Number of cpu cores.

  /**
   * Parse cpuinfo file.
   *
   * If the keyward processor is found the number of cpus is increased by 1.
   * Else only unique key strings are considered. So if e.g. the processors have
   * different parameters that is ignored and only parameters of the first
   * processor are considered.
   *
   * Only lines that contain a delimiter ":" are considered. All other lines
   * are skipped.
   */
  void parseInfoFile(const std::string& file);

 public:
  /** Iterator pointing to the beginning of the system data map */
  std::map<std::string, std::string>::iterator ibegin() { return sysData.begin(); }
  /** Iterator pointing to the end of the system data map */
  std::map<std::string, std::string>::iterator iend() { return sysData.end(); }

  /**
   * Read information about the system.
   * \param cpuinfoFile Name of cpu info file created by procps. Only change
   *                    this for test purposes.
   * \throws runtime_error Exception is thrown in case one of the /proc/ files could not be read.
   */
  SysInfo(const std::string& cpuinfoFile = "/proc/cpuinfo");

  /**
   * Get a system parameter stored in \c /proc/cpuinfo
   * \remark Not all parameter found in  \c /proc/cpuinfo are available.
   * \param pattern The key used in \c /proc/cpuinfo and the internal map sysData.
   */
  std::string getInfo(const std::string& pattern) { return sysData.at(pattern); }

  /**
   * \return Number cpu cores of the system
   */
  unsigned int getNCpu() { return CPUcount; }
};
