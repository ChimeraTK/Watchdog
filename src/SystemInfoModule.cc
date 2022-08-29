// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

/*
 * SystemInfo.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "SystemInfoModule.h"

#include "sys_stat.h"
#include <proc/readproc.h>
#include <proc/sysinfo.h>
#include <sys/vfs.h>

#include <cerrno>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// This symbol is introduced by procps and in boost 1.71 a function likely is used!
#undef likely
#include "boost/date_time/posix_time/posix_time.hpp"

SystemInfoModule::SystemInfoModule(EntityOwner* owner, const std::string& name, const std::string& description,
    ctk::HierarchyModifier hierarchyModifier, const std::unordered_set<std::string>& tags,
    const std::string& pathToTrigger)
: ctk::ApplicationModule(owner, name, description, hierarchyModifier, tags), triggerGroup(this, pathToTrigger) {
  for(auto it = sysInfo.ibegin(); it != sysInfo.iend(); it++) {
    info.strInfos.emplace(
        it->first, ctk::ScalarOutput<std::string>{&info, space2underscore(it->first), "", space2underscore(it->first)});
  }
  status.cpu_use = std::make_unique<ctk::ArrayOutput<double>>(&status, "cpuUsage", "%", sysInfo.getNCpu(),
      "CPU usage for each processor", std::unordered_set<std::string>{"history"});
  //  // add 1 since cpuTotal should be added too
  lastInfo = std::vector<cpu>(sysInfo.getNCpu() + 1);
}

void SystemInfoModule::mainLoop() {
  // Set variables that are read by other modules here
  double uptime_secs;
  double idle_secs;
  // read the system start time (uncertainty in the order of seconds...)
  uptime(&uptime_secs, &idle_secs);
  auto now = boost::posix_time::second_clock::local_time();
  status.startTime = boost::posix_time::to_time_t(now) - std::stoi(std::to_string(uptime_secs));
  status.startTimeStr = boost::posix_time::to_simple_string(boost::posix_time::from_time_t(status.startTime));
  status.startTime.write();
  status.startTimeStr.write();
  status.uptime_secTotal = std::stoi(std::to_string(uptime_secs));
  status.uptime_secTotal.write();

  meminfo();
  try {
    status.maxMem = std::stoi(std::to_string(kb_main_total));
    status.maxMem.write();
  }
  catch(std::exception& e) {
    logger->sendMessage(std::string("meminfo conversion failed: ") + e.what(), logging::LogLevel::ERROR);
  }

  info.ticksPerSecond = sysconf(_SC_CLK_TCK);
  for(auto it = sysInfo.ibegin(); it != sysInfo.iend(); it++) {
    info.strInfos.at(it->first) = it->second;
  }
  info.nCPU = sysInfo.getNCpu();
  info.writeAll();

  if(lastInfo.size() != (unsigned)(info.nCPU + 1)) {
    logger->sendMessage(std::string("Failed to open system file /proc/stat") + std::to_string(lastInfo.size()) +
            "\t nCPU" + std::to_string(info.nCPU),
        logging::LogLevel::ERROR);
    throw std::runtime_error("Vector size mismatch in SystemInfoModule::mainLoop.");
  }

  std::ifstream file("/proc/stat");
  if(!file.is_open()) {
    logger->sendMessage("Failed to open system file /proc/stat", logging::LogLevel::ERROR);
    file.close();
    return;
  }

  readCPUInfo(lastInfo);

  while(true) {
    meminfo();
    try {
      status.maxMem = std::stoi(std::to_string(kb_main_total));
      status.freeMem = std::stoi(std::to_string(kb_main_free));
      status.cachedMem = std::stoi(std::to_string(kb_main_cached));
      status.usedMem = std::stoi(std::to_string(status.maxMem - status.freeMem - status.cachedMem));
      status.maxSwap = std::stoi(std::to_string(kb_swap_total));
      status.freeSwap = std::stoi(std::to_string(kb_swap_free));
      status.usedSwap = std::stoi(std::to_string(status.maxSwap - status.freeSwap));
    }
    catch(std::exception& e) {
      logger->sendMessage(std::string("meminfo conversion failed: ") + e.what(), logging::LogLevel::ERROR);
    }
    status.memoryUsage = 1. * status.usedMem / status.maxMem * 100.;
    status.swapUsage = 1. * status.usedSwap / status.maxSwap * 100.;

    // get system uptime
    try {
      uptime(&uptime_secs, &idle_secs);
      //\ToDo: Use stoul once unsigned long is available
      status.uptime_secTotal = std::stoi(std::to_string(uptime_secs));
      status.uptime_day = std::stoi(std::to_string(status.uptime_secTotal / 86400));
      status.uptime_hour = std::stoi(std::to_string((status.uptime_secTotal - (status.uptime_day * 86400)) / 3600));
      status.uptime_min = std::stoi(
          std::to_string((status.uptime_secTotal - (status.uptime_day * 86400) - (status.uptime_hour * 3600)) / 60));
      status.uptime_sec = std::stoi(std::to_string(status.uptime_secTotal - (status.uptime_day * 86400) -
          (status.uptime_hour * 3600) - (status.uptime_min * 60)));
    }
    catch(std::exception& e) {
      logger->sendMessage(std::string("uptime conversion failed: ") + e.what(), logging::LogLevel::ERROR);
    }

    std::vector<double> v_tmp(3);

    loadavg(&v_tmp[0], &v_tmp[1], &v_tmp[2]);
    status.loadAvg = v_tmp;

    calculatePCPU();

    status.writeAll();
    logger->sendMessage("System data updated", logging::LogLevel::DEBUG);

    triggerGroup.trigger.read();
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
    if(newcpu->totalUser < lastcpu->totalUser || newcpu->totalUserLow < lastcpu->totalUserLow ||
        newcpu->totalSys < lastcpu->totalSys || newcpu->totalIdle < lastcpu->totalIdle) {
      // Overflow detection. Just skip this value.
      usage_tmp.at(iCPU) = (-1.0);
    }
    else {
      total = (newcpu->totalUser - lastcpu->totalUser) + (newcpu->totalUserLow - lastcpu->totalUserLow) +
          (newcpu->totalSys - lastcpu->totalSys);
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
  status.cpu_use->operator=(usage_tmp);
}

void SystemInfoModule::readCPUInfo(std::vector<cpu>& vcpu) {
  std::ifstream file("/proc/stat");
  if(!file.is_open()) {
    logger->sendMessage("Failed to open system file /proc/stat", logging::LogLevel::ERROR);
    file.close();
    return;
  }
  std::string::size_type sz = 0;
  std::string line;
  for(auto it = vcpu.begin(); it != vcpu.end(); it++) {
    if(!std::getline(file, line)) {
      logger->sendMessage("Could not find enough lines in /proc/stat", logging::LogLevel::ERROR);
      file.close();
      return;
    }
    auto strs = split_arguments(line);
    if(strs.size() < 5) {
      logger->sendMessage(
          std::string("Line seems to contain not enough data. Line: ") + line, logging::LogLevel::ERROR);
    }

    try {
      it->totalUser = std::stoull(strs.at(1), &sz, 0);
      it->totalUserLow = std::stoull(strs.at(2), &sz, 0);
      it->totalSys = std::stoull(strs.at(3), &sz, 0);
      it->totalIdle = std::stoull(strs.at(4), &sz, 0);
    }
    catch(std::exception& e) {
      std::stringstream ss;
      ss << "Caught an exception: " << e.what() << "\n";

      for(auto s : strs) {
        ss << "String token: " << s << "\n";
      }
      logger->sendMessage(ss.str(), logging::LogLevel::ERROR);
    }
  }
}

std::string getTime(ctk::ApplicationModule* mod) {
  std::string str{"WATCHDOG_SERVER: "};
  str.append(logging::getTime());
  str.append(mod->getOwner()->getName());
  str.append("/");
  str.append(mod->getName());
  str.append(" -> ");
  return str;
}

FileSystemModule::FileSystemModule(const std::string& devName, const std::string& mntPoint, EntityOwner* owner,
    const std::string& name, const std::string& description, ctk::HierarchyModifier hierarchyModifier,
    const std::unordered_set<std::string>& tags, const std::string& pathToTrigger)
: ctk::ApplicationModule(owner, name, description, hierarchyModifier, tags), triggerGroup(this, pathToTrigger) {
  tmp[0] = devName;
  tmp[1] = mntPoint;
}

std::mutex fs_mutex;
bool FileSystemModule::read() {
  std::lock_guard<std::mutex> lock(fs_mutex);
  struct statfs fs;
  if((statfs(((std::string)status.mountPoint).c_str(), &fs)) < 0) {
    logger->sendMessage(std::string("Failed to stat ") + (std::string)status.mountPoint + ": " + std::to_string(errno),
        logging::LogLevel::ERROR);
    return false;
  }
  else {
    status.disk_size = 1. * (fs.f_blocks * fs.f_frsize) * GiB;
    status.disk_free = 1. * (fs.f_bfree * fs.f_frsize) * GiB;
    status.disk_user = 1. * (fs.f_bavail * fs.f_frsize) * GiB;
    return true;
  }
}

void FileSystemModule::mainLoop() {
  deviceName = tmp[0];
  status.mountPoint = tmp[1];
  deviceName.write();
  status.mountPoint.write();

  auto group = readAnyGroup();
  while(1) {
    if(read()) {
      status.disk_usage = (status.disk_size - status.disk_user) / status.disk_size * 100.;
      if((config.errorLevel > 0 && status.disk_usage > config.errorLevel) ||
          (config.errorLevel == 0 && status.disk_usage > 95)) {
        logger->sendMessage(std::string("More than ") + std::to_string(config.errorLevel) + "% (" +
                std::to_string(status.disk_usage) + "%) of " + (std::string)deviceName + " mounted at " +
                (std::string)status.mountPoint + " are used!",
            logging::LogLevel::WARNING);
        status.disk_status = 2;
      }
      else if((config.warningLevel > 0 && status.disk_usage > config.warningLevel) ||
          (config.warningLevel == 0 && status.disk_usage > 90)) {
        logger->sendMessage(std::string("More than ") + std::to_string(config.warningLevel) + "% (" +
                std::to_string(status.disk_usage) + "%) of " + (std::string)deviceName + " mounted at " +
                (std::string)status.mountPoint + " are used!",
            logging::LogLevel::INFO);
        status.disk_status = 1;
      }
      else {
        status.disk_status = 0;
        logger->sendMessage(std::string("Disc usage: ") + std::to_string(status.disk_usage), logging::LogLevel::DEBUG);
      }
      status.writeAll();
    }
    group.readUntil(triggerGroup.trigger.getId());
  }
}

NetworkModule::NetworkModule(const std::string& device, EntityOwner* owner, const std::string& name,
    const std::string& description, ctk::HierarchyModifier hierarchyModifier,
    const std::unordered_set<std::string>& tags, const std::string& pathToTrigger)
: ctk::ApplicationModule(owner, name, description, hierarchyModifier, tags), triggerGroup(this, pathToTrigger) {
  networkDeviceName = device;
  status.data.emplace_back(ctk::ScalarOutput<double>{&status, "rx_packates", "1/s", "Received packates.", {"DAQ"}});
  status.data.emplace_back(ctk::ScalarOutput<double>{&status, "tx_packates", "1/s", "Transmitted packates.", {"DAQ"}});
  status.data.emplace_back(ctk::ScalarOutput<double>{&status, "rx", "MiB/s", "Data rate receive.", {"DAQ", "history"}});
  status.data.emplace_back(
      ctk::ScalarOutput<double>{&status, "tx", "MiB/s", "Data rate transmit.", {"DAQ", "history"}});
  status.data.emplace_back(
      ctk::ScalarOutput<double>{&status, "rx_dropped", "1/s", "Dropped received packates.", {"DAQ", "history"}});
  status.data.emplace_back(
      ctk::ScalarOutput<double>{&status, "tx_dropped", "1/s", "Dropped transmitted packates.", {"DAQ", "history"}});
  status.data.emplace_back(ctk::ScalarOutput<double>{&status, "collisions", "1/s", "Number of collisions.", {"DAQ"}});
}

void NetworkModule::read() {
  raw tmp((std::string)deviceName);
  for(size_t i = 0; i < tmp.files.size(); i++) {
    std::ifstream in(tmp.files.at(i));
    if(!in.is_open()) {
      logger->sendMessage(std::string("Failed to open: ") + tmp.files.at(i), logging::LogLevel::ERROR);
      return;
    }
    tmp.time.at(i) = boost::posix_time::microsec_clock::local_time();
    in >> tmp.data.at(i);
    // check if this is the first reading and no data is stored yet in previousData
    if(previousData.files.size() != 0) {
      boost::posix_time::time_duration diff = tmp.time.at(i) - previousData.time.at(i);
      status.data.at(i) =
          1. * (tmp.data.at(i) - previousData.data.at(i)) / 1024 / 1024 / (diff.total_nanoseconds() / 1e9);
    }
    in.close();
  }
  previousData = raw(tmp);
  status.writeAll();
}

void NetworkModule::mainLoop() {
  deviceName = networkDeviceName;
  deviceName.write();

  while(1) {
    read();
    triggerGroup.trigger.read();
  }
}
