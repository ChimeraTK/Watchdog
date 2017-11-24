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
#include <proc/readproc.h>

#undef GENERATE_XML
#include <ChimeraTK/ApplicationCore/ScalarAccessor.h>

/**
 * This class is used to read /proc information via libproc.
 * The libproc library is not thread safe. Therefore only one
 * instance of the ProcReader should be present!
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
  std::shared_ptr<proc_t> getInfo(const size_t &PID);
  /**
   * Read the number of processes that belong to the same process group id (PGID).
   * \param PGID The process group id used to look for processes
   */
  size_t getNChilds(const size_t &PGID);
};

/**
 * Handler used to start, stop processes.
 * Use the Handler only to start a single process.
 * When a process is started fork + execv is used. The parent
 * process does not know the child process and its pid. Therefore,
 * the child creates a file that contains the PID. The parent
 * process reads the PID from that file.
 * By default the file is created in the directory where the main
 * thread is executed and the file is deleted if the started process
 * is killed using killProcess. Else the file is not deleted.
 * If e.g. the watchdog is killed, the process that was started before
 * by its ProcessHandler can not be terminated and could still be running.
 * In that case the pid file indicates that. If the watchdog constructs a
 * new ProcessHandler it will realize during construction that the PID is still
 * present and it will check if the process is still running.
 * If the ProcessHandler is destroyed a check is performed
 * if a process with the stored pid is still running.
 * If this is the case it is killed. First the process is killed using SIGINT
 * and if that fails SIGKILL is used.
 */
struct ProcessHandler {
private:

  /**
   * Read the PID from a temporary file that is created by the child process.
   */
  bool readTempPID(int &PID);

  /**
   * If a process was started using the ProcessHandler this method can be used
   * to kill it in case it is still running.
   * If this is the case it is killed. First the process is killed using signum
   * and if that fails SIGKILL is used.
   * If that process stared other processes also they are
   * killed since
   * \code
   * kill -pid
   * \endcode
   * is called.
   * \throw runtime_error In case the process could not be killed (even using SIGKILL)
   */
  void cleanup();

  /**
   * Try do move to the directory where to store the PID file.
   * \throw runtime_error In case the directory is not writable
   */
  bool changeDirectory();

  /**
   * Check if a PID exists and read the PID.
   * Check if a process is running, that has the PID from the PID file.
   * \param PID In case there is a running process its PID is stored
   * \return True in case a running process was found.
   */
  bool checkRunningProcess(int &PID);

  bool isPIDFolderWritable();

  int pid; ///< The pid of the last process that was started.
  std::string pidFile; ///< Name of the temporary file that holds the child PID
  std::string pidDirectory; ///< Path where to create PID files.
  bool        deletePIDFile; ///< If true the PID file is deleted after reading the PID.
  int signum;///< Signal used to stop a process

public:
  /**
   * Constructor.
   * It is checked if a process is already running. This is done by testing if the
   * PID file already exists and a process with the PID read from the PID file is found.
   * \param path This is the path where to store the PID file. If an empty string is
   * entered the directory where the main thread is started is used.
   * \param PIDFileName the name of the PID file -> will result in: PIDFileName.PID
   * \param deletePIDFile If true the PID file deleted directly after reading the PID.
   * This avoids overwriting the PID in case a second ProcessHandler starts a process
   * with the same PID file settings. But you can not check for a running process if the
   * ProcessHandler is not terminated correctly and started again.
   * \param PID The PID is set in case a running process was found. Else it is set to -1.
   */
  ProcessHandler(const std::string &path, const std::string &PIDFileName,
      const bool deletePIDFile, int &PID);

  /**
     * Constructor.
     * \param path This is the path where to store the PID file. If an empty string is
     * entered the directory where the main thread is started is used.
     * \param PIDFileName the name of the PID file -> will result in: PIDFileName.PID
     * \param deletePIDFile If true the PID file deleted directly after reading the PID.
     * This avoids overwriting the PID in case a second ProcessHandler starts a process
     * with the same PID file settings. But you can not check for a running process if the
     * ProcessHandler is not terminated correctly and started again.
     */
  ProcessHandler(const std::string &path, const std::string &PIDFileName,
      const bool deletePIDFile = false);
  ~ProcessHandler();

  /**
   * Start a process.
   * The process is started using fork + execv. Usually the copied process will
   * have the same process group id (GPID) as the process how calls this function.
   * But here we set the GPID of the new process to the process id of the new
   * process. Thus if the child process itself starts new processes they all will
   * have the same GPID. This allows to kill all of them at the same time calling
   * \code
   * kill -PID
   * \endcode
   * where PID is the PID of the child process.
   * The child process creates a file that is called "PIDFileName.PID". Depending
   * on the set pidDirectory it can happen that more than one process is
   * started at the same time and the file would be overwritten before the PID
   * is read from the temporary file.
   * \remark In that case use a unique string PIDFileName used for the file name!
   *
   * \param path Path where to find the cmd you are about to call.
   * \param cmd CMD including command line options.
   * \return PID of the created process
   * \remark The command will not be executated in the working directory of the
   * calling process but in the given path!
   */
  size_t startProcess(const std::string &path, const std::string &cmd);

  /**
   * \param sig Signal used to kill the process (e.g. SIGINT = 2, SIGKILL = 9)
   */
  void setSigNum(int sig){signum = sig;}

};

/**
 * \return New string where spaces are replaced by underscores.
 */
std::string space2underscore(std::string text);

/**
 * Split a string in tokens. Considered delimiters are tabs and spaces.
 * If there is a sequence of delimiters they are treated as one delimiter.
 * \return Tokens of the split string.
 */
std::vector<std::string> split_arguments(const std::string &arguments);

/**
 * Class that reads static system information from the system by reading
 * \c /proc/cpuinfo and \c /sys/devices/system/cpu/present.
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
