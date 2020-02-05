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

#include "Logging.h"

SystemInfoModule::SystemInfoModule(EntityOwner *owner, const std::string &name,
    const std::string &description, bool eliminateHierarchy,
    const std::unordered_set<std::string> &tags) :
    ctk::ApplicationModule(owner, name, description, eliminateHierarchy, tags) {
  for(auto it = sysInfo.ibegin(); it != sysInfo.iend(); it++) {
    info.strInfos.emplace(it->first, ctk::ScalarOutput<std::string> {&info,
              space2underscore(it->first), "", space2underscore(it->first), {"CS"} });
  }
  status.cpu_use = std::make_unique<ctk::ArrayOutput<double> >(
      &status, "cpuUsage", "%", sysInfo.getNCpu(), "CPU usage for each processor",
      std::unordered_set<std::string>{"CS", "SYS", "History"});
//  // add 1 since cpuTotal should be added too
  lastInfo = std::vector<cpu>(sysInfo.getNCpu() + 1);
#ifdef ENABLE_LOGGING
  logStream = new std::stringstream();
#else
  logStream = &std::cerr;
#endif
}

void SystemInfoModule::mainLoop() {
  for(auto it = sysInfo.ibegin(); it != sysInfo.iend(); it++) {
    info.strInfos.at(it->first) = it->second;
  }
  info.nCPU = sysInfo.getNCpu();
  info.ticksPerSecond = sysconf(_SC_CLK_TCK);

  info.writeAll();

  if(lastInfo.size() != (unsigned) (info.nCPU + 1)) {
    (*logStream) << getName() << "Size of lastInfo: " << lastInfo.size()
        << "\t nCPU: " << info.nCPU << std::endl;
#ifdef ENABLE_LOGGING
    sendMessage(logging::LogLevel::ERROR);
#endif
    throw std::runtime_error(
        "Vector size mismatch in SystemInfoModule::mainLoop.");
  }

  std::ifstream file("/proc/stat");
  if(!file.is_open()) {
    (*logStream) << getTime(this) << "Failed to open system file /proc/stat"
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
  status.startTime = boost::posix_time::to_time_t(now) - std::stoi(std::to_string(uptime_secs));
  status.startTimeStr = boost::posix_time::to_simple_string(boost::posix_time::from_time_t(status.startTime));
  status.startTime.write();
  status.startTimeStr.write();
  
  // findTag takes some time and should not be called with every loop
  auto toWrite = findTag("SYS");
  // ToDo: Remove once the bug in the VitualModule is fixed (issue #30)
  toWrite.setOwner(this);

  while(true) {
    trigger.read();

    meminfo();
    try {
      status.maxMem    = std::stoi(std::to_string(kb_main_total));
      status.freeMem   = std::stoi(std::to_string(kb_main_free));
      status.cachedMem = std::stoi(std::to_string(kb_main_cached));
      status.usedMem   = std::stoi(std::to_string(status.maxMem - status.freeMem - status.cachedMem));
      status.maxSwap   = std::stoi(std::to_string(kb_swap_total));
      status.freeSwap  = std::stoi(std::to_string(kb_swap_free));
      status.usedSwap  = std::stoi(std::to_string(status.maxSwap - status.freeSwap));
    } catch(std::exception &e) {
      (*logStream) << getTime(this) << "meminfo conversion failed: " << e.what() << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::ERROR);
#endif
    }
    status.memoryUsage = 1.*status.usedMem/status.maxMem*100.;
    status.swapUsage = 1.*status.usedSwap/status.maxSwap*100.;
    
    // get system uptime
    try {
      uptime(&uptime_secs, &idle_secs);
      //\ToDo: Use stoul once unsigned long is available
      status.uptime_secTotal = std::stoi(std::to_string(uptime_secs));
      status.uptime_day     = std::stoi(std::to_string(status.uptime_secTotal / 86400));
      status.uptime_hour = std::stoi(
          std::to_string((status.uptime_secTotal - (status.uptime_day * 86400)) / 3600));
      status.uptime_min = std::stoi(std::to_string(
              (status.uptime_secTotal - (status.uptime_day * 86400) - (status.uptime_hour * 3600))
                  / 60));
      status.uptime_sec = std::stoi(std::to_string(
          status.uptime_secTotal - (status.uptime_day * 86400) - (status.uptime_hour * 3600) - (status.uptime_min*60)));
    } catch(std::exception &e) {
      (*logStream) << getTime(this) << "uptime conversion failed: " << e.what() << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::ERROR);
#endif
    }

    std::vector<double> v_tmp(3);

    loadavg(&v_tmp[0], &v_tmp[1], &v_tmp[2]);
    status.loadAvg = v_tmp;

    calculatePCPU();

    toWrite.writeAll();
#ifdef ENABLE_LOGGING
    (*logStream) << getTime(this) << "System data updated" << std::endl;
    sendMessage(logging::LogLevel::DEBUG);
#endif
  }
}

