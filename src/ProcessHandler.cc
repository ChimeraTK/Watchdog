/*
 * ProcessHandler.cc
 *
 *  Created on: Dec 1, 2017
 *      Author: zenker
 */

#include "ProcessHandler.h"
#include <signal.h>
#include "sys_stat.h"
#include <sstream>
#include <iostream>
#include <fstream>

//includes for set_all_close_on_exec
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <sys/resource.h>

ProcessHandler::ProcessHandler(const std::string &path, const std::string &PIDFileName, const bool deletePIDFile, int &PID):
 pid(-1), pidFile(PIDFileName + ".PID"), pidDirectory(path), deletePIDFile(deletePIDFile), signum(SIGINT){
  PID = -1;
  if(checkRunningProcess(PID))
    pid = PID;
};

ProcessHandler::ProcessHandler(const std::string &path, const std::string &PIDFileName, const bool deletePIDFile):
 pid(-1), pidFile(PIDFileName + ".PID"), pidDirectory(path), deletePIDFile(deletePIDFile), signum(SIGINT){
};


ProcessHandler::~ProcessHandler() {
  cleanup();
}

bool ProcessHandler::changeDirectory(){
  std::string pidDir = pidDirectory;
  if(pidDirectory.back() != '/')
    pidDir.append(std::string("/").c_str());
  if(!isPIDFolderWritable()){
    throw std::runtime_error("Folder where to store the PID file is not writable!");
  }
  if(chdir(pidDir.c_str())) {
    std::stringstream ss;
    ss << "Failed to change to pid file directory: " << pidDir;
    std::cout << ss.str() << std::endl;
    return false;
  } else {
    return true;
  }
}

void ProcessHandler::cleanup() {
  if(pid > 0 && proc_util::isProcessRunning(pid)){
#ifdef DEBUG
    std::cout
        << "Going to kill (SIGINT) process in the destructor of ProcessHandler for process: "
        << pid << std::endl;
#endif
    kill(-pid, signum);
    usleep(200000);
    if(proc_util::isProcessRunning(pid)) {
#ifdef DEBUG
      std::cout
          << "Going to kill (SIGKILL) process in the destructor of ProcessHandler for process: "
          << pid << std::endl;
#endif
      kill(-pid, SIGKILL);
      usleep(200000);
      if(proc_util::isProcessRunning(pid)) {
        std::stringstream ss;
        ss << "When cleaning up the ProcessHandler the process ";
        ss << pid;
        ss << " could not be stopped. Even using signal SIGKILL!";
        throw std::runtime_error(ss.str());
      }
    }
  }
  if(!deletePIDFile)
    remove(pidFile.c_str());
}

bool ProcessHandler::checkRunningProcess(int &PID){
  if(pidDirectory.empty()){
    return readTempPID(PID);
  } else if (changeDirectory()){
    return readTempPID(PID);
  }
  return false;
}

bool ProcessHandler::isPIDFolderWritable() {
    if(access(pidDirectory.c_str(), W_OK) == 0) {
        return true;
    } else {
        return false;
    }
}


size_t ProcessHandler::startProcess(const std::string &path, const std::string &cmd) {
  if(path.empty() || cmd.empty()) {
    throw std::runtime_error(
        "Path or command not set before starting a process!");
  }
  // process could be stopped even if it was present when the ProcessHandler was constructed.
  if(pid > 0 && proc_util::isProcessRunning(pid)) {
    std::cerr
        << "There is still a process running that was not cleaned up! I will do a cleanup now."
        << std::endl;
    cleanup();
  }

  if(!pidDirectory.empty() && !changeDirectory()){
    std::stringstream ss;
    ss << "Failed to change to the directory where to create the PID file: " << pidDirectory;
    throw std::runtime_error(ss.str());
  }

  pid_t p = fork();
  if(p == 0) {
    // Don't throw in the child since the parent will not catch it
    pid_t child = (int) getpid();
    if(setpgid(0, child)) {
      std::cerr << "Failed to reset GPID." << std::endl;
    }
#ifdef DEBUG
    std::cout << "Child running and its PID is: " << child << std::endl;
#endif
    std::ofstream file;
    file.open(pidFile);
    if(!file.is_open()) {
      file.close();
      std::cerr << "Failed to create PID file: " << pidFile << std::endl;
      _exit(0);
    } else {
      file << child;
      file.close();
    }
    std::string path_copy = path;
    if(path.back() != '/')
      path_copy.append(std::string("/").c_str());
    if(chdir(path.c_str())) {
      std::cerr << "Failed to change to directory: " << path << std::endl;
      _exit(0);
    }
    auto args = split_arguments(cmd);
    char * exec_args[1024];
    int arg_count = 0;
#ifdef DEBUG
    std::cout << "Going to call: execv(\"" << (path_copy + args.at(0)).c_str();
#endif
    for(size_t x = 0; x < args.size(); x++) {
      exec_args[arg_count++] = strdup(args[x].c_str());
      std::cout << "\", \"" << exec_args[x];
    }
    exec_args[arg_count++] = 0; // tell it when to stop!
#ifdef DEBUG
    std::cout << "\", \"NULL\")" << std::endl;
#endif
    setAllFHCloseOnExec();
    execv((path_copy + args.at(0)).c_str(), exec_args);
    _exit(0);
  } else {
    // Ignore the signal SIGCHLD by the parent since after killing the child it will hang in defunct status since the kernel
    // thinks that the parent expects a status
    signal(SIGCHLD, SIG_IGN);
    sleep(1);
    if(readTempPID(pid)) {
#ifdef DEBUG
      std::cout << "PID was read:" << pid << std::endl;
#endif
      if(deletePIDFile)
        remove(pidFile.c_str());
    } else {
      throw std::runtime_error("Process is not started!");
    }
  }

  return pid;
}

bool ProcessHandler::readTempPID(int &PID) {
  std::ifstream testFile;
  testFile.open(pidFile);
  if(testFile.is_open()) {
    std::string line;
    getline(testFile, line);
    PID = atoi(line.c_str());
    testFile.close();
    return true;
  } else {
    testFile.close();
    return false;
  }
}

void ProcessHandler::setAllFHCloseOnExec()
{
  struct rlimit  rlim;
  long           max;
  int            fd;

  /* Resource limit? */
#if defined(RLIMIT_NOFILE)
  if (getrlimit(RLIMIT_NOFILE, &rlim) != 0)
    rlim.rlim_max = 0;
#elif defined(RLIMIT_OFILE)
  if (getrlimit(RLIMIT_OFILE, &rlim) != 0)
    rlim.rlim_max = 0;
#else
  /* POSIX: 8 message queues, 20 files, 8 streams */
  rlim.rlim_max = 36;
#endif

  /* Configured limit? */
#if defined(_SC_OPEN_MAX)
  max = sysconf(_SC_OPEN_MAX);
#else
  max = 36L;
#endif

  /* Use the bigger of the two. */
  if ((int)max > (int)rlim.rlim_max)
    fd = max;
  else
    fd = rlim.rlim_max;

  while (fd-->0)
    if (fd != STDIN_FILENO  &&
        fd != STDOUT_FILENO &&
        fd != STDERR_FILENO)
      fcntl(fd, F_SETFD, FD_CLOEXEC);
}

