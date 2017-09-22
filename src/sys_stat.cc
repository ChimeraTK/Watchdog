/*
 * sys_stat.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include <fstream>
#include <string>
#include <stdlib.h>

#include <proc/readproc.h>
#include <proc/sysinfo.h>

#include "sys_stat.h"

#include <stdlib.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <signal.h>
#include <iterator>
#include <boost/algorithm/string.hpp>

std::string space2underscore(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](char ch) {
    return ch == ' ' ? '_' : ch;
  });
  return text;
}

std::vector<std::string> split_arguments(const std::string &arguments) {
  std::vector<std::string> strs;
  boost::split(strs, arguments, boost::is_any_of("\t "),
      boost::token_compress_on);
  return strs;
}

ProcessHandler::~ProcessHandler() {
  cleanup();
}

void ProcessHandler::cleanup() {
  if(isProcessRunning(pid)) {
    std::cout
        << "Going to kill (SIGINT) process in the destructor of ProcessHandler for process: "
        << pid << std::endl;
    killProcess(pid, SIGINT);
  } else {
    return;
  }
  usleep(200000);
  if(isProcessRunning(pid)) {
    std::cout
        << "Going to kill (SIGKILL) process in the destructor of ProcessHandler for process: "
        << pid << std::endl;
    killProcess(pid, SIGKILL);
  }
}

size_t ProcessHandler::startProcess(const std::string &path,
    const std::string &cmd, const std::string &append) {
  if(path.empty() || cmd.empty()) {
    throw std::runtime_error(
        "Path or command not set before starting a proccess!");
  }
  if(isProcessRunning(pid)) {
    std::cerr
        << "There is still a process running that was not cleaned up! I will do a cleanup now."
        << std::endl;
    cleanup();
  }
  pidFile = std::string("pid") + append;
  pid_t p = fork();
  if(p == 0) {
    // Don't throw in the child since the parent will not catch it
    pid_t child = (int) getpid();
    if(setpgid(0, child)) {
      throw std::runtime_error("Failed to reset GPID.");
    }
    printf("child running: %d\n", (int) child);
    std::ofstream file;
    file.open(pidFile);
    if(!file.is_open()) {
      file.close();
      std::stringstream ss;
      ss << "Failed to create PID file: " << pidFile;
      std::cout << ss.str() << std::endl;
      _exit(0);
    } else {
      file << child;
      file.close();
    }
    std::string path_copy = path;
    if(path.back() != '/')
      path_copy.append(std::string("/").c_str());
    if(chdir(path.c_str())) {
      std::stringstream ss;
      ss << "Failed to change to directory: " << path;
      std::cout << ss.str() << std::endl;
      _exit(0);
    }
    auto args = split_arguments(cmd);
    char * exec_args[1024];
    int arg_count = 0;
    std::cout << "Going to call: execv(\"" << (path_copy + args.at(0)).c_str();
    for(size_t x = 0; x < args.size(); x++) {
      exec_args[arg_count++] = strdup(args[x].c_str());
      std::cout << "\", \"" << exec_args[x];
    }
    exec_args[arg_count++] = 0; // tell it when to stop!
    std::cout << "\", \"NULL\")" << std::endl;
    execv((path_copy + args.at(0)).c_str(), exec_args);
//		execl((path + std::string("/") + cmd).c_str(), cmd.c_str(), NULL);
    _exit(0);
  } else {
    // Ignore the signal SIGCHLD by the parent since after killing the child it will hang in defunct status since the kernel
    // thinks that the parent expects a status
    signal(SIGCHLD, SIG_IGN);
    sleep(1);
    if(readTempPID(pid)) {
      std::cout << "PID was read:" << pid << std::endl;
      remove("pid");
    } else {
      throw std::runtime_error("Process is not started!");
    }
  }

  return pid;
}

bool ProcessHandler::readTempPID(size_t &PID) {
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

void ProcessHandler::killProcess(const size_t &PID, const int &sig) {
  if(!isProcessRunning(PID)) {
    std::cerr << "When trying to kill a process with PID " << PID
        << " no such process was running!" << std::endl;
    return;
  }
#ifdef DEBUG
  std::cout << "Going to kill process: " << PID << std::endl;
#endif
  kill(-PID, sig);
}

bool ProcessHandler::isProcessRunning(const size_t &PID) {
  PROCTAB* proc = openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLSTATUS);
  proc_t* proc_info;
  while((proc_info = readproc(proc, NULL))) {
    if(PID == (unsigned) proc_info->tid) {
      freeproc(proc_info);
      return true;
    }
  }
  freeproc(proc_info);
  return false;
}

size_t ProcessHandler::getNChilds(const size_t &PGID) {
  PROCTAB* proc = openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLSTATUS);
  proc_t* proc_info;
  size_t nChild = 0;
  while((proc_info = readproc(proc, NULL))) {
    if(PGID == (unsigned) proc_info->pgrp) {
#ifdef DEBUG
      std::cout << "Found child for PGID: " << PGID << std::endl;
      std::cout << "Childs for PID: " << proc_info->tid << std::endl;
#endif
      nChild++;
    }
  }
  freeproc(proc_info);
  return nChild;

}

std::shared_ptr<proc_t> ProcessHandler::getInfo(const size_t &PID) {
  PROCTAB* proc = openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLSTATUS);
  std::shared_ptr<proc_t> result(nullptr);
  while(proc_t* proc_info = readproc(proc, NULL)) {
    std::string tmp(proc_info->cmd);
    if(PID == (unsigned) proc_info->tid) {
      result.reset(new proc_t(*proc_info));
    }
    freeproc(proc_info);
  }
  closeproc(proc);
  return result;
}

SysInfo::SysInfo() :
    sysData { std::make_pair("vendor", ""),
        std::make_pair("family", ""),
        std::make_pair("cpu family", ""),
        std::make_pair("model", ""),
        std::make_pair("model name", ""),
        std::make_pair("stepping", ""),
        std::make_pair("cpu MHz", ""),
        std::make_pair("bogomips", "") },
        ibegin(sysData.begin()), iend(sysData.end()) {
  std::ifstream procfile("/proc/cpuinfo");
  if(!procfile.is_open())
    throw std::runtime_error("Failed to open /proc/cpuinfo");
  std::string line;
  for(auto pat = ibegin; pat != iend; pat++) {
    while(std::getline(procfile, line)) {
      if(lookup(line, pat->first))
        break;
    }
    procfile.seekg(0, std::ios::beg);
  }
  procfile.close();

  std::ifstream cpufile("/sys/devices/system/cpu/present");
  if(!cpufile.is_open())
    throw std::runtime_error("Failed to open /sys/devices/system/cpu/present");
  std::getline(cpufile, line);
  CPUcount = std::atoi(line.substr(2, 2).c_str());
}

bool SysInfo::lookup(const std::string &line, const std::string &pattern) {
  std::size_t found = line.find(pattern);
  if(found == std::string::npos)
    return false;

  std::string pat = line.substr(0, line.find(":"));
  std::string val = line.substr(line.find(":") + 1, line.length() - 1);

  fill(val, pattern);
  return true;
}

