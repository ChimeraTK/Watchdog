/*
 * ProcessModule.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "ProcessModule.h"
#include <signal.h>

// This symbol is introduced by procps and in boost 1.71 a function likely is used!
#undef likely
#include "boost/date_time/posix_time/posix_time.hpp"

void ProcessInfoModule::mainLoop(){
  info.processPID = getpid();
  info.processPID.write();
#ifdef ENABLE_LOGGING
  logStream = new std::stringstream();
#else
  logStream = &std::cerr;
#endif
  while(true) {
    trigger.read();
    try{
      FillProcInfo(proc_util::getInfo(info.processPID));
#ifdef ENABLE_LOGGING
      (*logStream) << getTime() << "Process is running (PID: " << info.processPID << ")" << std::endl;
      sendMessage(logging::LogLevel::DEBUG);
#endif
    } catch (std::runtime_error &e) {
      (*logStream) << getTime() << "Failed to read process information for process " << info.processPID << std::endl;
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
      old_time = std::stoi(std::to_string(statistics.utime + statistics.stime + statistics.cutime + statistics.cstime));
      statistics.utime = std::stoi(std::to_string(info->utime));
      statistics.stime = std::stoi(std::to_string(info->stime));
      statistics.cutime = std::stoi(std::to_string(info->cutime));
      statistics.cstime = std::stoi(std::to_string(info->cstime));

      input.readAll();

      // info->start_time reads s since system was started
      int relativeStartTime = 1. * std::stoi(std::to_string(info->start_time)) / input.ticksPerSecond;
      statistics.startTime = input.sysStartTime + relativeStartTime;
      statistics.startTimeStr = boost::posix_time::to_simple_string(boost::posix_time::from_time_t(statistics.startTime));
      statistics.priority = std::stoi(std::to_string(info->priority));
      statistics.nice = std::stoi(std::to_string(info->nice));
      statistics.rss = std::stoi(std::to_string(info->rss));
      statistics.mem = std::stoi(std::to_string(info->vm_rss));

      statistics.memoryUsage = 1.*statistics.mem/input.maxMem*100.;

      statistics.runtime = std::stoi(std::to_string(input.sysUpTime - std::stoi(std::to_string(info->start_time)) * 1. / input.ticksPerSecond));
    } catch(std::exception &e) {
      (*logStream) << getTime() << "FillProcInfo::Conversion failed: " << e.what() << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::ERROR);
#endif
    }
    // check if it is the first call after process is started (time_stamp  == not_a_date_time)
    if(!time_stamp.is_special()) {
      boost::posix_time::time_duration diff = now - time_stamp;
      statistics.pcpu = 1. * (statistics.utime + statistics.stime + statistics.cutime + statistics.cstime - old_time) / input.ticksPerSecond
          / (1.*diff.total_milliseconds() / 1000) * 100;
      statistics.avpcpu = 1. * (statistics.utime + statistics.stime + statistics.cutime + statistics.cstime) / input.ticksPerSecond / statistics.runtime
          * 100;
    }
    time_stamp = now;
  } else {
    time_stamp   = boost::posix_time::not_a_date_time;
    statistics.utime        = 0;
    statistics.stime        = 0;
    statistics.cutime       = 0;
    statistics.cstime       = 0;
    statistics.startTime    = 0;
    statistics.startTimeStr = "";
    statistics.priority     = 0;
    statistics.nice         = 0;
    statistics.rss          = 0;
    statistics.pcpu         = 0;
    statistics.avpcpu       = 0;
    statistics.runtime      = 0;
    statistics.mem          = 0;
    statistics.memoryUsage  = 0.;
  }
  statistics.writeAll();
}

void ProcessInfoModule::terminate(){
  ApplicationModule::terminate();
#ifdef ENABLE_LOGGING
  if(logStream != nullptr)
    delete logStream;
  logStream = 0;
#endif
}

void ProcessControlModule::mainLoop() {
#ifdef ENABLE_LOGGING
  logStream = new std::stringstream();
  std::stringstream handlerMessage;
#else
  logStream = &std::cerr;
#endif
  (*logStream) << getTime() << "New ProcessModule started!" << std::endl;
#ifdef ENABLE_LOGGING
  sendMessage(logging::LogLevel::INFO);
#endif
  SetOffline();
  status.nRestarts = 0;
  status.nRestarts.write();

  // check for left over processes reading the persist file
  config.path.read();
  config.cmd.read();
  config.bootDelay.read();

  try{
#ifdef ENABLE_LOGGING
    process.reset(new ProcessHandler(getName(), false, info.processPID, handlerMessage));
    evaluateMessage(handlerMessage);
#else
    process.reset(new ProcessHandler(getName(), false, info.processPID, std::cout));
#endif
    if(info.processPID > 0){
      (*logStream) << getTime() << "Found process that is still running. PID is: " << info.processPID << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::INFO);
#endif
      SetOnline(info.processPID);
    } else {
      if(config.bootDelay > 0){
        (*logStream) << getTime() << "Process sleeping before starting main loop. Delay: " << config.bootDelay << "s." <<  std::endl;
#ifdef ENABLE_LOGGING
        sendMessage(logging::LogLevel::INFO);
#endif
        sleep(config.bootDelay);
      }
    }
  } catch (std::runtime_error &e){
    (*logStream) << getTime() << " Failed to check for existing processes. Message:\n" << e.what() << std::endl;
#ifdef ENABLE_LOGGING
    sendMessage(logging::LogLevel::ERROR);
#endif
  }

  while(true) {
    trigger.read();
    enableProcess.read();
    config.maxFails.read();
    config.maxRestarts.read();
    config.alias.read();
    // reset number of failed tries and restarts in case the process is set offline
    if(!enableProcess) {
      status.nFailed = 0;
      status.nFailed.write();
      status.nRestarts = 0;
      status.nRestarts.write();
      _stop = false;
      _restartRequired = false;
    }

    /*
     * don't do anything in case failed more than the user set limit
     * or if the server was restarted more than the user set limit
     * -> to reset turn off/on the process
     */
    if(_stop) {
      (*logStream) << getTime() << "Process sleeping. Fails: " << status.nFailed << "/" << config.maxFails
          << ", Restarts: " << status.nRestarts << "/" << config.maxRestarts << std::endl;
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
    if(info.processPID > 0 && enableProcess) {
      CheckIsOnline(info.processPID);
      if(!status.isRunning){
        if(config.maxRestarts == 0){
          _stop = true;
          (*logStream) << getTime() << "Maximum number of restarts is 0. Process will not be started again." << std::endl;
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
    if(config.maxRestarts == 0 && _restartRequired){
      _restartRequired = false;
      _stop = true;
    }

    /**
     * Check number of restarts in case it is set
     */
    if(config.maxRestarts != 0 && status.nRestarts == config.maxRestarts){
      (*logStream) << getTime() << "Maximum number of restarts reached. Restarts: " << status.nRestarts << "/" << config.maxRestarts << std::endl;
#ifdef ENABLE_LOGGING
      sendMessage(logging::LogLevel::DEBUG);
#endif
      // Only stop if the process terminated. This ensures that the process status is updated.
      if(status.isRunning == 0){
        _stop = true;
        (*logStream) << getTime() << "Process terminated after maximum number of restarts reached. Restarts: " << status.nRestarts << "/" << config.maxRestarts << std::endl;
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
      if(info.processPID < 0 && !_stop && (status.nRestarts < config.maxRestarts || config.maxRestarts == 0)) {
        // process should run and is not running
        if(_restartRequired){
          status.nRestarts += 1;
          status.nRestarts.write();
          _restartRequired = false;
        }
        // fill 0 since the process is started here and not running yet
        if(_historyOn)
          FillProcInfo(nullptr);
        try {
          config.path.read();
          config.cmd.read();
          config.env.read();
          config.overwriteEnv.read();
#ifdef ENABLE_LOGGING
          config.externalLogfile.read();
          (*logStream) << getTime() << "Trying to start a new process: " << (std::string)config.path << "/" << (std::string)config.cmd << std::endl;
          sendMessage(logging::LogLevel::INFO);
          // log level of the process handler is DEBUG per default. So all messages will end up here
          process.reset(new ProcessHandler(getName(), false, handlerMessage, this->getName()));
#else
          process.reset(new ProcessHandler(getName(), false, std::cout, this->getName()));
#endif


#ifdef ENABLE_LOGGING
          SetOnline(
              process->startProcess((std::string) config.path,
                  (std::string) config.cmd, (std::string)config.externalLogfile, (std::string)config.env, config.overwriteEnv));
          evaluateMessage(handlerMessage);
          status.nChilds = proc_util::getNChilds(info.processPID, handlerMessage);
          sendMessage(logging::LogLevel::DEBUG);
#else
          SetOnline(
          process->startProcess((std::string) config.path,
              (std::string) config.cmd, std::string(""), (std::string)config.env,config.overwriteEnv));
          status.nChilds = proc_util::getNChilds(info.processPID);
#endif
          status.nChilds.write();
        } catch(std::runtime_error &e) {
          (*logStream) << getTime() << e.what() << std::endl;
#ifdef ENABLE_LOGGING
          sendMessage(logging::LogLevel::ERROR);
#endif
          Failed();
          SetOffline();
        }
      } else if (info.processPID > 0){
        // process should run and is running
#ifdef ENABLE_LOGGING
        (*logStream) << getTime() << "Process is running..." << status.isRunning << " PID: " << info.processPID << std::endl;
        sendMessage(logging::LogLevel::DEBUG);
#endif
        config.pidOffset.read();
        try{
          FillProcInfo(proc_util::getInfo(info.processPID + config.pidOffset));
        } catch (std::runtime_error &e){
          (*logStream) << getTime() << "Failed to read information for process " << (info.processPID + config.pidOffset) <<
              ". Check if pidOffset is set correctly!" << std::endl;
#ifdef ENABLE_LOGGING
          sendMessage(logging::LogLevel::ERROR);
#endif
        }
      }
    } else {
      // process should not run
      if(info.processPID < 0) {
        // process should not run and is not running
        status.isRunning = 0;
        status.isRunning.write();
#ifdef ENABLE_LOGGING
        (*logStream) << getTime() << "Process Running: " << status.isRunning << ". Process is not running...OK" << std::endl;
        sendMessage(logging::LogLevel::DEBUG);
#endif
      if(_historyOn)
        FillProcInfo(nullptr);
      } else {
        // process should not run and is running
#ifdef ENABLE_LOGGING
        (*logStream) << getTime() << "Trying to kill the process..." << " PID: "
            << info.processPID << std::endl;
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
  status.externalLogfile = (std::string)config.externalLogfile;
  status.externalLogfile.write();
#endif
  CheckIsOnline(pid);
  if(status.isRunning == 1){
    info.processPID = pid;
    info.processPID.write();
    status.path = (std::string)config.path;
    status.path.write();
    status.cmd = (std::string)config.cmd;
    status.cmd.write();
    status.env = (std::string)config.env;
    status.env.write();
#ifdef ENABLE_LOGGING
    (*logStream) << getTime() << "Ok process is started successfully with PID: " << info.processPID << std::endl;
    sendMessage(logging::LogLevel::INFO);
#endif
  } else {
    SetOffline();
    (*logStream) << getTime()
        << "Failed to start process " << (std::string)config.path << "/" << (std::string)config.cmd << std::endl;
#ifdef ENABLE_LOGGING
    sendMessage(logging::LogLevel::ERROR);
#endif
    Failed();
  }
}

void ProcessControlModule::SetOffline(){
  info.processPID = -1;
  info.processPID.write();
  status.isRunning = 0;
  status.isRunning.write();
  status.path = "";
  status.path.write();
  status.cmd = "";
  status.cmd.write();
  status.env = "";
  status.env.write();
  /* Don't reset the log file name since the Logger might still try to access it.
     It will be updated once a new process is started!
  processLogfile = "";
  processLogfile.write();
  */
  FillProcInfo(nullptr);
}

void ProcessControlModule::Failed(){
  status.nFailed = status.nFailed + 1;
  status.nFailed.write();
  if(!_stop && status.nFailed == config.maxFails){
    _stop = true;
    _restartRequired = false;
    (*logStream) << getTime() <<  "Failed to start the process " << (std::string)config.path << "/" << (std::string)config.cmd << " " << config.maxFails << " times."
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
        (*logStream) << getTime() << "Checking process status for process: " << pid << std::endl;
        sendMessage(logging::LogLevel::DEBUG);
#endif
  if(!proc_util::isProcessRunning(pid)) {
    (*logStream) << getTime()
        << "Child process with PID " << info.processPID << " is not running, but it should run!" << std::endl;
    SetOffline();
#ifdef ENABLE_LOGGING
    sendMessage(logging::LogLevel::ERROR);
#endif
  } else {
    status.isRunning = 1;
    status.isRunning.write();
  }
}


void ProcessControlModule::resetProcessHandler(std::stringstream* handlerMessage){
  config.killSig.read();
  //ToDo: Set default to 2!
  if(config.killSig < 1)
    process->setSigNum(2);
  else
    process->setSigNum(config.killSig);
  config.killTimeout.read();
  process->setKillTimeout(config.killTimeout);
  process.reset(nullptr);
#ifdef ENABLE_LOGGING
  evaluateMessage(*handlerMessage);
#endif
}

void ProcessControlModule::terminate(){
  if(process != nullptr){
#ifdef ENABLE_LOGGING
    if(info.processPID > 0){
      (*logStream) << getTime() << "Process " << getName() << " is disconnected. It is no longer controlled by the ProcessHandler!"
          " You have to take care of it on your own. PID is: " << info.processPID << std::endl;
      sendMessage(logging::LogLevel::INFO);
    }
#endif
    process->Disconnect();
  }
  process.reset(nullptr);
  ProcessInfoModule::terminate();
}

#ifdef ENABLE_LOGGING
void ProcessInfoModule::sendMessage(const logging::LogLevel &level){
  auto logging_ss = dynamic_cast<std::stringstream*>(logStream);
  if(logging_ss){
    logging.message = logging_ss->str();
    logging.messageLevel = level;
    logging.writeAll();
    logging_ss->clear();
    logging_ss->str("");
  }
}

void ProcessInfoModule::evaluateMessage(std::stringstream &msg){
  auto list = logging::stripMessages(msg);
  for(auto &message : list){
    (*logStream) << message.message.str() << std::endl;
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

std::string ProcessControlModule::getTime(){
  std::string str{"WATCHDOG_SERVER: "};
  str.append(logging::getTime());
  str.append(this->getOwner()->getName());
  str.append("/");
  str.append(this->getName());
  if(!((std::string)(config.alias)).empty()){
    str.append(" (alias: " + (std::string)config.alias + ")");
  }
  str.append(" -> ");
  return str;
}
