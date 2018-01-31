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
#ifdef ENABLE_LOGGING
  logging = new std::stringstream();
#else
  logging = &std::cerr;
#endif
  while(true) {
    trigger.read();
    try{
      FillProcInfo(proc_util::getInfo(processPID));
#ifdef ENABLE_LOGGING
      (*logging) << getTime() << "Process is running (PID: " << processPID << ")" << std::endl;
      sendMessage(logging::LogLevel::DEBUG);
#endif
    } catch (std::runtime_error &e) {
      (*logging) << getTime() << "Failed to read process information for process " << processPID << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::ERROR);
#endif
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
      (*logging) << getTime() << "FillProcInfo::Conversion failed: " << e.what() << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::ERROR);
#endif
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

void ProcessInfoModule::terminate(){
  ApplicationModule::terminate();
#ifdef ENABLE_LOGGING
  if(logging != nullptr)
    delete logging;
  logging = 0;
#endif
}

void ProcessControlModule::mainLoop() {
  SetOffline();
  processRestarts = 0;
  processRestarts.write();

  // check for left over processes reading the persist file
  processSetPath.read();
  processSetCMD.read();

#ifdef ENABLE_LOGGING
  logging = new std::stringstream();
  std::stringstream handlerMessage;
#else
  logging = &std::cerr;
#endif
  try{
#ifdef ENABLE_LOGGING
    process.reset(new ProcessHandler("", getName(), false, processPID, handlerMessage));
    evaluateMessage(handlerMessage);
#else
    process.reset(new ProcessHandler("", getName(), false, processPID, std::cout));
#endif
    if(processPID > 0){
      (*logging) << getTime() << "Found process that is still running. PID is: " << processPID << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::INFO);
#endif
      SetOnline(processPID);
    }
  } catch (std::runtime_error &e){
    (*logging) << getTime() << " Failed to check for existing processes. Message:\n" << e.what() << std::endl;
#ifdef ENABLE_LOGGING
    sendMessage(logging::LogLevel::ERROR);
#endif
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
          processSetPath.read();
          processSetCMD.read();
#ifdef ENABLE_LOGGING
          processSetExternalLogfile.read();
          (*logging) << getTime() << "Trying to start a new process: " << (std::string)processSetPath << "/" << (std::string)processSetCMD << std::endl;
          sendMessage(logging::LogLevel::INFO);
          // log level of the process handler is DEBUG per default. So all messages will end up here
          process.reset(new ProcessHandler("", getName(), false, handlerMessage, this->getName()));
#else
          process.reset(new ProcessHandler("", getName(), false, std::cout, this->getName()));
#endif


#ifdef Enable_LOGGING
          SetOnline(
              process->startProcess((std::string) processSetPath,
                  (std::string) processSetCMD, (std::string)processSetExternalLogfile));
          evaluateMessage(handlerMessage);
#else
          SetOnline(
          process->startProcess((std::string) processSetPath,
              (std::string) processSetCMD, std::string("")));

#endif
          processNChilds = proc_util::getNChilds(processPID);
          processNChilds.write();
        } catch(std::runtime_error &e) {
          (*logging) << getTime() << e.what() << std::endl;
#ifdef ENABLE_LOGGING
          sendMessage(logging::LogLevel::ERROR);
#endif
          Failed();
          SetOffline();
        }
      } else {
#ifdef ENABLE_LOGGING
        (*logging) << getTime() << "Process is running..." << processIsRunning << " PID: " << processPID << std::endl;
        sendMessage(logging::LogLevel::DEBUG);
#endif
        pidOffset.read();
        try{
          FillProcInfo(proc_util::getInfo(processPID + pidOffset));
        } catch (std::runtime_error &e){
          (*logging) << getTime() << "Failed to read information for process " << (processPID + pidOffset) <<
              ". Check if pidOffset is set correctly!" << std::endl;
#ifdef ENABLE_LOGGING
          sendMessage(logging::LogLevel::ERROR);
#endif
        }
      }
    } else {
      if(processPID < 0) {
        processIsRunning = 0;
        processIsRunning.write();
#ifdef ENABLE_LOGGING
        (*logging) << getTime() << "Process Running: " << processIsRunning << ". Process is not running...OK" << std::endl;
        sendMessage(logging::LogLevel::DEBUG);
#endif
      } else {
#ifdef ENABLE_LOGGING
        (*logging) << getTime() << "Trying to kill the process..." << " PID: "
            << processPID << std::endl;
        sendMessage(logging::LogLevel::INFO);
#endif
        killSig.read();
        process->setSigNum(killSig);
        process.reset(nullptr);
#ifdef ENABLE_LOGGING
        evaluateMessage(handlerMessage);
#endif
        usleep(200000);
        SetOffline();
      }
    }
  }
}

void ProcessControlModule::SetOnline(const int &pid){
  usleep(100000);
#ifdef ENABLE_LOGGING
  // set external log file in order to read the log file even if starting the process failed
  processExternalLogfile = (std::string)processSetExternalLogfile;
  processExternalLogfile.write();
#endif
  CheckIsOnline(pid);
  if(processIsRunning == 1){
    processPID = pid;
    processPID.write();
    processPath = (std::string)processSetPath;
    processPath.write();
    processCMD = (std::string)processSetCMD;
    processCMD.write();
#ifdef ENABLE_LOGGING
    (*logging) << getTime() << "Ok process is started successfully" << std::endl;
    sendMessage(logging::LogLevel::INFO);
#endif
  } else {
    SetOffline();
    (*logging) << getTime()
        << "Failed to start process " << (std::string)processSetPath << "/" << (std::string)processSetCMD << std::endl;
#ifdef ENABLE_LOGGING
    sendMessage(logging::LogLevel::ERROR);
#endif
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
    (*logging) << getTime() <<  "Failed to start the process " << (std::string)processSetPath << "/" << (std::string)processSetCMD << " 5 times."
        << " It will not be started again until you reset the process by switching it off and on again." << std::endl;
#ifdef ENABLE_LOGGING
    sendMessage(logging::LogLevel::ERROR);
#endif
  }
  sleep(2);
}

void ProcessControlModule::CheckIsOnline(const int pid){
  if(!proc_util::isProcessRunning(pid)) {
    SetOffline();
    (*logging) << getTime()
        << "Child process with PID " << processPID << " is not running, but it should run!" << std::endl;
#ifdef ENABLE_LOGGING
    sendMessage(logging::LogLevel::ERROR);
#endif
    processRestarts += 1;
    processRestarts.write();
    if(processRestarts > 100){
      (*logging) << getTime()
            << "The process " << (std::string)processSetPath << "/" << (std::string)processSetCMD << " was restarted 100 times."
            << " It will not be started again until you reset the process by switching it off and on again." << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::ERROR);
#endif
    }

  } else {
    processIsRunning = 1;
    processIsRunning.write();
  }
}

#ifdef ENABLE_LOGGING
void ProcessInfoModule::sendMessage(const logging::LogLevel &level){
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

void ProcessInfoModule::evaluateMessage(std::stringstream &msg){
  auto list = logging::stripMessages(msg);
  for(auto &message : list){
    (*logging) << getTime() << message.message.str() << std::endl;
    sendMessage(message.logLevel);
  }
  msg.clear();
  msg.str("");
}
#endif

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
