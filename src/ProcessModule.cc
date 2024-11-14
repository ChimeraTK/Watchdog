// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

/*
 * ProcessModule.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: Klaus Zenker (HZDR)
 */

#include "ProcessModule.h"

#ifndef WITH_PROCPS
#  include <libproc2/pids.h>
#endif

#include <signal.h>

// This symbol is introduced by procps and in boost 1.71 a function likely is used!
#undef likely
#include "boost/date_time/posix_time/posix_time.hpp"

ProcessInfoModule::ProcessInfoModule(ctk::ModuleGroup* owner, const std::string& name, const std::string& description,
    const std::unordered_set<std::string>& tags, const std::string& pathToTrigger)
: ctk::ApplicationModule(owner, name, description, tags), trigger(this, pathToTrigger, "", "Trigger input") {
#ifndef WITH_PROCPS
  fatal_proc_unmounted(infoptr, 0);
  if(!infoptr) {
    ctk::runtime_error("ProcessInfoModule::Failed to access proc data.");
  }
  enum pids_item Items[] = {PIDS_ID_PID,
      PIDS_TICS_USER,     // utime
      PIDS_TICS_SYSTEM,   // stime
      PIDS_TICS_USER_C,   // utime+cutime
      PIDS_TICS_SYSTEM_C, // stime+cstime
      PIDS_RSS, PIDS_NICE, PIDS_PRIORITY, PIDS_TIME_START, PIDS_TIME_ELAPSED, PIDS_MEM_RES};

  if(procps_pids_new(&infoptr, Items, 11) < 0) {
    ctk::runtime_error("Failed to prepare procps in ProcessInfoModule.");
  }

  fatal_proc_unmounted(infoptrPID, 0);
  if(!infoptrPID) {
    std::runtime_error("ProcessInfoModule::Failed to access proc data.");
  }
  enum pids_item ItemsPID[] = {PIDS_ID_PID, PIDS_ID_PGRP};
  if(procps_pids_new(&infoptrPID, ItemsPID, 1) < 0) {
    ctk::runtime_error("Failed to prepare procps PID in ProcessInfoModule");
  }
#endif
};

