/*
 * SystemInfo.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "SystemInfoModule.h"

SystemInfoModule::SystemInfoModule(EntityOwner *owner, const std::string &name, const std::string &description,
        bool eliminateHierarchy, const std::unordered_set<std::string> &tags):
		ctk::ApplicationModule(owner, name, description, eliminateHierarchy, tags){
	std::cout << "Init SystemInfoModule" << std::endl;
	for(auto it = sysInfo.nfo.ibegin; it != sysInfo.nfo.iend; it++){
		std::cout << "Adding sysInfor: " << space2underscore(it->first) << std::endl;
		strInfos[it->first].replace( ctk::ScalarOutput<std::string>{this, space2underscore(it->first), "" ,space2underscore(it->first)});
	}
}

void SystemInfoModule::mainLoop(){
	for(auto it = sysInfo.nfo.ibegin; it != sysInfo.nfo.iend; it++){
		strInfos.at(it->first) = it->second;
		strInfos.at(it->first).write();
	}
}
