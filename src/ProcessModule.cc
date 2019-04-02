/*
 * ProcessModule.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "ProcessModule.h"
#include <signal.h>

#include "boost/date_time/posix_time/posix_time.hpp"

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
      //\FixMe: Exception is thrown on server start -> find out why.
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

      maxMem.read();
      memoryUsage = 1.*mem/maxMem*100.;

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
          / (1.*diff.total_milliseconds() / 1000) * 100;
      avpcpu = 1. * (utime + stime + cutime + cstime) / ticksPerSecond / runtime
          * 100;
    }
    time_stamp = now;
  } else {
    time_stamp   = boost::posix_time::not_a_date_time;
    utime        = 0;
    stime        = 0;
    cutime       = 0;
    cstime       = 0;
    startTime    = 0;
    startTimeStr = "";
    priority     = 0;
    nice         = 0;
    rss          = 0;
    pcpu         = 0;
    avpcpu       = 0;
    runtime      = 0;
    mem          = 0;
    memoryUsage  = 0.;
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
  memoryUsage.write();
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
#ifdef ENABLE_LOGGING
  logging = new std::stringstream();
  std::stringstream handlerMessage;
#else
  logging = &std::cerr;
#endif
  (*logging) << getTime() << "New ProcessModule started!" << std::endl;
#ifdef ENABLE_LOGGING
  sendMessage(logging::LogLevel::INFO);
#endif
  SetOffline();
  processRestarts = 0;
  processRestarts.write();

  // check for left over processes reading the persist file
  processSetPath.read();
  processSetCMD.read();
  bootDelay.read();

  try{
#ifdef ENABLE_LOGGING
    process.reset(new ProcessHandler(getName(), false, processPID, handlerMessage));
    evaluateMessage(handlerMessage);
#else
    process.reset(new ProcessHandler(getName(), false, processPID, std::cout));
#endif
    if(processPID > 0){
      (*logging) << getTime() << "Found process that is still running. PID is: " << processPID << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::INFO);
#endif
      SetOnline(processPID);
    } else {
      if(bootDelay > 0){
        (*logging) << getTime() << "Process sleeping before starting main loop. Delay: " << bootDelay << "s." <<  std::endl;
#ifdef ENABLE_LOGGING
        sendMessage(logging::LogLevel::INFO);
#endif
        sleep(bootDelay);
      }
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
    processMaxFails.read();
    processMaxRestarts.read();
    // reset number of failed tries and restarts in case the process is set offline
    if(!enableProcess) {
      processNFailed = 0;
      processNFailed.write();
      processRestarts = 0;
      processRestarts.write();
      _stop = false;
      _restartRequired = false;
    }

    /*
     * don't do anything in case failed more than the user set limit
     * or if the server was restarted more than the user set limit
     * -> to reset turn off/on the process
     */
    if(_stop) {
      (*logging) << getTime() << "Process sleeping. Fails: " << processNFailed << "/" << processMaxFails
          << ", Restarts: " << processRestarts << "/" << processMaxRestarts << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::DEBUG);
#endif
      if(_historyOn)
        FillProcInfo(nullptr);
      continue;
    }

    /**
     * Check if restart is required
     */
    if(processPID > 0 && enableProcess) {
      CheckIsOnline(processPID);
      if(!processIsRunning){
        if(processMaxRestarts == 0){
          _stop = true;
          (*logging) << getTime() << "Maximum number of restarts is 0. Process will not be started again." << std::endl;
#ifdef ENABLE_LOGGING
          sendMessage(logging::LogLevel::WARNING);
#endif
          _restartRequired = false;
        } else {
          _restartRequired = true;
        }
      }
    }

    /**
     * If starting a process fails and max restarts is 0 stop.
     */
    if(processMaxRestarts == 0 && _restartRequired){
      _restartRequired = false;
      _stop = true;
    }

    /**
     * Check number of restarts in case it is set
     */
    if(processMaxRestarts != 0 && processRestarts == processMaxRestarts){
      (*logging) << getTime() << "Maximum number of restarts reached. Restarts: " << processRestarts << "/" << processMaxRestarts << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::DEBUG);
