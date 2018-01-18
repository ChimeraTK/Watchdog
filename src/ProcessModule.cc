/*
 * ProcessModule.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "ProcessModule.h"
#include <signal.h>
#include <chrono>
#include <ctime>

void ProcessInfoModule::mainLoop(){
  processPID = getpid();
  processPID.write();
  while(true) {
    trigger.read();
    try{
      FillProcInfo(proc_util::getInfo(processPID));
    } catch (std::runtime_error &e) {
      logging << this << "Failed to read process information for process " << processPID;
      sendMessage(LogLevel::ERROR);
    }
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
      mem = std::stoi(std::to_string(info->vm_rss));

      sysUpTime.read();
      ticksPerSecond.read();
      runtime = std::stoi(std::to_string(sysUpTime - std::stoi(std::to_string(info->start_time)) * 1. / ticksPerSecond));
    } catch(std::exception &e) {
      logging << this << "FillProcInfo::Conversion failed: " << e.what();
      sendMessage(LogLevel::ERROR);
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
    mem       = 0;
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
  mem      .write();
}

void ProcessControlModule::mainLoop() {
  SetOffline();
  processRestarts = 0;
  processRestarts.write();

  // check for left over processes reading the persist file
  processSetPath.read();
  processSetCMD.read();
  try{
    process.reset(new ProcessHandler("", getName(), false, processPID));
    if(processPID > 0){
      logging << this << "Found process that is still running.";
      sendMessage(LogLevel::INFO);
      SetOnline(processPID);
    }
  } catch (std::runtime_error &e){
    logging << this << " Failed to check for existing processes. Message:\n" << e.what();
    sendMessage(LogLevel::ERROR);
  }
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

    /*
     * don't do anything in case failed more than 4 times
     * or if the server was restarted more than 100 times
     * -> to reset turn off/on the process
     */
    if(processNFailed > 4 || processRestarts > 100) {
      usleep(200000);
      continue;
    }

    if(processPID > 0 && enableProcess) {
      CheckIsOnline(processPID);
    }

    if(enableProcess) {
      if(processPID < 0) {
        try {
          processSetLogfile.read();
          processSetPath.read();
          processSetCMD.read();
          logging << this << "Trying to start a new process: " << (std::string)processSetPath << "/" << (std::string)processSetCMD;
          sendMessage(LogLevel::INFO);
          process.reset(new ProcessHandler("", getName()));
          SetOnline(
              process->startProcess((std::string) processSetPath,
                  (std::string) processSetCMD, (std::string)processSetLogfile));
          processNChilds = proc_util::getNChilds(processPID);
          processNChilds.write();
        } catch(std::runtime_error &e) {
          logging << this << e.what();
          sendMessage(LogLevel::ERROR);
          Failed();
          SetOffline();
        }
      } else {
#ifdef DEBUG
        std::cout << this << "Process is running..." << processIsRunning << " PID: " << getpid() << std::endl;
#endif
        pidOffset.read();
        try{
          FillProcInfo(proc_util::getInfo(processPID + pidOffset));
        } catch (std::runtime_error &e){
          logging << this << "Failed to read information for process " << (processPID + pidOffset) <<
              ". Check if pidOffset is set correctly!";
          sendMessage(LogLevel::ERROR);
        }
      }
    } else {
      if(processPID < 0) {
        processIsRunning = 0;
        processIsRunning.write();

        logging << this << "Process Running: " << processIsRunning << ". Process is not running...OK" << " PID: " << getpid();
        sendMessage(LogLevel::DEBUG);
      } else {
        logging << this << "Trying to kill the process..." << " PID: "
            << processPID;
        sendMessage(LogLevel::INFO);
        killSig.read();
        process->setSigNum(killSig);
        process.reset(nullptr);
        usleep(200000);
        SetOffline();
      }
    }
  }
}

void ProcessControlModule::SetOnline(const int &pid){
  usleep(100000);
  CheckIsOnline(pid);
  if(processIsRunning == 1){
    processPID = pid;
    processPID.write();
    processPath = (std::string)processSetPath;
    processPath.write();
    processCMD = (std::string)processSetCMD;
    processCMD.write();
    processLogfile = (std::string)processSetLogfile;
    processLogfile.write();
    logging << this << "Ok process is started successfully";
//    sendMessage(LogLevel::INFO);
  } else {
    SetOffline();
    logging << this
        << "Failed to start process " << (std::string)processSetPath << "/" << (std::string)processSetCMD;
    sendMessage(LogLevel::ERROR);
    Failed();
  }
}

void ProcessControlModule::SetOffline(){
  processPID = -1;
  processPID.write();
  processIsRunning = 0;
  processIsRunning.write();
  processPath = "";
  processPath.write();
  processCMD = "";
  processCMD.write();
  processLogfile = "";
  processLogfile.write();
  FillProcInfo(nullptr);
}

void ProcessControlModule::Failed(){
  processNFailed = processNFailed + 1;
  processNFailed.write();
  if(processNFailed == 5){
    logging << this <<  "Failed to start the process " << (std::string)processSetPath << "/" << (std::string)processSetCMD << " 5 times."
        << " It will not be started again until you reset the process by switching it off and on again.";
    sendMessage(LogLevel::ERROR);
  }
  sleep(2);
}

void ProcessControlModule::CheckIsOnline(const int pid){
  if(!proc_util::isProcessRunning(pid)) {
    SetOffline();
    logging << this
        << "Child process with PID " << processPID << " is not running, but it should run!";
    sendMessage(LogLevel::ERROR);
    processRestarts += 1;
    processRestarts.write();
    if(processRestarts > 100){
      logging << this
            << "The process " << (std::string)processSetPath << "/" << (std::string)processSetCMD << " was restarted 100 times."
            << " It will not be started again until you reset the process by switching it off and on again.";
      sendMessage(LogLevel::ERROR);
    }

  } else {
    processIsRunning = 1;
    processIsRunning.write();
  }
}

void ProcessInfoModule::sendMessage(const LogLevel &level){
  message = logging.str();
  message.write();
  messageLevel = level;
  messageLevel.write();
  logging.str("");
}

std::stringstream& operator<<(std::stringstream &ss, const ProcessInfoModule* module){
  std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
  time_t t = std::chrono::system_clock::to_time_t(tp);
  const char * tc = ctime(&t);
  std::string str = std::string {tc};
  str.pop_back();
  ss << "WATCHDOG_SERVER: " << str << " " << module->getName() << " -> ";
  return ss;
}
