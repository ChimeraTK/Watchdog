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
#include <fcntl.h> // open
#include <unistd.h>
#include <sys/wait.h>

//includes for set_all_close_on_exec
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <sys/resource.h>

void handle_sigchld(int /*sig*/) {
  int saved_errno = errno;
  while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {}
  errno = saved_errno;
}

void ProcessHandler::setupHandler(){
// see http://www.microhowto.info/howto/reap_zombie_processes_using_a_sigchld_handler.html
  struct sigaction sa;
  sa.sa_handler = &handle_sigchld;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP | SA_NOCLDWAIT;
  if (sigaction(SIGCHLD, &sa, 0) == -1) {
    perror(0);
    exit(1);
  }
}

ProcessHandler::ProcessHandler(const std::string &_PIDFileName, const bool _deletePIDFile, int &_PID, std::ostream &_stream, const std::string &_name):
 pid(-1), pidFile("/tmp/" + _PIDFileName + ".PID"), deletePIDFile(_deletePIDFile), signum(SIGINT),
 os(_stream), log(logging::LogLevel::DEBUG), name(_name + "/ProcessHandler: "), connected(true), killTimeout(1){
  _PID = -1;
  if(readTempPID(_PID)){
    if(proc_util::isProcessRunning(_PID))
      pid = _PID;
    else
      _PID = -1;
  }
};

ProcessHandler::ProcessHandler(const std::string &_PIDFileName, const bool _deletePIDFile, std::ostream &_stream, const std::string &_name):
 pid(-1), pidFile("/tmp/" + _PIDFileName + ".PID"), deletePIDFile(_deletePIDFile), signum(SIGINT),
 os(_stream), log(logging::LogLevel::DEBUG), name(_name + "/ProcessHandler: "), connected(true), killTimeout(1) {
};


ProcessHandler::~ProcessHandler() {
  if(connected)
    cleanup();
}

void ProcessHandler::cleanup() {
  if(pid > 0 && proc_util::isProcessRunning(pid)){
    if(log == logging::LogLevel::DEBUG){
      os  << logging::LogLevel::DEBUG << name << logging::getTime() << "Going to kill (" << signum << ") process in the destructor of ProcessHandler for process: "
          << pid << std::endl;
    }
    kill(-pid, signum);
    // allow 1s for terminating the process, else it will be killed
    // \ToDo: Verify that 1s is ok...
    bool running = true;
    if(killTimeout < 1)
      killTimeout = 1;
    if(log == logging::LogLevel::DEBUG){
      os << logging::LogLevel::DEBUG << name << logging::getTime() << "Waiting for the process to exit (no longer than " << killTimeout << "s)." << std::endl;
    }
    for(size_t i = 0; i < killTimeout; i++){
      sleep(1);
      if(!proc_util::isProcessRunning(pid)){
        running = false;
        break;
      }
    }

    if(running) {
      if(log == logging::LogLevel::DEBUG){
        os << logging::LogLevel::DEBUG << name << logging::getTime() << "Going to kill (SIGKILL) process in the destructor of ProcessHandler for process: "
           << pid << std::endl;
      }
      kill(-pid, SIGKILL);
      usleep(200000);
      if(proc_util::isProcessRunning(pid)) {
        os << logging::LogLevel::ERROR << name << logging::getTime() << "When cleaning up the ProcessHandler the process " << pid <<
            " could not be stopped. Even using signal SIGKILL!" << std::endl;
      } else {
        os << logging::LogLevel::INFO << name << logging::getTime() << "Ok process was terminated."  << std::endl;
      }
    } else {
      if(log == logging::LogLevel::DEBUG){
        os << logging::LogLevel::DEBUG << name << logging::getTime() << "Process exited normally." << std::endl;
      }
    }
  } else if(pid > 0 && log == logging::LogLevel::DEBUG) {
    os  << logging::LogLevel::DEBUG << name << logging::getTime() << "Destructor called for the handler. Seems like process with PID: " << pid << " died (no attemp to kill it)." << std::endl;
  }
  if(!deletePIDFile)
    remove(pidFile.c_str());
}

bool ProcessHandler::isPIDFolderWritable() {
    if(access("/tmp", W_OK) == 0) {
        return true;
    } else {
        return false;
    }
}

