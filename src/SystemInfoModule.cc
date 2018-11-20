/*
 * SystemInfo.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "SystemInfoModule.h"

#include <sstream>
#include <fstream>
#include <vector>
#include <sys/vfs.h>
#include <thread>
#include <chrono>
#include <cerrno>
#include <string>

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
  cpu_use.reset(new ctk::ArrayOutput<double>{this, "cpuUsage", "%", sysInfo.getNCpu(), "CPU usage for each processor", { "CS", "SYS", "History" }});
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
    (*logging) << getTime(this) << "Failed to open system file /proc/stat"
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
      (*logging) << getTime(this) << "meminfo conversion failed: " << e.what() << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::ERROR);
#endif
    }
    memoryUsage = 1.*usedMem/maxMem*100.;
    swapUsage = 1.*usedSwap/maxSwap*100.;
    
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
      (*logging) << getTime(this) << "uptime conversion failed: " << e.what() << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::ERROR);
#endif
    }

    std::vector<double> v_tmp(3);

    loadavg(&v_tmp[0], &v_tmp[1], &v_tmp[2]);
    loadAvg = v_tmp;

    calculatePCPU();

    findTag("SYS").writeAll();
#ifdef ENABLE_LOGGING
    (*logging) << getTime(this) << "System data updated" << std::endl;
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
  for(size_t iCPU = 0; iCPU < (nCPU + 1); iCPU++) {
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
  usage_tmp.pop_back();
  cpu_use->operator =(usage_tmp);
}

void SystemInfoModule::readCPUInfo(std::vector<cpu> &vcpu) {
  std::ifstream file("/proc/stat");
  if(!file.is_open()) {
    (*logging) << getTime(this) << "Failed to open system file /proc/stat"
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
      (*logging) << getTime(this) << "Could not find enough lines in /proc/stat"
          << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::ERROR);
#endif
      file.close();
      return;
    }
    auto strs = split_arguments(line);
    if(strs.size() < 5) {
      (*logging) << getTime(this) << "Line seems to contain not enough data. Line: "
          << line << std::endl;
    }

    try {
      it->totalUser    = std::stoull(strs.at(1), &sz, 0);
      it->totalUserLow = std::stoull(strs.at(2), &sz, 0);
      it->totalSys     = std::stoull(strs.at(3), &sz, 0);
      it->totalIdle    = std::stoull(strs.at(4), &sz, 0);
    } catch(std::exception &e) {
      (*logging) << getTime(this) << "Caught an exception: " << e.what()
          << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::ERROR);
#endif
      for(auto s : strs) {
        (*logging) << getTime(this) << "String token: " << s << std::endl;
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

std::string getTime(ctk::ApplicationModule* mod){
  std::string str{"WATCHDOG_SERVER: "};
  str.append(logging::getTime());
  str.append(mod->getName());
  str.append(" -> ");
  return str;
}

FileSystemModule::FileSystemModule(const std::string &devName, const std::string &mntPoint, EntityOwner *owner, const std::string &name,
       const std::string &description, bool eliminateHierarchy,
       const std::unordered_set<std::string> &tags):
         ctk::ApplicationModule(owner, name, description, eliminateHierarchy, tags){
  tmp[0] = devName;
  tmp[1] = mntPoint;
#ifdef ENABLE_LOGGING
  logging = new std::stringstream();
#else
  logging = &std::cerr;
#endif
}

std::mutex fs_mutex;
bool FileSystemModule::read(){
  std::lock_guard<std::mutex> lock(fs_mutex);
  struct statfs fs;
  if( ( statfs( ((std::string)mountPoint).c_str(), &fs ) ) < 0 ) {
      (*logging) << getTime(this) << "Failed to stat " << (std::string)mountPoint << ": " << errno << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::ERROR);
#endif
    return false;
  } else {
    disk_size = 1.*(fs.f_blocks * fs.f_frsize)*GiB;
    disk_free = 1.*(fs.f_bfree * fs.f_frsize)*GiB;
    disk_user  = 1.*(fs.f_bavail * fs.f_frsize)*GiB;
    return true;
  }
}

void FileSystemModule::mainLoop(){
  deviceName = tmp[0];
  mountPoint = tmp[1];
  deviceName.write();
  mountPoint.write();
  while(1){
    trigger.read();
    if(read()){
      disk_usage = (disk_size-disk_user)/disk_size*100.;
      if(disk_usage > 95){
        (*logging) << getTime(this) << "More than 95% (" << disk_usage << "%) of "
            << (std::string)deviceName << " mounted at " << (std::string)mountPoint << " are used!" << std::endl;
#ifdef ENABLE_LOGGING
        sendMessage(logging::LogLevel::WARNING);
#endif
      } else if (disk_usage > 85){
        (*logging) << getTime(this) << "More than 85% (" << disk_usage << "%) of "
            << (std::string)deviceName << " mounted at " << (std::string)mountPoint << " are used!" << std::endl;
#ifdef ENABLE_LOGGING
        sendMessage(logging::LogLevel::INFO);
#endif
      } else {
        (*logging) << getTime(this) << "Disk usage: " << disk_usage << std::endl;
#ifdef ENABLE_LOGGING
        sendMessage(logging::LogLevel::DEBUG);
#endif
      }
      findTag("SYS").writeAll();
    }
  }
}

#ifdef ENABLE_LOGGING
void FileSystemModule::sendMessage(const logging::LogLevel &level){
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

void FileSystemModule::terminate(){
#ifdef ENABLE_LOGGING
  delete logging;
  logging = 0;
#endif
  ApplicationModule::terminate();
}

NetworkModule::NetworkModule(const std::string &device, EntityOwner *owner, const std::string &name,
       const std::string &description, bool eliminateHierarchy,
       const std::unordered_set<std::string> &tags):
         ctk::ApplicationModule(owner, name, description, eliminateHierarchy, tags){
  tmp = device;
#ifdef ENABLE_LOGGING
  logging = new std::stringstream();
#else
  logging = &std::cerr;
#endif
  data.emplace_back(ctk::ScalarOutput<double>{this, "rx_packates", "1/s", "Received packates.",
      { "CS", "SYS", "DAQ"}});
  data.emplace_back(ctk::ScalarOutput<double>{this, "tx_packates", "1/s", "Transmitted packates.",
      { "CS", "SYS", "DAQ"}});
  data.emplace_back(ctk::ScalarOutput<double>{this, "rx", "MiB/s", "Data rate receive.",
      { "CS", "SYS", "DAQ", "History"}});
  data.emplace_back(ctk::ScalarOutput<double>{this, "tx", "MiB/s", "Data rate transmit.",
      { "CS", "SYS", "DAQ", "History"}});
  data.emplace_back(ctk::ScalarOutput<double>{this, "rx_dropped", "1/s", "Dropped received packates.",
      { "CS", "SYS", "DAQ", "History"}});
  data.emplace_back(ctk::ScalarOutput<double>{this, "tx_dropped", "1/s", "Dropped transmitted packates.",
      { "CS", "SYS", "DAQ", "History"}});
  data.emplace_back(ctk::ScalarOutput<double>{this, "collisions", "1/s", "Number of collisions.",
    { "CS", "SYS", "DAQ"}});
}

bool NetworkModule::read(){
  raw tmp((std::string)deviceName);
  for(size_t i = 0; i < tmp.files.size(); i++){
    std::ifstream in(tmp.files.at(i));
    if(!in.is_open()){
      (*logging) << getTime(this) << "Failed to open: " << tmp.files.at(i) << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::ERROR);
#endif
      return false;
    }
    tmp.time.at(i) = boost::posix_time::microsec_clock::local_time();
    in >> tmp.data.at(i);
    // check if this is the first reading and no data is stored yet in previousData
    if(previousData.files.size() != 0){
      boost::posix_time::time_duration diff = tmp.time.at(i) - previousData.time.at(i);
      data.at(i) =  1.*(tmp.data.at(i) - previousData.data.at(i))/1024/1024/(diff.total_nanoseconds()/1e9);
    }
    in.close();
  }
  previousData = raw(tmp);
  return true;
}

void NetworkModule::mainLoop(){
  deviceName = tmp;
  deviceName.write();
  while(1){
    trigger.read();
    if(read()){
      findTag("SYS").writeAll();
    }
  }
}

#ifdef ENABLE_LOGGING
void NetworkModule::sendMessage(const logging::LogLevel &level){
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

void NetworkModule::terminate(){
#ifdef ENABLE_LOGGING
  delete logging;
  logging = 0;
#endif
  ApplicationModule::terminate();
}
