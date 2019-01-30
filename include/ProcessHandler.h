/*
 * ProcessHandler.h
 *
 *  Created on: Dec 1, 2017
 *      Author: zenker
 */

#ifndef INCLUDE_PROCESSHANDLER_H_
#define INCLUDE_PROCESSHANDLER_H_

#undef GENERATE_XML
#include <string>
#include <iostream>
#include "Logging.h"

/**
 * \brief Handler used to start and stop processes.
 *
 * Use the Handler only to start a single process.
 * When a process is started fork + execv is used. The parent
 * process does not know the child process and its pid. Therefore,
 * the child creates a file that contains the PID. The parent
 * process reads the PID from that file.
 * The file is created in the /tmp directory of the system.
 * In case the system reboots /tmp is cleaned and therefor it can not
 * happen, that the PID read from the PID file is given to another process
 * after system boot. The file is also deleted if the started process
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
 *
 * \attention Use the setupHandler() function once to define a handler for the
 * SIGCHLD signal!
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
   * Check if /tmp is writable.
   */
  bool isPIDFolderWritable();

  int pid; ///< The pid of the last process that was started.
  std::string pidFile; ///< Name of the temporary file that holds the child PID
  bool deletePIDFile; ///< If true the PID file is deleted after reading the PID.
  int signum;///< Signal used to stop a process
  std::ostream &os; ///< Stream used to send messages
  logging::LogLevel log;///< The current log level
  const std::string name; ///< Name of this class
  bool connected; ///< If false no cleanup is performed on destructor call
  size_t killTimeout;///< Time in seconds to wait for a process to exit before using SIGKILL
public:
  /**
   * Constructor.
   * It is checked if a process is already running. This is done by testing if the
   * PID file already exists and a process with the PID read from the PID file is found.
   * \param PIDFileName the name of the PID file -> will result in: PIDFileName.PID
   * \param os The ostream used to send status messages and errors.
   * \param deletePIDFile If true the PID file deleted directly after reading the PID.
   * \param name Give a name to the ProcessHandler to distinguish between multiple handlers.
   * It is used in the messages send by the handler.
   * This avoids overwriting the PID in case a second ProcessHandler starts a process
   * with the same PID file settings. But you can not check for a running process if the
   * ProcessHandler is not terminated correctly and started again.
   * \param PID The PID is set in case a running process was found. Else it is set to -1.
   */
  ProcessHandler(const std::string &PIDFileName,
      const bool deletePIDFile, int &PID, std::ostream &os, const std::string &name = "");

  /**
     * Constructor.
     * \param PIDFileName the name of the PID file -> will result in: PIDFileName.PID
     * \param os The ostream used to send status messages and errors.
     * \param deletePIDFile If true the PID file deleted directly after reading the PID.
     * \param name Give a name to the ProcessHandler to distinguish between multiple handlers.
     * It is used in the messages send by the handler.
     * This avoids overwriting the PID in case a second ProcessHandler starts a process
     * with the same PID file settings. But you can not check for a running process if the
     * ProcessHandler is not terminated correctly and started again.
     */
  ProcessHandler(const std::string &PIDFileName,
       const bool deletePIDFile = false, std::ostream &os = std::cout, const std::string &name = "");
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
   * \param logfile Name of the log file. It will be created in the path directory. If
   * that file already exists new output will be appended.
   * \param environment Set environment variables if needed (e.g. ENSHOST=localhost,
   * PATH=/home/bin:/home/test/bin). Separate different environments using a comma.
   * Separate different paths per environment using a colon.
   * \param overwriteENV If true the environment variables are overwritten. Else they
   * are extended.
   * \return PID of the created process
   * \remark The command will not be executated in the working directory of the
   * calling process but in the given path!
   */
  size_t startProcess(const std::string &path, const std::string &cmd, const std::string &logfile,
      const std::string &environment = "", const bool &overwriteENV = false);

  /**
   * \param sig Signal used to kill the process (e.g. SIGINT = 2, SIGKILL = 9)
   */
  void setSigNum(int sig){signum = sig;}

  /**
   * Tell all file handles to be closed when exec is called.
   * Therefore this should be called after forking in the child process brefore calling
   * exec.
   * In principle one can find out the number of open file handles by checking /proc/self/fd
   * Here we simply set it for all file handles that are allowed by process (typically 1024).
   * This is supposed to be easier than finding out the correct nuber of file handles conneced
   * to the process.
   */
  static void setAllFHCloseOnExec();

  /**
   * Set the log level.
   *
   * Depending on the level messages are put to the ostream.
   */
  void SetLogLevel(const logging::LogLevel &level){log = level;}


  /**
   * Setup handler for SIGCHLD signals.
   *
   * This function should be called once before using the ProcessHandler. It is not done automatically,
   * because if multiple handlers are used there is no need to call this function multiple times.
   */
  static void setupHandler();

  /**
   * Set the time used to wait for the process to exit after it is stopped using signum.
   * After that time the process is killed using SIGKILL.
   *
   * \param timeout The time (given in seconds) to wait before killing the process using SIGKILL
   */
  void setKillTimeout(const size_t timeout){killTimeout = timeout;}

  void Disconnect();

};



#endif /* INCLUDE_PROCESSHANDLER_H_ */
