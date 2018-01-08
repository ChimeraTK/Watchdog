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

namespace proc_util{
std::mutex proc_mutex;

bool isProcessRunning(const int &PID) {
  std::lock_guard<std::mutex> lock(proc_mutex);
  pid_t pid = PID;
  PROCTAB* proc = openproc(PROC_FILLSTAT | PROC_PID, &pid, NULL);
  proc_t* proc_info = readproc(proc, NULL);
  if(proc_info == NULL){
    freeproc(proc_info);
    closeproc(proc);
    return false;
  }
  freeproc(proc_info);
  closeproc(proc);
  return true;
}

size_t getNChilds(const size_t &PGID) {
  std::lock_guard<std::mutex> lock(proc_mutex);
  PROCTAB* proc = openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLSTATUS);
  proc_t* proc_info;
  size_t nChild = 0;
  while((proc_info = readproc(proc, NULL)) != NULL) {
    if(PGID == (unsigned) proc_info->pgrp) {
#ifdef DEBUG
      std::cout << "Found child for PGID: " << PGID << std::endl;
      std::cout << "Childs for PID: " << proc_info->tid << std::endl;
#endif
      nChild++;
    }
    freeproc(proc_info);
  }
  closeproc(proc);
  return nChild;

}

std::shared_ptr<proc_t> getInfo(const size_t &PID) {
  std::lock_guard<std::mutex> lock(proc_mutex);
  pid_t pid = PID;
  PROCTAB* proc = openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLSTATUS | PROC_PID, &pid, NULL);
  proc_t* proc_info;
  std::shared_ptr<proc_t> result(nullptr);
  proc_info = readproc(proc, NULL);
  if(proc_info == NULL){
    freeproc(proc_info);
    closeproc(proc);
    std::stringstream ss;
    ss << "Process " << PID << " not found when trying to read process information.";
    throw std::runtime_error("Process not found.");
  }
  result.reset(new proc_t(*proc_info));
  freeproc(proc_info);
  closeproc(proc);
  return result;
}
}

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

SysInfo::SysInfo() :
    sysData { std::make_pair("vendor", ""),
        std::make_pair("family", ""),
        std::make_pair("cpu family", ""),
        std::make_pair("model", ""),
        std::make_pair("model name", ""),
        std::make_pair("stepping", ""),
        std::make_pair("cpu MHz", ""),
        std::make_pair("bogomips", "") },
        CPUcount(0), ibegin(sysData.begin()), iend(sysData.end()) {
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
  if(!cpufile.is_open()){
    procfile.open("/proc/cpuinfo");
    // this file might not be present (e.g. when using pbuilder)
    while(std::getline(procfile, line)) {
      if(lookup(line, "processor"))
        CPUcount += 1;
    }
    procfile.close();
  } else {
    std::getline(cpufile, line);
    CPUcount = std::atoi(line.substr(2, 2).c_str()) + 1;
  }
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

