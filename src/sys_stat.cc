// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

/*
 * sys_stat.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: Klaus Zenker (HZDR)
 */

#include "sys_stat.h"
#ifdef WITH_PROCPS
#  include <proc/readproc.h>
#  include <proc/sysinfo.h>
#else
#  include <libproc2/pids.h>
#endif
#include <boost/algorithm/string.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <signal.h>
#include <sstream>
#include <stdlib.h>
#include <string>

namespace proc_util {
  std::mutex proc_mutex;

  bool isProcessRunning(const int& PID) {
#ifdef WITH_PROCPS
    std::lock_guard<std::mutex> lock(proc_mutex);
    pid_t pid = PID;
    PROCTAB* proc = openproc(PROC_FILLSTAT | PROC_PID, &pid, NULL);
    proc_t* proc_info = readproc(proc, NULL);
    // Check in addition if the tid is correct. It happened that the pointer was not NULL but the process was dead.
    if(proc_info == NULL || proc_info->tid != pid) {
      freeproc(proc_info);
      closeproc(proc);
      return false;
    }
    freeproc(proc_info);
    closeproc(proc);
#else
    struct pids_info* infoptr = nullptr;
    struct pids_fetch* stack;
    fatal_proc_unmounted(infoptr, 0);
    if(!infoptr) {
      std::runtime_error("proc_util::Failed to access proc data.");
    }
    enum pids_item Items[] = {PIDS_ID_PID};
    if(procps_pids_new(&infoptr, Items, 1) < 0) return false;
    uint tempPID = PID;
    stack = procps_pids_select(infoptr, &tempPID, 2, PIDS_SELECT_PID);
    if(stack->counts->total < 1 || PIDS_VAL(0, s_int, stack->stacks[0], info) != PID) {
      return false;
    }
    else {
      return true;
    }
#endif
    return true;
  }

  size_t getNChilds(const size_t& PGID, std::ostream& os) {
    size_t nChild = 0;
#ifdef WITH_PROCPS
    std::lock_guard<std::mutex> lock(proc_mutex);
    PROCTAB* proc = openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLSTATUS);
    proc_t* proc_info;

    while((proc_info = readproc(proc, NULL)) != NULL) {
      if(PGID == (unsigned)proc_info->pgrp && PGID != (unsigned)proc_info->tid) {
        os << "Found child for PGID: " << PGID << " with PID: " << proc_info->tid << std::endl;
        nChild++;
      }
      freeproc(proc_info);
    }
    closeproc(proc);
#else
    struct pids_info* infoptr = nullptr;
    struct pids_stack* stack;
    stack = fatal_proc_unmounted(infoptr, 0);
    enum pids_item Items[] = {PIDS_ID_PID, PIDS_ID_PGRP};
    if(procps_pids_new(&infoptr, Items, 2) < 0) {
      std::runtime_error("Failed to identify nChilds.");
    }
    while((stack = procps_pids_get(infoptr, PIDS_FETCH_TASKS_ONLY))) {
      if(PGID == (size_t)PIDS_VAL(1, s_int, stack, info) && PGID != (size_t)PIDS_VAL(0, s_int, stack, info)) {
        os << "Found child for PGID: " << PGID << " with PID: " << PIDS_VAL(0, s_int, stack, info) << std::endl;
        nChild++;
      }
    }
    procps_pids_unref(&infoptr);
#endif
    return nChild;
  }

#ifdef WITH_PROCPS
  std::shared_ptr<proc_t> getInfo(const size_t& PID) {
    std::lock_guard<std::mutex> lock(proc_mutex);
    pid_t pid = PID;
    PROCTAB* proc = openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLSTATUS | PROC_PID, &pid, NULL);
    proc_t* proc_info;
    std::shared_ptr<proc_t> result(nullptr);
    proc_info = readproc(proc, NULL);
    if(proc_info == NULL) {
      freeproc(proc_info);
      closeproc(proc);
      std::stringstream ss;
      ss << "Process " << PID << " not found when trying to read process information.";
      throw std::runtime_error(ss.str());
    }
    result.reset(new proc_t(*proc_info));
    freeproc(proc_info);
    closeproc(proc);
    return result;
  }
#endif
} // namespace proc_util

std::string space2underscore(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](char ch) { return ch == ' ' ? '_' : ch; });
  return text;
}

std::vector<std::string> split_arguments(const std::string& arguments, const std::string& token) {
  std::vector<std::string> strs;
  boost::split(strs, arguments, boost::is_any_of(token.c_str()), boost::token_compress_on);
  return strs;
}

SysInfo::SysInfo(const std::string& cpuinfoFile) : CPUcount(0) {
  parseInfoFile(cpuinfoFile);
}

void SysInfo::parseInfoFile(const std::string& file) {
  std::ifstream procfile(file.c_str());
  if(!procfile.is_open()) throw std::runtime_error(std::string("Failed to open ") + file);
  std::string line;
  while(std::getline(procfile, line)) {
    auto strVec = split_arguments(line, ":");
    // ignore lines without a delimiter
    if(strVec.size() == 2) {
      for(auto& str : strVec) {
        boost::trim(str);                            // removes all leading and trailing white spaces
        boost::trim_if(str, boost::is_any_of("\t")); // removes only tabs
      }
      // count number of processors
      if(boost::to_upper_copy(strVec.at(0)).find("PROCESSOR") != std::string::npos) CPUcount += 1;
      if(sysData.find(strVec.at(0)) == sysData.end()) sysData[strVec.at(0)] = strVec.at(1);
    }
  }
  procfile.close();
}
