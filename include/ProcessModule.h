/*
 * ProcessModule.h
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#ifndef INCLUDE_PROCESSMODULE_H_
#define INCLUDE_PROCESSMODULE_H_


#undef GENERATE_XML
#include <ChimeraTK/ApplicationCore/ApplicationCore.h>

#include <memory>

//#include "sys_stat.h"

namespace ctk = ChimeraTK;

#ifdef BOOST_1_64
#include <boost_process/process/child.hpp>
namespace bp = boost_process;
#else
#include "sys_stat.h"
#endif

struct ProcessModule : public ctk::ApplicationModule {
    ProcessModule(EntityOwner *owner, const std::string &name, const std::string &description,
            bool eliminateHierarchy=false, const std::unordered_set<std::string> &tags={});
#ifdef BOOST_1_64
    std::shared_ptr<boost_process::process::child> process;
    ctk::ScalarOutput<int> processPID{this, "PID", "", "PID of the process"};
#else
    ProcessHandler process;
    ctk::ScalarPollInput<std::string> processPath{this, "path", "", "Path where to execute the command used to start the process"};
#endif

    ctk::ScalarPollInput<int> startProcess{this, "startProcess", "", "Start the process"};
    ctk::ScalarPollInput<std::string> processCMD{this, "cmd", "", "Command used to start the process"};
    ctk::ScalarOutput<int> processRunning{this, "Status", "", "Process status 0: not running, 1: running"};
    ctk::ScalarOutput<int> processNFailed{this, "Failed", "", "Number of failed restarts"};
    ctk::ScalarOutput<int> processPID{this, "PID", "", "PID of the process"};
    ctk::ScalarOutput<int> processRestarts{this, "Restarts", "", "Number of time the process was automatically "
    		"restarted by the watchdog since server start."};

    /**
     * Set the PID and set status to running.
     * \param pid PID of the process that was started.
     */
    void SetOnline(const int &pid);

    /**
     * Reset the PID to -1 and the status to not running.
     */
    void SetOffline();

    /**
     * Calls SetOffset and increases the failed counter.
     * In addition a sleep of 1s is added to have some delay between different attempts to start a process.
     */
    void Failed();

    /**
     * Application core main loop.
     */
    void mainLoop();
};


#endif /* INCLUDE_PROCESSMODULE_H_ */