size_t ProcessHandler::startProcess(const std::string &path, const std::string &cmd, const std::string &logfile,
    const std::string &environment, const bool &overwriteENV) {
  if(path.empty() || cmd.empty()) {
    throw std::runtime_error(
        "Path or command not set before starting a process!");
  }
  // process could be stopped even if it was present when the ProcessHandler was constructed.
  if(pid > 0 && proc_util::isProcessRunning(pid)) {
    if(log <= logging::LogLevel::ERROR){
        os << logging::LogLevel::ERROR << name << logging::getTime() << "There is still a process running that was not cleaned up! I will do a cleanup now."
        << std::endl;
    }
    cleanup();
  }

  if(!isPIDFolderWritable()){
    std::stringstream ss;
    ss << "Failed to change to the directory where to create the PID file (/tmp).";
    throw std::runtime_error(ss.str());
  }
  // empty streams before forking to have empty copies in the child.
  std::cout.clear();
  std::cerr.clear();
  std::cout.flush();
  std::cerr.flush();

  pid_t p = fork();
  if(p == 0) {
    if(logfile.empty()){
      if(log <= logging::LogLevel::WARNING)
        std::cout << logging::LogLevel::WARNING << name << logging::getTime() << "No log file name is set. Process output is dumped to stout/stderr." << std::endl;
    } else {
      // open the logfile
      int fd = open(logfile.c_str(), O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
      if(fd == -1 && log <= logging::LogLevel::ERROR){
        std::cerr << logging::LogLevel::ERROR << name << logging::getTime() << "Failed to open log file. No logfile will be written." << std::endl;
      } else {
        dup2(fd, 1);   // make stdout go to file
        dup2(fd, 2);   // make stderr go to file
        close(fd);
      }
    }
    std::cout << logging::LogLevel::INFO << name  << logging::getTime() << "Going to start a new process." << std::endl;
    // Don't throw in the child since the parent will not catch it
    pid_t child = (int) getpid();
    if(setpgid(0, child) && log <= logging::LogLevel::ERROR) {
      std::cerr << logging::LogLevel::ERROR << name << logging::getTime() << "Failed to reset GPID." << std::endl;
    }
    if(log == logging::LogLevel::DEBUG){
      std::cout << logging::LogLevel::DEBUG << name << logging::getTime() << "Child running and its PID is: " << child << std::endl;
    }
    std::ofstream file;
    file.open(pidFile);
    if(!file.is_open()) {
      file.close();
      if(log <= logging::LogLevel::ERROR){
        std::cerr << logging::LogLevel::ERROR << name << logging::getTime() << "Failed to create PID file: " << pidFile << std::endl;
      }
      _exit(0);
    } else {
      file << child;
      file.close();
    }
    std::string path_copy = path;
    if(path.back() != '/')
      path_copy.append(std::string("/").c_str());
    if(chdir(path.c_str())) {
      if(log <= logging::LogLevel::ERROR){
        std::cerr << logging::LogLevel::ERROR << name << logging::getTime() << "Failed to change to directory: " << path << std::endl;
      }
      _exit(0);
    }

    // prepare arguments
    auto args = split_arguments(cmd);
    char * exec_args[args.size()];
    int arg_count = 0;
    if(log == logging::LogLevel::DEBUG){
      std::cout << logging::LogLevel::DEBUG << name << logging::getTime() << "Going to call: execve with command:" << (path_copy + args.at(0)).c_str() << std::endl;
      std::cout << logging::LogLevel::DEBUG << name << logging::getTime() << "Adding arguments: ";
    }

    for(size_t x = 0; x < args.size(); x++) {
      exec_args[arg_count++] = strdup(args[x].c_str());
      if(log == logging::LogLevel::DEBUG)
        std::cout << exec_args[x] << ", ";
    }
    exec_args[arg_count++] = 0; // tell it when to stop!
    if(log == logging::LogLevel::DEBUG)
      std::cout << "NULL" << std::endl;


    // prepare environment
    auto env_args = split_arguments(environment, ",");
    if(log == logging::LogLevel::DEBUG)
          std::cout << logging::LogLevel::DEBUG << name << logging::getTime() << "Adding " << env_args.size() << " environment variables." << std::endl;
    for(auto &env_arg : env_args) {
      std::size_t sep = env_arg.find_first_of("=");
      if(sep != std::string::npos){
        setenv(env_arg.substr(0,sep).c_str(),env_arg.substr(sep+1,env_arg.length()).c_str(),overwriteENV);
        if(log == logging::LogLevel::DEBUG)
          std::cout << logging::LogLevel::DEBUG << name << logging::getTime() <<
            "Setting environment variable " << env_arg.substr(0,sep) << ": " << env_arg.substr(sep+1,env_arg.length()) << std::endl;
      } else if (log == logging::LogLevel::ERROR) {
        std::cerr << logging::LogLevel::ERROR << name << logging::getTime() <<
            "Failed to interpret environment string: " << env_arg << std::endl;
      }
    }
    //environ is a variable declared in unistd.h
    extern char** environ;

		// close file handles when calling execv -> release the OPC UA port
    setAllFHCloseOnExec();
    execve((path_copy + args.at(0)).c_str(), exec_args, environ);
    _exit(0);
  } else {
    sleep(1);
    if(readTempPID(pid)) {
      if(log == logging::LogLevel::DEBUG)
        os << logging::LogLevel::DEBUG << name << logging::getTime() << "PID was read:" << pid << std::endl;
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
  // POSIX: 8 message queues, 20 files, 8 streams
  rlim.rlim_max = 36;
#endif

  // Configured limit? 
#if defined(_SC_OPEN_MAX)
  max = sysconf(_SC_OPEN_MAX);
#else
  max = 36L;
#endif

  // Use the bigger of the two.
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

void ProcessHandler::Disconnect(){
  connected = false;
  if(log == logging::LogLevel::DEBUG && pid > 0)
    os << logging::LogLevel::DEBUG << name << logging::getTime() << "Process is disconnected. It is no longer controlled by the ProcessHandler!"
        " You have to take care of it on your own. PID is: " << pid << std::endl;
}

