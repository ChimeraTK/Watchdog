/*
 * sys_stst.h
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#ifndef INCLUDE_SYS_STAT_H_
#define INCLUDE_SYS_STAT_H_

#include <map>
#include <string.h>
#include <memory>
#include <mutex>
#include <vector>
#include <proc/readproc.h>

/**
 * \brief This namespace contains functions is used to read /proc information via libproc.
 *
 * The libproc library is not thread safe. Therefore a mutex is used here.
 */
namespace proc_util{
  extern std::mutex proc_mutex;

  /**
   * Use system folder \c /proc to search for a process with the given process ID.
   * If a directory with the given PID is found the process is running.
   * \param PID Process ID to look for
   * \return True if the process is running and registered in the \c /proc folder
   */
  bool isProcessRunning(const int &PID);

  /**
   * Read procps information of a certain process.
   * \param PID pid of the process to be considered.
   * \thows std::runtime_error In case no process with the given PID exists.
   */
  std::shared_ptr<proc_t> getInfo(const size_t &PID);

  /**
   * Read the number of processes that belong to the same process group id (PGID).
   * \param PGID The process group id used to look for processes
   */
  size_t getNChilds(const size_t &PGID);
};

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
std::vector<std::string> split_arguments(const std::string &arguments, const std::string &token = "\t ");

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
   * Check if the given pattern is included in the given line.
   * In case the patter is found the data is filled into the map sysData by
   * calling fill(...).
   */
  bool lookup(const std::string &line, const std::string &pattern);

  /**
   * Fill a value into the map sysData using the pattern string as map key.
   */
  void fill(const std::string &val, const std::string &pattern) {
    if(sysData.count(pattern))
      sysData.at(pattern) = val;
  }

public:
  /** Iterator pointing to the beginning of the system data map */
  const std::map<std::string, std::string>::iterator ibegin;
  /** Iterator pointing to the end of the system data map */
  const std::map<std::string, std::string>::iterator iend;

  /**
   * Read information about the system.
   * \throws runtime_error Exception is thrown in case one of the /proc/ files could not be read.
   */
  SysInfo();

  /**
   * Get a system parameter stored in \c/proc/cpuinfo
   * \remark Not all parameter found in  \c/proc/cpuinfo are available.
   * \param pattern The key used in \c /proc/cpuinfo and the internal map sysData.
   */
  std::string getInfo(const std::string &pattern) {
    return sysData.at(pattern);
  }

  /**
   * \return Number cpu cores of the system
   */
  unsigned int getNCpu() {
    return CPUcount;
  }

};

#endif /* INCLUDE_SYS_STAT_H_ */