void SystemInfoModule::calculatePCPU() {
  unsigned long long total;
  double tmp;
  std::vector<double> usage_tmp(info.nCPU + 1);
  std::vector<std::string> strs;
  std::vector<cpu> vcpu(info.nCPU + 1);
  readCPUInfo(vcpu);
  auto lastcpu = lastInfo.begin();
  auto newcpu = vcpu.begin();
  for(size_t iCPU = 0; iCPU < (info.nCPU + 1); iCPU++) {
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
  status.cpu_useTotal = usage_tmp.back();
  usage_tmp.pop_back();
  status.cpu_use->operator =(usage_tmp);
}

void SystemInfoModule::readCPUInfo(std::vector<cpu> &vcpu) {
  std::ifstream file("/proc/stat");
  if(!file.is_open()) {
    (*logStream) << getTime(this) << "Failed to open system file /proc/stat"
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
      (*logStream) << getTime(this) << "Could not find enough lines in /proc/stat"
          << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::ERROR);
#endif
      file.close();
      return;
    }
    auto strs = split_arguments(line);
    if(strs.size() < 5) {
      (*logStream) << getTime(this) << "Line seems to contain not enough data. Line: "
          << line << std::endl;
    }

    try {
      it->totalUser    = std::stoull(strs.at(1), &sz, 0);
      it->totalUserLow = std::stoull(strs.at(2), &sz, 0);
      it->totalSys     = std::stoull(strs.at(3), &sz, 0);
      it->totalIdle    = std::stoull(strs.at(4), &sz, 0);
    } catch(std::exception &e) {
      (*logStream) << getTime(this) << "Caught an exception: " << e.what()
          << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::ERROR);
#endif
      for(auto s : strs) {
        (*logStream) << getTime(this) << "String token: " << s << std::endl;
#ifdef ENABLE_LOGGING
        sendMessage(logging::LogLevel::ERROR);
#endif
      }
    }
  }
}

void SystemInfoModule::terminate(){
#ifdef ENABLE_LOGGING
  delete logStream;
  logStream = 0;
#endif
  ApplicationModule::terminate();
}

#ifdef ENABLE_LOGGING
void SystemInfoModule::sendMessage(const logging::LogLevel &level){
  auto logging_ss = dynamic_cast<std::stringstream*>(logStream);
  if(logging_ss){
    logging.message = logging_ss->str();
    logging.messageLevel = level;
    logging.writeAll();
    logging_ss->clear();
    logging_ss->str("");
  }
}
#endif

std::string getTime(ctk::ApplicationModule* mod){
  std::string str{"WATCHDOG_SERVER: "};
  str.append(logging::getTime());
  str.append(mod->getOwner()->getName());
  str.append("/");
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
  logStream = new std::stringstream();
#else
  logStream = &std::cerr;
#endif
}

std::mutex fs_mutex;
bool FileSystemModule::read(){
  std::lock_guard<std::mutex> lock(fs_mutex);
  struct statfs fs;
  if( ( statfs( ((std::string)status.mountPoint).c_str(), &fs ) ) < 0 ) {
      (*logStream) << getTime(this) << "Failed to stat " << (std::string)status.mountPoint << ": " << errno << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::ERROR);
#endif
    return false;
  } else {
    status.disk_size = 1.*(fs.f_blocks * fs.f_frsize)*GiB;
    status.disk_free = 1.*(fs.f_bfree * fs.f_frsize)*GiB;
    status.disk_user  = 1.*(fs.f_bavail * fs.f_frsize)*GiB;
    return true;
  }
}