#endif
      // Only stop if the process terminated. This ensures that the process status is updated.
      if(processIsRunning == 0){
        _stop = true;
        (*logging) << getTime() << "Process terminated after maximum number of restarts reached. Restarts: " << processRestarts << "/" << processMaxRestarts << std::endl;
#ifdef ENABLE_LOGGING
        sendMessage(logging::LogLevel::ERROR);
        resetProcessHandler(&handlerMessage);
#else
        resetProcessHandler(nullptr);
#endif
      } else {
        _restartRequired = false;
      }
    }

    if(enableProcess) {
      // process should run
      if(processPID < 0 && !_stop && (processRestarts < processMaxRestarts || processMaxRestarts == 0)) {
        // process should run and is not running
        if(_restartRequired){
          processRestarts += 1;
          processRestarts.write();
          _restartRequired = false;
        }
        // fill 0 since the process is started here and not running yet
        if(_historyOn)
          FillProcInfo(nullptr);
        try {
          processSetPath.read();
          processSetCMD.read();
          processSetENV.read();
          processOverwriteENV.read();
#ifdef ENABLE_LOGGING
          processSetExternalLogfile.read();
          (*logging) << getTime() << "Trying to start a new process: " << (std::string)processSetPath << "/" << (std::string)processSetCMD << std::endl;
          sendMessage(logging::LogLevel::INFO);
          // log level of the process handler is DEBUG per default. So all messages will end up here
          process.reset(new ProcessHandler(getName(), false, handlerMessage, this->getName()));
#else
          process.reset(new ProcessHandler(getName(), false, std::cout, this->getName()));
#endif


#ifdef ENABLE_LOGGING
          SetOnline(
              process->startProcess((std::string) processSetPath,
                  (std::string) processSetCMD, (std::string)processSetExternalLogfile, (std::string)processSetENV, processOverwriteENV));
          evaluateMessage(handlerMessage);
          processNChilds = proc_util::getNChilds(processPID, handlerMessage);
          sendMessage(logging::LogLevel::DEBUG);
#else
          SetOnline(
          process->startProcess((std::string) processSetPath,
              (std::string) processSetCMD, std::string(""), (std::string)processSetENV,processOverwriteENV));
          processNChilds = proc_util::getNChilds(processPID);
#endif
          processNChilds.write();
        } catch(std::runtime_error &e) {
          (*logging) << getTime() << e.what() << std::endl;
#ifdef ENABLE_LOGGING
          sendMessage(logging::LogLevel::ERROR);
#endif
          Failed();
          SetOffline();
        }
      } else if (processPID > 0){
        // process should run and is running
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
      // process should not run
      if(processPID < 0) {
        // process should not run and is not running
        processIsRunning = 0;
        processIsRunning.write();
#ifdef ENABLE_LOGGING
        (*logging) << getTime() << "Process Running: " << processIsRunning << ". Process is not running...OK" << std::endl;
        sendMessage(logging::LogLevel::DEBUG);
#endif
      if(_historyOn)
        FillProcInfo(nullptr);
      } else {
        // process should not run and is running
#ifdef ENABLE_LOGGING
        (*logging) << getTime() << "Trying to kill the process..." << " PID: "
            << processPID << std::endl;
        sendMessage(logging::LogLevel::INFO);
#endif
        // Here the process is stopped in case enableProcess is set to 0. If it is already reset due to restart stop don't do anything here
        if(process.get() != nullptr){
#ifdef ENABLE_LOGGING
          resetProcessHandler(&handlerMessage);
#else
          resetProcessHandler(nullptr);
#endif
        }
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
    processENV = (std::string)processSetENV;
    processENV.write();
#ifdef ENABLE_LOGGING
    (*logging) << getTime() << "Ok process is started successfully with PID: " << processPID << std::endl;
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
  processENV = "";
  processENV.write();
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
  if(!_stop && processNFailed == processMaxFails){
    _stop = true;
    _restartRequired = false;
    (*logging) << getTime() <<  "Failed to start the process " << (std::string)processSetPath << "/" << (std::string)processSetCMD << " " << processMaxFails << " times."
        << " It will not be started again until you reset the process by switching it off and on again." << std::endl;
#ifdef ENABLE_LOGGING
    sendMessage(logging::LogLevel::ERROR);
#endif
  } else {
    _restartRequired = true;
  }
}

void ProcessControlModule::CheckIsOnline(const int pid){
#ifdef ENABLE_LOGGING
        (*logging) << getTime() << "Checking process status for process: " << pid << std::endl;
        sendMessage(logging::LogLevel::DEBUG);
#endif
  if(!proc_util::isProcessRunning(pid)) {
    (*logging) << getTime()
        << "Child process with PID " << processPID << " is not running, but it should run!" << std::endl;
    SetOffline();
#ifdef ENABLE_LOGGING
    sendMessage(logging::LogLevel::ERROR);
#endif
  } else {
    processIsRunning = 1;
    processIsRunning.write();
  }
}


void ProcessControlModule::resetProcessHandler(std::stringstream* handlerMessage){
  killSig.read();
  //ToDo: Set default to 2!
  if(killSig < 1)
    process->setSigNum(2);
  else
    process->setSigNum(killSig);
  killTimeout.read();
  process->setKillTimeout(killTimeout);
  process.reset(nullptr);
#ifdef ENABLE_LOGGING
  evaluateMessage(*handlerMessage);
#endif
}

void ProcessControlModule::terminate(){
  if(process != nullptr)
    process->Disconnect();
  process.reset(nullptr);
  ProcessInfoModule::terminate();
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
    (*logging) << message.message.str() << std::endl;
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

