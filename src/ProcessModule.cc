/*
 * ProcessModule.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "ProcessModule.h"
#include <signal.h>

#ifdef BOOST_1_64
void ProcessModule::mainLoop(){
  SetOffline();
  processRestarts = 0;
  processRestarts.write();
  while(true) {
    startProcess.read();
    // reset number of failed tries in case the process is set offline
    if(!startProcess){
      processNFailed = 0;
      processNFailed.write();
    }

    // don't do anything in case failed more than 4 times -> to reset turn off/on the process
    if(processNFailed > 4){
      usleep(200000);
      continue;
    }

    if(startProcess){
      if(process.get() == nullptr || !process->running()){
        std::cout << getName() << "::Trying to start the process..." << std::endl;
        processCMD.read();
        try{
          process.reset(new bp::child((std::string)processPath + (std::string)processCMD));
          SetOnline(process->id());
          process->detach();
        } catch (std::system_error &e){
          std::cerr << getName() << "::Failed to start the process with cmd: " << (std::string)processPath + (std::string)processCMD << "\n Message: " << e.what() << std::endl;
          Failed();
        }

      } else {
        std::cout << "Process is running..." << std::endl;
      }
    } else {
      if(process.get() == nullptr || !process->running()){
        std::cout << getName() << "::Process is not running...OK" << std::endl;
      } else if (process->running()) {
        std::cout << getName() << "::Trying to kill the process..." << std::endl;
        try{
          process->terminate();
          process.reset();
          SetOffline();
        } catch (std::system_error &e){
          std::cerr << getName() << "::Failed to kill the process." << std::endl;
        }
      }
    }
    usleep(200000);
  }
}
#else

void ProcessModule::mainLoop(){
  SetOffline();
  processRestarts = 0;
  processRestarts.write();
  while(true) {
    startProcess.read();
    // reset number of failed tries in case the process is set offline
    if(!startProcess){
      processNFailed = 0;
      processNFailed.write();
    }

    // don't do anything in case failed more than 4 times -> to reset turn off/on the process
    if(processNFailed > 4){
      usleep(200000);
      continue;
    }

    if(processPID > 0 && startProcess){
      if(!process.isProcessRunning(processPID)){
        Failed();
        std::cerr << getName() << "::Child process not running any more, but it should run!" << std::endl;
        processRestarts += 1;
        processRestarts.write();

      } else {
        processRunning = 1;
        processRunning.write();
      }
    }

    if(startProcess){
      if(processPID < 0){
        std::cout << getName() << "::Trying to start the process..." << " PID: " << getpid() <<std::endl;
        try{
          processPath.read();
          processCMD.read();
          SetOnline(process.startProcess((std::string)processPath, (std::string)processCMD, getName()));
          processNChilds = process.getNChilds(processPID);
          processNChilds.write();
        } catch (std::runtime_error &e){
          std::cout << e.what() << std::endl;
          Failed();
        }
      } else {
#ifdef DEBUG
        std::cout << getName() << "::Process is running..." << processRunning << " PID: " << getpid() << std::endl;
#endif
        pidOffset.read();
        FillProcInfo(process.getInfo(processPID+pidOffset));
      }
    } else {
      if(processPID < 0 ){
#ifdef DEBUG
        std::cout << getName() << "::Process Running: " << processRunning << std::endl;
        std::cout << getName() << "::Process is not running...OK" << " PID: " << getpid() <<std::endl;
#endif
      } else {
        std::cout << getName() << "::Trying to kill the process..." << " PID: " << getpid() <<std::endl;
        killSig.read();
        process.killProcess(processPID, killSig);
        usleep(200000);
        if(process.isProcessRunning(processPID)){
          std::cerr << getName() << "::Failed to kill the process. Try another signal, e.g. SIGKILL (9)." << std::endl;
        } else {
          SetOffline();
        }
      }
    }
//    usleep(200000);
    sleep(2);
  }
}

void ProcessModule::SetOnline(const int &pid){
  processPID = pid;
  processPID.write();
  processRunning = 1;
  processRunning.write();
}

void ProcessModule::SetOffline(){
  processPID = -1;
  processPID.write();
  processRunning = 0;
  processRunning.write();
  FillProcInfo(nullptr);
}

void ProcessModule::Failed(){
  processNFailed = processNFailed + 1;
    processNFailed.write();
    SetOffline();
    sleep(2);
}

void ProcessModule::FillProcInfo(const std::shared_ptr<proc_t> &info){
  if(info != nullptr){
    auto now = boost::posix_time::microsec_clock::local_time();
    int old_time = utime + stime + cutime + cstime;
    utime     = info->utime;
    stime     = info->stime;
    cutime    = info->cutime;
    cstime    = info->cstime;
    startTime = info->start_time;
    priority  = info->priority;
    nice      = info->nice;
    rss       = info->rss;
    uptime.read();
    ticksPerSecond.read();
    runtime   = uptime - startTime*1./ticksPerSecond;
    // check if it is the first call after process is started (time_stamp  == not_a_date_time)
    if(!time_stamp.is_special()){
      boost::posix_time::time_duration diff = now - time_stamp;
      pcpu      = 1.*(utime + stime + cutime + cstime - old_time)/ticksPerSecond / (diff.total_milliseconds() / 1000) * 100;
      avpcpu    = 1.*(utime + stime + cutime + cstime)/ticksPerSecond / runtime * 100;
      std::cout << "Percent CPU usage: " << pcpu << std::endl;
    }
    time_stamp = now;
  } else {
    time_stamp = boost::posix_time::not_a_date_time;
    utime     = 0;
    stime     = 0;
    cutime    = 0;
    cstime    = 0;
    startTime = 0;
    priority  = 0;
    nice      = 0;
    rss       = 0;
    pcpu      = 0;
    runtime   = 0;
  }
  utime    .write();
  stime    .write();
  cutime   .write();
  cstime   .write();
  startTime.write();
  priority .write();
  nice     .write();
  rss      .write();
  pcpu     .write();
  runtime  .write();
}

#endif


