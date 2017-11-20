/*
 * ProcessModule.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "ProcessModule.h"
#include <signal.h>

void ProcessInfoModule::mainLoop(){
  processPID = getpid();
  processPID.write();
  while(true) {
    trigger.read();
    FillProcInfo(proc_util::getInfo(processPID));
  }
}

void ProcessInfoModule::FillProcInfo(const std::shared_ptr<proc_t> &info){
  if(info != nullptr) {
    auto now = boost::posix_time::microsec_clock::local_time();
    int old_time = 0;
    try {
      old_time = std::stoi(std::to_string(utime + stime + cutime + cstime));
      utime = std::stoi(std::to_string(info->utime));
      stime = std::stoi(std::to_string(info->stime));
      cutime = std::stoi(std::to_string(info->cutime));
      cstime = std::stoi(std::to_string(info->cstime));


      // info->start_time reads s since system was started
      sysStartTime.read();
      int relativeStartTime = 1. * std::stoi(std::to_string(info->start_time)) / ticksPerSecond;
      startTime = sysStartTime + relativeStartTime;
      startTimeStr = boost::posix_time::to_simple_string(boost::posix_time::from_time_t(startTime));
      priority = std::stoi(std::to_string(info->priority));
      nice = std::stoi(std::to_string(info->nice));
      rss = std::stoi(std::to_string(info->rss));

      sysUpTime.read();
      ticksPerSecond.read();
      runtime = std::stoi(std::to_string(sysUpTime - std::stoi(std::to_string(info->start_time)) * 1. / ticksPerSecond));
    } catch(std::exception &e) {
      std::cerr << getName() << "FillProcInfo::Conversion failed: " << e.what()
          << std::endl;
    }
    // check if it is the first call after process is started (time_stamp  == not_a_date_time)
    if(!time_stamp.is_special()) {
      boost::posix_time::time_duration diff = now - time_stamp;
      pcpu = 1. * (utime + stime + cutime + cstime - old_time) / ticksPerSecond
          / (diff.total_milliseconds() / 1000) * 100;
      avpcpu = 1. * (utime + stime + cutime + cstime) / ticksPerSecond / runtime
          * 100;
    }
    time_stamp = now;
  } else {
    time_stamp = boost::posix_time::not_a_date_time;
    utime     = 0;
    stime     = 0;
    cutime    = 0;
    cstime    = 0;
    startTime = 0;
    startTimeStr = "";
    priority  = 0;
    nice      = 0;
    rss       = 0;
    pcpu      = 0;
    avpcpu    = 0;
    runtime   = 0;
  }
  utime    .write();
  stime    .write();
  cutime   .write();
  cstime   .write();
  startTime.write();
  startTimeStr.write();
  priority .write();
  nice     .write();
  rss      .write();
  pcpu     .write();
  avpcpu   .write();
  runtime  .write();
}

#ifdef BOOST_1_64
void ProcessControlModule::mainLoop() {
  SetOffline();
  processRestarts = 0;
  processRestarts.write();
  while(true) {
    trigger.read();
    enableProcess.read();
    // reset number of failed tries in case the process is set offline
    if(!enableProcess) {
      processNFailed = 0;
      processNFailed.write();
    }

    // don't do anything in case failed more than 4 times -> to reset turn off/on the process
    if(processNFailed > 4) {
      usleep(200000);
      continue;
    }

    if(enableProcess) {
      if(process.get() == nullptr || !process->running()) {
        std::cout << getName() << "::Trying to start the process..." << std::endl;
        processCMD.read();
        try {
          process.reset(new bp::child((std::string)processPath + (std::string)processCMD));
          SetOnline(process->id());
          process->detach();
        } catch (std::system_error &e) {
          std::cerr << getName() << "::Failed to start the process with cmd: " << (std::string)processPath + (std::string)processCMD << "\n Message: " << e.what() << std::endl;
          Failed();
        }

      } else {
        std::cout << "Process is running..." << std::endl;
      }
    } else {
      if(process.get() == nullptr || !process->running()) {
        std::cout << getName() << "::Process is not running...OK" << std::endl;
      } else if (process->running()) {
        std::cout << getName() << "::Trying to kill the process..." << std::endl;
        try {
          process->terminate();
          process.reset();
          SetOffline();
        } catch (std::system_error &e) {
          std::cerr << getName() << "::Failed to kill the process." << std::endl;
        }
      }
    }
  }
}
#else

void ProcessControlModule::mainLoop() {
  SetOffline();
  processRestarts = 0;
  processRestarts.write();
  while(true) {
    trigger.read();
    enableProcess.read();
    // reset number of failed tries and restarts in case the process is set offline
    if(!enableProcess) {
      processNFailed = 0;
      processNFailed.write();
      processRestarts = 0;
      processRestarts.write();
    }

    // don't do anything in case failed more than 4 times -> to reset turn off/on the process
    if(processNFailed > 4) {
      usleep(200000);
      continue;
    }

    if(processPID > 0 && enableProcess) {
      if(!proc_util::isProcessRunning(processPID)) {
        Failed();
        std::cerr << getName()
            << "::Child process not running any more, but it should run!"
            << std::endl;
        processRestarts += 1;
        processRestarts.write();

      } else {
        processIsRunning = 1;
        processIsRunning.write();
      }
    }

    if(enableProcess) {
      if(processPID < 0) {
        std::cout << getName() << "::Trying to start a new process..." << std::endl;
        try {
          processPath.read();
          processCMD.read();
          SetOnline(
              process.startProcess((std::string) processPath,
                  (std::string) processCMD, getName()));
          processNChilds = proc_util::getNChilds(processPID);
          processNChilds.write();
        } catch(std::runtime_error &e) {
          std::cout << e.what() << std::endl;
          Failed();
        }
      } else {
#ifdef DEBUG
        std::cout << getName() << "::Process is running..." << processIsRunning << " PID: " << getpid() << std::endl;
#endif
        pidOffset.read();
        FillProcInfo(proc_util::getInfo(processPID + pidOffset));
      }
    } else {
      if(processPID < 0) {
        processIsRunning = 0;
        processIsRunning.write();
#ifdef DEBUG
        std::cout << getName() << "::Process Running: " << processIsRunning << std::endl;
        std::cout << getName() << "::Process is not running...OK" << " PID: " << getpid() <<std::endl;
#endif
      } else {
        std::cout << getName() << "::Trying to kill the process..." << " PID: "
            << processPID << std::endl;
        killSig.read();
        process.killProcess(processPID, killSig);
        usleep(200000);
        if(proc_util::isProcessRunning(processPID)) {
          std::cerr << getName()
              << "::Failed to kill process with PID: " << processPID << ". Try another signal, e.g. SIGKILL (9)."
              << std::endl;
        } else {
          SetOffline();
        }
      }
    }
  }
}
#endif

void ProcessControlModule::SetOnline(const int &pid){
  processPID = pid;
  processPID.write();
  processIsRunning = 1;
  processIsRunning.write();
}

void ProcessControlModule::SetOffline(){
  processPID = -1;
  processPID.write();
  processIsRunning = 0;
  processIsRunning.write();
  FillProcInfo(nullptr);
}

void ProcessControlModule::Failed(){
  processNFailed = processNFailed + 1;
  processNFailed.write();
  SetOffline();
  sleep(2);
}

