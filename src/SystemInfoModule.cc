/*
 * SystemInfo.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "SystemInfoModule.h"

#include <sstream>

#include <proc/readproc.h>
#include <proc/sysinfo.h>

#include "sys_stat.h"

SystemInfoModule::SystemInfoModule(EntityOwner *owner, const std::string &name, const std::string &description,
        bool eliminateHierarchy, const std::unordered_set<std::string> &tags):
		ctk::ApplicationModule(owner, name, description, eliminateHierarchy, tags){
	for(auto it = sysInfo.ibegin; it != sysInfo.iend; it++){
#ifdef DEBUG
		std::cout << "Adding sysInfor: " << space2underscore(it->first) << std::endl;
#endif
		strInfos[it->first].replace( ctk::ScalarOutput<std::string>{this, space2underscore(it->first), "" ,space2underscore(it->first)});
	}
	// add two since nCPU starts counting at 0 and cpuTotal should be added too
	for(int iCPU = 0; iCPU < (sysInfo.getNCpu() + 2); iCPU++){
	  std::stringstream ss;
	  ss << "cpu";
	  if(iCPU != 0){
	       ss << iCPU;
	  ss << "Usage";
	  cpu_use.emplace_back(new ctk::ScalarOutput<double>(this, ss.str(), "%", "CPU usage", {"CS", "SYS"}));
	  } else {
	    cpu_use.emplace_back(new ctk::ScalarOutput<double>(this, "cpuUsageTotal", "%", "Total CPU usage", {"CS", "SYS"}));
	  }
	  lastInfo.push_back(cpu());
	}
}

void SystemInfoModule::mainLoop(){
	for(auto it = sysInfo.ibegin; it != sysInfo.iend; it++){
		strInfos.at(it->first) = it->second;
		strInfos.at(it->first).write();
	}
	nCPU = sysInfo.getNCpu();
	nCPU.write();
	ticksPerSecond = sysconf(_SC_CLK_TCK);
	ticksPerSecond.write();
	double tmp[3] = {0., 0., 0.};

  if(lastInfo.size() != (unsigned)(nCPU+2)){
    std::cerr << getName() << "Size of lastInfo: " << lastInfo.size() << "\t nCPU: " << nCPU << std::endl;
    throw std::runtime_error("Vector size mismatch in SystemInfoModule::mainLoop.");
  }

  if(cpu_use.size() != (unsigned)(nCPU+2)){
    std::cerr << getName() << "Size of cpu_use: " << cpu_use.size() << "\t nCPU: " << nCPU << std::endl;
    throw std::runtime_error("Vector size mismatch in SystemInfoModule::mainLoop.");
  }

	std::ifstream file("/proc/stat");
  if(!file.is_open()){
    std::cerr << getName() << "Failed to open system file /proc/stat" << std::endl;
    file.close();
    return;
  }

  readCPUInfo(lastInfo);

	while(true){
	  trigger.read();

	  meminfo ();
	  try{
      maxMem            = std::stoi(std::to_string(kb_main_total));
      freeMem           = std::stoi(std::to_string(kb_main_free));
      cachedMem         = std::stoi(std::to_string(kb_main_cached));
      usedMem           = std::stoi(std::to_string(maxMem - freeMem));
      maxSwap           = std::stoi(std::to_string(kb_swap_total));
      freeSwap          = std::stoi(std::to_string(kb_swap_free));
      usedSwap          = std::stoi(std::to_string(maxSwap - freeSwap));
	  } catch (std::exception &e){
	    std::cerr << getName() << "Conversion failed: " << e.what() << std::endl;
	  }
		maxMem.write();
		freeMem.write();
		cachedMem.write();
		usedMem.write();
		maxSwap.write();
		freeSwap.write();
		usedSwap.write();

    // get system uptime
		try{
      double    uptime_secs;
      double    idle_secs;
      uptime (&uptime_secs, &idle_secs);
      uptime_sec        = std::stoi(std::to_string(uptime_secs));
      uptime_days       = std::stoi(std::to_string(uptime_sec / 86400));
      uptime_day_hour   = std::stoi(std::to_string((uptime_sec - (uptime_days * 86400)) / 3600));
      uptime_day_mins   = std::stoi(std::to_string((uptime_sec - (uptime_days * 86400) -
                          (uptime_day_hour * 3600)) / 60));
    } catch (std::exception &e){
      std::cerr << getName() << "Conversion failed: " << e.what() << std::endl;
    }

		loadavg (&tmp[0], &tmp[1], &tmp[2]);
		loadAvg = tmp[0];
		loadAvg5 = tmp[1];
		loadAvg15 = tmp[2];

		uptime_sec.write();
		uptime_days.write();
		uptime_day_hour.write();
		uptime_day_mins.write();
		loadAvg.write();
		loadAvg5.write();
		loadAvg15.write();

		calculatePCPU();
	}
}

void SystemInfoModule::calculatePCPU(){
  unsigned long long total;
  double tmp;
  std::vector<std::string> strs;
  std::vector<cpu> vcpu(nCPU + 2);
  readCPUInfo(vcpu);
  auto lastcpu = lastInfo.begin();
  auto newcpu  = vcpu.begin();
  for(int iCPU = 0; iCPU < (nCPU + 2); iCPU++){
    if (newcpu->totalUser < lastcpu->totalUser || newcpu->totalUserLow < lastcpu->totalUserLow ||
        newcpu->totalSys < lastcpu->totalSys || newcpu->totalIdle < lastcpu->totalIdle){
        //Overflow detection. Just skip this value.
      cpu_use.at(iCPU)->operator =(-1.0);
    }
    else{
        total = (newcpu->totalUser - lastcpu->totalUser) + (newcpu->totalUserLow - lastcpu->totalUserLow) +
            (newcpu->totalSys - lastcpu->totalSys);
        tmp = total;
        total += (newcpu->totalIdle - lastcpu->totalIdle);
        tmp /= total;
        tmp *= 100.;
        cpu_use.at(iCPU)->operator =(tmp);

    }
    *lastcpu = *newcpu;
    cpu_use.at(iCPU)->write();
    lastcpu++;
    newcpu++;
  }
}

void SystemInfoModule::readCPUInfo(std::vector<cpu> &vcpu){
  std::ifstream file("/proc/stat");
  if(!file.is_open()){
    std::cerr << getName() << "Failed to open system file /proc/stat" << std::endl;
    file.close();
    return;
  }
  std::string::size_type sz = 0;
  std::string line;
  for(auto it = vcpu.begin(); it != vcpu.end(); it++){
    if(!std::getline(file,line)){
      std::cerr << getName() << "Could not find enough lines in /proc/stat" << std::endl;
      file.close();
      return;
    }
    auto strs = split_arguments(line);
    if(strs.size() < 5){
      std::cerr << getName() << "Line seems to contain not enough data. Line: " << line << std::endl;
    }

    try{
      it->totalUser = std::stoull(strs.at(1),&sz,0);
      it->totalUserLow = std::stoull(strs.at(2),&sz,0);
      it->totalSys = std::stoull(strs.at(3),&sz,0);
      it->totalIdle = std::stoull(strs.at(4),&sz,0);
    } catch (std::exception &e){
      std::cerr << getName() << "Caught an exception: " << e.what() << std::endl;
      for(auto s : strs){
        std::cerr << getName() << "String token: " << s << std::endl;
      }
    }
  }
}
