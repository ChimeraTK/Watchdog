/*
 * SystemInfo.h
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#ifndef INCLUDE_SYSTEMINFOMODULE_H_
#define INCLUDE_SYSTEMINFOMODULE_H_

#undef GENERATE_XML
#include <ChimeraTK/ApplicationCore/ApplicationCore.h>

#include "sys_stat.h"

namespace ctk = ChimeraTK;

class SystemInfoModule : public ctk::ApplicationModule {
private:
	SysInfo sysInfo;

public:
	SystemInfoModule(EntityOwner *owner, const std::string &name, const std::string &description,
            bool eliminateHierarchy=false, const std::unordered_set<std::string> &tags={});
//	using ctk::ApplicationModule::ApplicationModule;
    std::map<std::string,ctk::ScalarOutput<std::string> > strInfos;

    void mainLoop();
};


#endif /* INCLUDE_SYSTEMINFOMODULE_H_ */