void ProcessInfoModule::mainLoop() {
  info.processPID = getpid();
  info.processPID.write();

  auto group = readAnyGroup();
  while(true) {
    try {
#ifdef WITH_PROCPS
      FillProcInfo(proc_util::getInfo(info.processPID));
#else
      uint tmpPID = info.processPID;
      FillProcInfo(&tmpPID);
#endif
      logger->sendMessage(
          std::string("Process is running (PID: ") + std::to_string(info.processPID) + ")", logging::LogLevel::DEBUG);
    }
    catch(std::runtime_error& e) {
      logger->sendMessage(
          std::string("Failed to read process information for process with PID: ") + std::to_string(info.processPID),
          logging::LogLevel::ERROR);
    }
    writeAll();
    group.readUntil(trigger.getId());
  }
#ifndef WITH_PROCPS
  procps_pids_unref(&infoptr);
  procps_pids_unref(&infoptrPID);
#endif
}
#ifdef WITH_PROCPS
void ProcessInfoModule::FillProcInfo(const std::shared_ptr<proc_t>& infoPtr) {
  if(infoPtr != nullptr) {
    auto now = boost::posix_time::microsec_clock::local_time();
    int old_time = 0;
    try {
      //\FixMe: Exception is thrown on server start -> find out why.
      old_time = std::stoi(std::to_string(statistics.utime + statistics.stime + statistics.cutime + statistics.cstime));
      statistics.utime = std::stoi(std::to_string(infoPtr->utime));
      statistics.stime = std::stoi(std::to_string(infoPtr->stime));
      statistics.cutime = std::stoi(std::to_string(infoPtr->cutime));
      statistics.cstime = std::stoi(std::to_string(infoPtr->cstime));

      // info->start_time reads s since system was started
      int relativeStartTime = 1. * std::stoi(std::to_string(infoPtr->start_time)) / system.info.ticksPerSecond;
      statistics.startTime = system.status.sysStartTime + relativeStartTime;
      statistics.startTimeStr =
          boost::posix_time::to_simple_string(boost::posix_time::from_time_t(statistics.startTime));
      statistics.priority = std::stoi(std::to_string(infoPtr->priority));
      statistics.nice = std::stoi(std::to_string(infoPtr->nice));
      statistics.rss = std::stoi(std::to_string(infoPtr->rss));
      statistics.mem = std::stoi(std::to_string(infoPtr->vm_rss));

      statistics.memoryUsage = 1. * statistics.mem / system.status.maxMem * 100.;

      statistics.runtime = std::stoi(std::to_string(
          system.status.sysUpTime - std::stoi(std::to_string(infoPtr->start_time)) * 1. / system.info.ticksPerSecond));
    }
    catch(std::exception& e) {
      logger->sendMessage(std::string("FillProcInfo::Conversion failed: ") + e.what(), logging::LogLevel::ERROR);
    }
    // check if it is the first call after process is started (time_stamp  == not_a_date_time)
    if(!time_stamp.is_special()) {
      boost::posix_time::time_duration diff = now - time_stamp;
      statistics.pcpu = 1. * (statistics.utime + statistics.stime + statistics.cutime + statistics.cstime - old_time) /
          system.info.ticksPerSecond / (1. * diff.total_milliseconds() / 1000) * 100;
      statistics.avgcpu = 1. * (statistics.utime + statistics.stime + statistics.cutime + statistics.cstime) /
          system.info.ticksPerSecond / statistics.runtime * 100;
    }
    time_stamp = now;
  }
  else {
    time_stamp = boost::posix_time::not_a_date_time;
    statistics.utime = 0;
    statistics.stime = 0;
    statistics.cutime = 0;
    statistics.cstime = 0;
    statistics.startTime = 0;
    statistics.startTimeStr = "";
    statistics.priority = 0;
    statistics.nice = 0;
    statistics.rss = 0;
    statistics.pcpu = 0;
    statistics.avgcpu = 0;
    statistics.runtime = 0;
    statistics.mem = 0;
    statistics.memoryUsage = 0.;
  }
}
#else
void ProcessInfoModule::FillProcInfo(uint* pid) {
  if(pid) {
    auto now = boost::posix_time::microsec_clock::local_time();
    int old_time =
        std::stoi(std::to_string(statistics.utime + statistics.stime + statistics.cutime + statistics.cstime));
    struct pids_fetch* stack;

    stack = procps_pids_select(infoptr, pid, 1, PIDS_SELECT_PID);
    if(stack->counts->total < 1 || PIDS_VAL(0, s_int, stack->stacks[0], info) != (int)(*pid)) {
      logger->sendMessage(
          std::string("FillProcInfo::Failed to read data for PID: ") + std::to_string(*pid), logging::LogLevel::ERROR);
      return;
    }
    statistics.utime = PIDS_VAL(1, ull_int, stack->stacks[0], info);
    statistics.stime = PIDS_VAL(2, ull_int, stack->stacks[0], info);
    statistics.cutime = PIDS_VAL(3, ull_int, stack->stacks[0], info) - (uint)statistics.utime;
    statistics.cstime = PIDS_VAL(4, ull_int, stack->stacks[0], info);
    statistics.rss = PIDS_VAL(5, ul_int, stack->stacks[0], info);
    statistics.nice = PIDS_VAL(6, s_int, stack->stacks[0], info);
    statistics.priority = PIDS_VAL(7, s_int, stack->stacks[0], info);
    statistics.startTime = system.status.sysStartTime + (uint)PIDS_VAL(8, real, stack->stacks[0], info);
    statistics.startTimeStr = boost::posix_time::to_simple_string(boost::posix_time::from_time_t(statistics.startTime));

    statistics.runtime = (uint)PIDS_VAL(9, real, stack->stacks[0], info);
    statistics.mem = PIDS_VAL(10, ul_int, stack->stacks[0], info);
    statistics.memoryUsage = 1. * statistics.mem / system.status.maxMem * 100.;

    // check if it is the first call after process is started (time_stamp  == not_a_date_time)
    if(!time_stamp.is_special()) {
      boost::posix_time::time_duration diff = now - time_stamp;
      statistics.pcpu = 1. * (statistics.utime + statistics.stime + statistics.cutime + statistics.cstime - old_time) /
          system.info.ticksPerSecond / (1. * diff.total_milliseconds() / 1000) * 100;
      statistics.avgcpu = 1. * (statistics.utime + statistics.stime + statistics.cutime + statistics.cstime) /
          system.info.ticksPerSecond / statistics.runtime * 100;
    }
    time_stamp = now;
  }
  else {
    time_stamp = boost::posix_time::not_a_date_time;
    statistics.utime = 0;
    statistics.stime = 0;
    statistics.cutime = 0;
    statistics.cstime = 0;
    statistics.startTime = 0;
    statistics.startTimeStr = "";
    statistics.priority = 0;
    statistics.nice = 0;
    statistics.rss = 0;
    statistics.pcpu = 0;
    statistics.avgcpu = 0;
    statistics.runtime = 0;
    statistics.mem = 0;
    statistics.memoryUsage = 0.;
  }
}
#endif
void ProcessControlModule::mainLoop() {
  std::stringstream handlerMessage;
  logger->sendMessage(std::string("New ProcessModule started!"), logging::LogLevel::INFO);
  SetOffline();
  status.nRestarts = 0;

  try {
#ifdef WITH_PROCPS
    process.reset(new ProcessHandler(getName(), false, info.processPID, handlerMessage));
#else
    process.reset(new ProcessHandler(getName(), infoptrPID, false, info.processPID, handlerMessage));
#endif
    evaluateMessage(handlerMessage);
    if(info.processPID > 0) {
      logger->sendMessage(
          std::string("Found process that is still running. PID is: ") + std::to_string(info.processPID),
          logging::LogLevel::INFO);
      SetOnline(info.processPID);
    }
    else {
      if(config.bootDelay > 0) {
        logger->sendMessage(std::string("Process sleeping before starting main loop. Delay: ") +
                std::to_string(config.bootDelay) + "s.",
            logging::LogLevel::INFO);
        sleep(config.bootDelay);
      }
    }
  }
  catch(std::runtime_error& e) {
    logger->sendMessage(
        std::string("Failed to check for existing processes. Message:\n") + e.what(), logging::LogLevel::ERROR);
  }
  auto group = readAnyGroup();
  status.nFailed = 0;
  status.nRestarts = 0;
  while(true) {
    // reset number of failed tries and restarts in case the process is set offline
    if(!enableProcess) {
      status.nFailed = 0;
      status.nRestarts = 0;
      _stop = false;
      _restartRequired = false;
    }

    /*
     * don't do anything in case failed more than the user set limit
     * or if the server was restarted more than the user set limit
     * -> to reset turn off/on the process
     */
    if(_stop) {
      logger->sendMessage(std::string("Process sleeping. Fails: ") + std::to_string(status.nFailed) + "/" +
              std::to_string(config.maxFails) + ", Restarts: " + std::to_string(status.nRestarts) + "/" +
              std::to_string(config.maxRestarts),
          logging::LogLevel::DEBUG);

      if(_historyOn) FillProcInfo(nullptr);
      writeAll();
      group.readUntil(trigger.getId());
      continue;
    }

    /**
     * Check if restart is required
     */
    if(info.processPID > 0 && enableProcess) {
      CheckIsOnline(info.processPID);
      if(!status.isRunning) {
        if(config.maxRestarts == 0) {
          _stop = true;
          logger->sendMessage(std::string("Maximum number of restarts is 0. Process will not be started again."),
              logging::LogLevel::ERROR);
          _restartRequired = false;
        }
        else {
          _restartRequired = true;
        }
      }
    }

    /**
     * If starting a process fails and max restarts is 0 stop.
     */
    if(config.maxRestarts == 0 && _restartRequired) {
      _restartRequired = false;
      _stop = true;
    }

    /**
     * Check number of restarts in case it is set
     */
    if(config.maxRestarts != 0 && status.nRestarts == config.maxRestarts) {
      logger->sendMessage(std::string("Maximum number of restarts reached. Restarts: ") +
              std::to_string(status.nRestarts) + "/" + std::to_string(config.maxRestarts),
          logging::LogLevel::DEBUG);
      // Only stop if the process terminated. This ensures that the process status is updated.
      if(status.isRunning == 0) {
        _stop = true;
        logger->sendMessage(std::string("Process terminated after maximum number of restarts reached. Restarts: ") +
                std::to_string(status.nRestarts) + "/" + std::to_string(config.maxRestarts),
            logging::LogLevel::ERROR);
        resetProcessHandler(&handlerMessage);
      }
      else {
        _restartRequired = false;
      }
    }

    if(enableProcess) {
      // process should run
      if(info.processPID < 0 && !_stop && (status.nRestarts < config.maxRestarts || config.maxRestarts == 0)) {
        // process should run and is not running
        if(_restartRequired) {
          status.nRestarts += 1;
          _restartRequired = false;
        }
        // fill 0 since the process is started here and not running yet
        if(_historyOn) FillProcInfo(nullptr);
        try {
          logger->sendMessage(
              std::string("Trying to start a new process: ") + (std::string)config.path + "/" + (std::string)config.cmd,
              logging::LogLevel::INFO);
          // log level of the process handler is DEBUG per default. So all messages will end up here
#ifdef WITH_PROCPS
          process.reset(new ProcessHandler(getName(), false, handlerMessage, this->getName()));
#else
          process.reset(new ProcessHandler(getName(), infoptrPID, false, handlerMessage, this->getName()));
#endif
          SetOnline(process->startProcess((std::string)config.path, (std::string)config.cmd,
              (std::string)config.externalLogfile, (std::string)config.env, config.overwriteEnv));
          evaluateMessage(handlerMessage);
#ifdef WITH_PROCPS
          status.nChilds = proc_util::getNChilds(info.processPID, handlerMessage);
#else
          status.nChilds = proc_util::getNChilds(info.processPID, infoptrPID, handlerMessage);
#endif
          logger->sendMessage(handlerMessage.str(), logging::LogLevel::DEBUG);
        }
        catch(std::runtime_error& e) {
          logger->sendMessage(e.what(), logging::LogLevel::ERROR);
          Failed();
          SetOffline();
        }
      }
      else if(info.processPID > 0) {
        // process should run and is running
        logger->sendMessage(std::string("Process is running...") + std::to_string(status.isRunning) +
                " PID: " + std::to_string(info.processPID),
            logging::LogLevel::DEBUG);

        try {
#ifdef WITH_PROCPS
          FillProcInfo(proc_util::getInfo(info.processPID + config.pidOffset));
#else
          uint tmpPID = info.processPID + config.pidOffset;
          FillProcInfo(&tmpPID);
#endif
        }
        catch(std::runtime_error& e) {
          logger->sendMessage(std::string("Failed to read information for process ") +
                  std::to_string(info.processPID + config.pidOffset) + ". Check if pidOffset is set correctly!",
              logging::LogLevel::ERROR);
        }
      }
    }
    else {
      // process should not run
      if(info.processPID < 0) {
        // process should not run and is not running
        status.isRunning = 0;
        logger->sendMessage(
            std::string("Process Running: ") + std::to_string(status.isRunning) + ". Process is not running...OK",
            logging::LogLevel::DEBUG);
        if(_historyOn) FillProcInfo(nullptr);
      }
      else {
        // process should not run and is running
        logger->sendMessage(std::string("Trying to kill the process... PID: ") + std::to_string(info.processPID),
            logging::LogLevel::INFO);
        // Here the process is stopped in case enableProcess is set to 0. If it is already reset due to restart stop
        // don't do anything here
        if(process.get() != nullptr) {
          resetProcessHandler(&handlerMessage);
        }
        SetOffline();
      }
    }
    writeAll();
    group.readUntil(trigger.getId());
  }
#ifndef WITH_PROCPS
  procps_pids_unref(&infoptr);
  procps_pids_unref(&infoptrPID);
#endif
}

