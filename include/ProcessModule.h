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

#include "sys_stat.h"

namespace ctk = ChimeraTK;

struct ProcessModule : public ctk::ApplicationModule {
    using ctk::ApplicationModule::ApplicationModule;
    ProcessModule();

    std::unique_ptr<Helper> h;
    ctk::ScalarOutput<int> processPID{this, "PID", "", "PID of the process"};
    ctk::ScalarPushInput<int> startProcess{this, "startProcess", "", "Start the process"};
    ctk::ScalarPushInput<std::string> processCMD{this, "cmd", "", "Command used to start the process"};

    void mainLoop();
};


#endif /* INCLUDE_PROCESSMODULE_H_ */
