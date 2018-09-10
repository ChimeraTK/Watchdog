/*
 * SystemInfo.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "SystemInfoModule.h"

#include <sstream>
#include <fstream>

#include <proc/readproc.h>
#include <proc/sysinfo.h>

#include "sys_stat.h"

#include "boost/date_time/posix_time/posix_time.hpp"

SystemInfoModule::SystemInfoModule(EntityOwner *owner, const std::string &name,
    const std::string &description, bool eliminateHierarchy,
    const std::unordered_set<std::string> &tags) :
    ctk::ApplicationModule(owner, name, description, eliminateHierarchy, tags) {
  for(auto it = sysInfo.ibegin; it != sysInfo.iend; it++) {
    strInfos[it->first].replace(ctk::ScalarOutput<std::string> { this,
        space2underscore(it->first), "", space2underscore(it->first) });
  }
  cpu_use.reset(new ctk::ArrayOutput<double>{this, "cpuUsage", "%", sysInfo.getNCpu(), "CPU usage for each processor", { "CS", "SYS" }});
//  // add 1 since cpuTotal should be added too
  lastInfo = std::vector<cpu>(sysInfo.getNCpu() + 1);
#ifdef ENABLE_LOGGING
  logging = new std::stringstream();
#else
  logging = &std::cerr;
#endif
}

void SystemInfoModule::mainLoop() {
  for(auto it = sysInfo.ibegin; it != sysInfo.iend; it++) {
    strInfos.at(it->first) = it->second;
    strInfos.at(it->first).write();
  }
  nCPU = sysInfo.getNCpu();
  nCPU.write();
  ticksPerSecond = sysconf(_SC_CLK_TCK);
  ticksPerSecond.write();

  if(lastInfo.size() != (unsigned) (nCPU + 1)) {
    (*logging) << getName() << "Size of lastInfo: " << lastInfo.size()
        << "\t nCPU: " << nCPU << std::endl;
#ifdef ENABLE_LOGGING
    sendMessage(logging::LogLevel::ERROR);
#endif
    throw std::runtime_error(
        "Vector size mismatch in SystemInfoModule::mainLoop.");
  }

  std::ifstream file("/proc/stat");
  if(!file.is_open()) {
    (*logging) << getTime() << "Failed to open system file /proc/stat"
        << std::endl;
#ifdef ENABLE_LOGGING
    sendMessage(logging::LogLevel::ERROR);
#endif
    file.close();
    return;
  }

  readCPUInfo(lastInfo);

  // read the system start time (uncertainty in the order of seconds...)
  double uptime_secs;
  double idle_secs;
  uptime(&uptime_secs, &idle_secs);
  auto now = boost::posix_time::second_clock::local_time();
  startTime = boost::posix_time::to_time_t(now) - std::stoi(std::to_string(uptime_secs));
  startTimeStr = boost::posix_time::to_simple_string(boost::posix_time::from_time_t(startTime));
  startTime.write();
  startTimeStr.write();

  while(true) {
    trigger.read();

    meminfo();
    try {
      maxMem    = std::stoi(std::to_string(kb_main_total));
      freeMem   = std::stoi(std::to_string(kb_main_free));
      cachedMem = std::stoi(std::to_string(kb_main_cached));
      usedMem   = std::stoi(std::to_string(maxMem - freeMem - cachedMem));
      maxSwap   = std::stoi(std::to_string(kb_swap_total));
      freeSwap  = std::stoi(std::to_string(kb_swap_free));
      usedSwap  = std::stoi(std::to_string(maxSwap - freeSwap));
    } catch(std::exception &e) {
      (*logging) << getTime() << "Conversion failed: " << e.what() << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::ERROR);
#endif
    }
    maxMem   .write();
    freeMem  .write();
    cachedMem.write();
    usedMem  .write();
    maxSwap  .write();
    freeSwap .write();
    usedSwap .write();

    // get system uptime
    try {
      uptime(&uptime_secs, &idle_secs);
      //\ToDo: Use stoul once unsigned long is available
      uptime_secTotal = std::stoi(std::to_string(uptime_secs));
      uptime_day     = std::stoi(std::to_string(uptime_secTotal / 86400));
      uptime_hour = std::stoi(
          std::to_string((uptime_secTotal - (uptime_day * 86400)) / 3600));
      uptime_min = std::stoi(std::to_string(
              (uptime_secTotal - (uptime_day * 86400) - (uptime_hour * 3600))
                  / 60));
      uptime_sec = std::stoi(std::to_string(
          uptime_secTotal - (uptime_day * 86400) - (uptime_hour * 3600) - (uptime_min*60)));
    } catch(std::exception &e) {
      (*logging) << getTime() << "Conversion failed: " << e.what() << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::ERROR);
#endif
    }

//    double tmp[3] = { 0., 0., 0. };
    std::vector<double> v_tmp(3);

    loadavg(&v_tmp[0], &v_tmp[1], &v_tmp[2]);
    loadAvg = v_tmp;

    uptime_secTotal.write();
    uptime_sec.write();
    uptime_day.write();
    uptime_hour.write();
    uptime_min.write();
    loadAvg.write();

    calculatePCPU();

#ifdef ENABLE_LOGGING
    (*logging) << getTime() << "System data updated" << std::endl;
    sendMessage(logging::LogLevel::DEBUG);
#endif
  }
}

void SystemInfoModule::calculatePCPU() {
  unsigned long long total;
  double tmp;
  std::vector<double> usage_tmp(nCPU + 1);
  std::vector<std::string> strs;
  std::vector<cpu> vcpu(nCPU + 1);
  readCPUInfo(vcpu);
  auto lastcpu = lastInfo.begin();
  auto newcpu = vcpu.begin();
  for(int iCPU = 0; iCPU < (nCPU + 1); iCPU++) {
    if(newcpu->totalUser < lastcpu->totalUser
        || newcpu->totalUserLow < lastcpu->totalUserLow
        || newcpu->totalSys < lastcpu->totalSys
        || newcpu->totalIdle < lastcpu->totalIdle) {
      //Overflow detection. Just skip this value.
      usage_tmp.at(iCPU) =(-1.0);
    } else {
      total = (newcpu->totalUser - lastcpu->totalUser)
          + (newcpu->totalUserLow - lastcpu->totalUserLow)
          + (newcpu->totalSys - lastcpu->totalSys);
      tmp = total;
      total += (newcpu->totalIdle - lastcpu->totalIdle);
      tmp /= total;
      tmp *= 100.;
      usage_tmp.at(iCPU) = tmp;

    }
    *lastcpu = *newcpu;
    lastcpu++;
    newcpu++;
  }
  cpu_useTotal = usage_tmp.back();
  cpu_useTotal.write();
  usage_tmp.pop_back();
  cpu_use->operator =(usage_tmp);
  cpu_use->write();
}

void SystemInfoModule::readCPUInfo(std::vector<cpu> &vcpu) {
  std::ifstream file("/proc/stat");
  if(!file.is_open()) {
    (*logging) << getTime() << "Failed to open system file /proc/stat"
        << std::endl;
#ifdef ENABLE_LOGGING
    sendMessage(logging::LogLevel::ERROR);
#endif
    file.close();
    return;
  }
  std::string::size_type sz = 0;
  std::string line;
  for(auto it = vcpu.begin(); it != vcpu.end(); it++) {
    if(!std::getline(file, line)) {
      (*logging) << getTime() << "Could not find enough lines in /proc/stat"
          << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::ERROR);
#endif
      file.close();
      return;
    }
    auto strs = split_arguments(line);
    if(strs.size() < 5) {
      (*logging) << getTime() << "Line seems to contain not enough data. Line: "
          << line << std::endl;
    }

    try {
      it->totalUser    = std::stoull(strs.at(1), &sz, 0);
      it->totalUserLow = std::stoull(strs.at(2), &sz, 0);
      it->totalSys     = std::stoull(strs.at(3), &sz, 0);
      it->totalIdle    = std::stoull(strs.at(4), &sz, 0);
    } catch(std::exception &e) {
      (*logging) << getTime() << "Caught an exception: " << e.what()
          << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::ERROR);
#endif
      for(auto s : strs) {
        (*logging) << getTime() << "String token: " << s << std::endl;
#ifdef ENABLE_LOGGING
        sendMessage(logging::LogLevel::ERROR);
#endif
      }
    }
  }
}

void SystemInfoModule::terminate(){
#ifdef ENABLE_LOGGING
  delete logging;
  logging = 0;
#endif
  ApplicationModule::terminate();
}

#ifdef ENABLE_LOGGING
void SystemInfoModule::sendMessage(const logging::LogLevel &level){
  auto logging_ss = dynamic_cast<std::stringstream*>(logging);
  if(logging_ss){
    message = logging_ss->str();
    message.write();
    messageLevel = level;
    messageLevel.write();
    logging_ss->clear();
    logging_ss->str("");
  }
}
#endif

std::string SystemInfoModule::getTime(){
  std::string str{"WATCHDOG_SERVER: "};
  str.append(logging::getTime());
  str.append(this->getName());
  str.append(" -> ");
  return str;
}