void ProcessControlModule::SetOnline(const int& pid) {
  usleep(100000);
  // set external log file in order to read the log file even if starting the process failed
  status.externalLogfile = (std::string)config.externalLogfile;
  CheckIsOnline(pid);
  if(status.isRunning == 1) {
    info.processPID = pid;
    status.path = (std::string)config.path;
    status.cmd = (std::string)config.cmd;
    status.env = (std::string)config.env;
    logger->sendMessage(std::string("Ok process is started successfully with PID: ") + std::to_string(info.processPID),
        logging::LogLevel::INFO);
  }
  else {
    SetOffline();
    logger->sendMessage(
        std::string("Failed to start process ") + config.path + "/" + config.cmd, logging::LogLevel::ERROR);
    Failed();
  }
}

void ProcessControlModule::SetOffline() {
  info.processPID = -1;
  status.isRunning = 0;
  status.path = "";
  status.cmd = "";
  status.env = "";
  /* Don't reset the log file name since the Logger might still try to access it.
     It will be updated once a new process is started!
  processLogfile = "";
  processLogfile.write();
  */
  FillProcInfo(nullptr);
}

void ProcessControlModule::Failed() {
  status.nFailed = status.nFailed + 1;
  if(!_stop && status.nFailed == config.maxFails) {
    _stop = true;
    _restartRequired = false;
    logger->sendMessage(std::string("Failed to start the process ") + config.path + "/" + (std::string)config.cmd +
            " " + std::to_string(config.maxFails) +
            " times. It will not be started again until you reset the process by switching it off and on again.",
        logging::LogLevel::ERROR);
  }
  else {
    _restartRequired = true;
  }
}