void FileSystemModule::mainLoop(){
  deviceName = tmp[0];
  status.mountPoint = tmp[1];
  deviceName.write();
  status.mountPoint.write();

  // findTag takes some time and should not be called with every loop
  auto toWrite = findTag("SYS");
  // ToDo: Remove once the bug in the VitualModule is fixed (issue #30)
  toWrite.setOwner(this);

  while(1){
    trigger.read();
    config.readAll();
    if(read()){
      status.disk_usage = (status.disk_size-status.disk_user)/status.disk_size*100.;
      if(status.disk_usage > config.errorLevel){
        (*logStream) << getTime(this) << "More than " << config.errorLevel << "% (" << status.disk_usage << "%) of "
            << (std::string)deviceName << " mounted at " << (std::string)status.mountPoint << " are used!" << std::endl;
        status.disk_status = 2;
#ifdef ENABLE_LOGGING
        sendMessage(logging::LogLevel::WARNING);
#endif
      } else if (status.disk_usage > config.warningLevel){
        (*logStream) << getTime(this) << "More than " << config.warningLevel << "% (" << status.disk_usage << "%) of "
            << (std::string)deviceName << " mounted at " << (std::string)status.mountPoint << " are used!" << std::endl;
        status.disk_status = 1;
#ifdef ENABLE_LOGGING
        sendMessage(logging::LogLevel::INFO);
#endif
      } else {
        status.disk_status = 0;
#ifdef ENABLE_LOGGING
        (*logStream) << getTime(this) << "Disk usage: " << status.disk_usage << std::endl;
        sendMessage(logging::LogLevel::DEBUG);
#endif
      }
      toWrite.writeAll();
    }
  }
}

#ifdef ENABLE_LOGGING
void FileSystemModule::sendMessage(const logging::LogLevel &level){
  auto logging_ss = dynamic_cast<std::stringstream*>(logStream);
  if(logging_ss){
    logging.message = logging_ss->str();
    logging.messageLevel = level;
    logging.writeAll();
    logging_ss->clear();
    logging_ss->str("");
  }
}
#endif

void FileSystemModule::terminate(){
#ifdef ENABLE_LOGGING
  delete logStream;
  logStream = 0;
#endif
  ApplicationModule::terminate();
}

NetworkModule::NetworkModule(const std::string &device, EntityOwner *owner, const std::string &name,
       const std::string &description, bool eliminateHierarchy,
       const std::unordered_set<std::string> &tags):
         ctk::ApplicationModule(owner, name, description, eliminateHierarchy, tags){
  tmp = device;
#ifdef ENABLE_LOGGING
  logStream = new std::stringstream();
#else
  logStream = &std::cerr;
#endif
  status.data.emplace_back(ctk::ScalarOutput<double>{&status, "rx_packates", "1/s", "Received packates.",
      { "CS", "SYS", "DAQ"}});
  status.data.emplace_back(ctk::ScalarOutput<double>{&status, "tx_packates", "1/s", "Transmitted packates.",
      { "CS", "SYS", "DAQ"}});
  status.data.emplace_back(ctk::ScalarOutput<double>{&status, "rx", "MiB/s", "Data rate receive.",
      { "CS", "SYS", "DAQ", "History"}});
  status.data.emplace_back(ctk::ScalarOutput<double>{&status, "tx", "MiB/s", "Data rate transmit.",
      { "CS", "SYS", "DAQ", "History"}});
  status.data.emplace_back(ctk::ScalarOutput<double>{&status, "rx_dropped", "1/s", "Dropped received packates.",
      { "CS", "SYS", "DAQ", "History"}});
  status.data.emplace_back(ctk::ScalarOutput<double>{&status, "tx_dropped", "1/s", "Dropped transmitted packates.",
      { "CS", "SYS", "DAQ", "History"}});
  status.data.emplace_back(ctk::ScalarOutput<double>{&status, "collisions", "1/s", "Number of collisions.",
    { "CS", "SYS", "DAQ"}});
}

bool NetworkModule::read(){
  raw tmp((std::string)deviceName);
  for(size_t i = 0; i < tmp.files.size(); i++){
    std::ifstream in(tmp.files.at(i));
    if(!in.is_open()){
      (*logStream) << getTime(this) << "Failed to open: " << tmp.files.at(i) << std::endl;
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
      status.data.at(i) =  1.*(tmp.data.at(i) - previousData.data.at(i))/1024/1024/(diff.total_nanoseconds()/1e9);
    }
    in.close();
  }
  previousData = raw(tmp);
  return true;
}

void NetworkModule::mainLoop(){
  deviceName = tmp;
  deviceName.write();

  // findTag takes some time and should not be called with every loop
  auto toWrite = findTag("SYS");
  // ToDo: Remove once the bug in the VitualModule is fixed (issue #30)
  toWrite.setOwner(this);

  while(1){
    trigger.read();
    if(read()){
      toWrite.writeAll();
    }
  }
}

#ifdef ENABLE_LOGGING
void NetworkModule::sendMessage(const logging::LogLevel &level){
  auto logging_ss = dynamic_cast<std::stringstream*>(logStream);
  if(logging_ss){
    logging.message = logging_ss->str();
    logging.messageLevel = level;
    logging.writeAll();
    logging_ss->clear();
    logging_ss->str("");
  }
}
#endif

void NetworkModule::terminate(){
#ifdef ENABLE_LOGGING
  delete logStream;
  logStream = 0;
#endif
  ApplicationModule::terminate();
}
