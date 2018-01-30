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
    try{
      FillProcInfo(proc_util::getInfo(processPID));
      logging << getTime() << "Process is running (PID: " << processPID << ")";
      sendMessage(logging::LogLevel::DEBUG);
    } catch (std::runtime_error &e) {
      logging << getTime() << "Failed to read process information for process " << processPID;
      sendMessage(logging::LogLevel::ERROR);
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
      logging << getTime() << "FillProcInfo::Conversion failed: " << e.what();
      sendMessage(logging::LogLevel::ERROR);
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
  std::stringstream handlerMessage;
  try{
    process.reset(new ProcessHandler("", getName(), false, processPID, handlerMessage));
    evaluateMessage(handlerMessage);
    if(processPID > 0){
      logging << getTime() << "Found process that is still running.";
      sendMessage(logging::LogLevel::INFO);
      SetOnline(processPID);
    }
  } catch (std::runtime_error &e){
    logging << getTime() << " Failed to check for existing processes. Message:\n" << e.what();
    sendMessage(logging::LogLevel::ERROR);
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
          processSetExternalLogfile.read();
          processSetPath.read();
          processSetCMD.read();
          logging << getTime() << "Trying to start a new process: " << (std::string)processSetPath << "/" << (std::string)processSetCMD;
          sendMessage(logging::LogLevel::INFO);
          // log level of the process handler is DEBUG per default. So all messages will end up here
          process.reset(new ProcessHandler("", getName(), false, handlerMessage, this->getName()));
          SetOnline(
              process->startProcess((std::string) processSetPath,
                  (std::string) processSetCMD, (std::string)processSetExternalLogfile));
          evaluateMessage(handlerMessage);
          processNChilds = proc_util::getNChilds(processPID);
          processNChilds.write();
        } catch(std::runtime_error &e) {
          logging << getTime() << e.what();
          sendMessage(logging::LogLevel::ERROR);
          Failed();
          SetOffline();
        }
      } else {
        logging << getTime() << "Process is running..." << processIsRunning << " PID: " << processPID;
        sendMessage(logging::LogLevel::DEBUG);
        pidOffset.read();
        try{
          FillProcInfo(proc_util::getInfo(processPID + pidOffset));
        } catch (std::runtime_error &e){
          logging << getTime() << "Failed to read information for process " << (processPID + pidOffset) <<
              ". Check if pidOffset is set correctly!";
          sendMessage(logging::LogLevel::ERROR);
        }
      }
    } else {
      if(processPID < 0) {
        processIsRunning = 0;
        processIsRunning.write();
        logging << getTime() << "Process Running: " << processIsRunning << ". Process is not running...OK";
        sendMessage(logging::LogLevel::DEBUG);
      } else {
        logging << getTime() << "Trying to kill the process..." << " PID: "
            << processPID;
        sendMessage(logging::LogLevel::INFO);
        killSig.read();
        process->setSigNum(killSig);
        process.reset(nullptr);
        evaluateMessage(handlerMessage);
        usleep(200000);
        SetOffline();
      }
    }
  }
}

void ProcessControlModule::SetOnline(const int &pid){
  usleep(100000);
  // set external log file in order to read the log file even if starting the process failed
  processExternalLogfile = (std::string)processSetExternalLogfile;
  processExternalLogfile.write();
  CheckIsOnline(pid);
  if(processIsRunning == 1){
    processPID = pid;
    processPID.write();
    processPath = (std::string)processSetPath;
    processPath.write();
    processCMD = (std::string)processSetCMD;
    processCMD.write();
    logging << getTime() << "Ok process is started successfully";
    sendMessage(logging::LogLevel::INFO);
  } else {
    SetOffline();
    logging << getTime()
        << "Failed to start process " << (std::string)processSetPath << "/" << (std::string)processSetCMD;
    sendMessage(logging::LogLevel::ERROR);
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
  /* Don't reset the log file name since the Logger might still try to access it.
     It will be updated once a new process is started!
  processLogfile = "";
  processLogfile.write();
  */
  FillProcInfo(nullptr);
}

void ProcessControlModule::Failed(){
  processNFailed = processNFailed + 1;
  processNFailed.write();
  if(processNFailed == 5){
    logging << getTime() <<  "Failed to start the process " << (std::string)processSetPath << "/" << (std::string)processSetCMD << " 5 times."
        << " It will not be started again until you reset the process by switching it off and on again.";
    sendMessage(logging::LogLevel::ERROR);
  }
  sleep(2);
}

void ProcessControlModule::CheckIsOnline(const int pid){
  if(!proc_util::isProcessRunning(pid)) {
    SetOffline();
    logging << getTime()
        << "Child process with PID " << processPID << " is not running, but it should run!";
    sendMessage(logging::LogLevel::ERROR);
    processRestarts += 1;
    processRestarts.write();
    if(processRestarts > 100){
      logging << getTime()
            << "The process " << (std::string)processSetPath << "/" << (std::string)processSetCMD << " was restarted 100 times."
            << " It will not be started again until you reset the process by switching it off and on again.";
      sendMessage(logging::LogLevel::ERROR);
    }

  } else {
    processIsRunning = 1;
    processIsRunning.write();
  }
}

void ProcessInfoModule::sendMessage(const logging::LogLevel &level){
  message = logging.str();
  message.write();
  messageLevel = level;
  messageLevel.write();
  logging.clear();
  logging.str("");
}

void ProcessInfoModule::evaluateMessage(std::stringstream &msg){
  auto list = logging::stripMessages(msg);
  for(auto &message : list){
    logging << getTime() << message.message.str();
    sendMessage(message.logLevel);
  }
  msg.clear();
  msg.str("");
}

std::string ProcessInfoModule::getTime(){
  std::string str{"WATCHDOG_SERVER: "};
  str.append(logging::getTime());
  str.append(this->getName());
  str.append(" -> ");
  return str;
}

void ProcessControlModule::terminate(){
  process.reset(nullptr);
  ProcessInfoModule::terminate();
}