void ProcessControlModule::CheckIsOnline(const int pid) {
  logger->sendMessage(
      std::string("Checking process status for process: ") + std::to_string(pid), logging::LogLevel::DEBUG);
#ifdef WITH_PROCPS
  if(!proc_util::isProcessRunning(pid)) {
#else
  if(!proc_util::isProcessRunning(pid, infoptrPID)) {
#endif
    logger->sendMessage(std::string("Child process with PID  ") + std::to_string(info.processPID) +
            " is not running, but it should run!",
        logging::LogLevel::ERROR);
    SetOffline();
  }
  else {
    status.isRunning = 1;
  }
}

void ProcessControlModule::resetProcessHandler(std::stringstream* handlerMessage) {
  // ToDo: Set default to 2!
  if(config.killSig < 1)
    process->setSigNum(2);
  else
    process->setSigNum(config.killSig);
  process->setKillTimeout(config.killTimeout);
  process.reset(nullptr);
  evaluateMessage(*handlerMessage);
}

void ProcessControlModule::terminate() {
  if(process != nullptr) {
    logger->sendMessage(std::string("Process ") + getName() +
            " is disconnected. It is no longer controlled by the ProcessHandler! You have to take care of it on your "
            "own. PID is: " +
            std::to_string(info.processPID),
        logging::LogLevel::INFO);
    process->Disconnect();
  }
  process.reset(nullptr);
  ProcessInfoModule::terminate();
}

void ProcessControlModule::evaluateMessage(std::stringstream& msg) {
  auto list = logging::stripMessages(msg);
  for(auto& message : list) {
    logger->sendMessage(message.message.str(), message.logLevel);
  }
  msg.clear();
  msg.str("");
}
