/*
 * WatchdogServer.h
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#ifndef INCLUDE_WATCHDOGSERVER_H_
#define INCLUDE_WATCHDOGSERVER_H_

#undef GENERATE_XML
#include <ChimeraTK/ApplicationCore/ApplicationCore.h>

#include <map>

#include "SystemInfoModule.h"
#include "ProcessModule.h"

namespace ctk = ChimeraTK;

struct WatchdogServer : public ctk::Application {
	WatchdogServer();
	~WatchdogServer() { shutdown(); }

	SystemInfoModule systemInfo{this, "systeminfo", "Module reading system information"};
	std::map<std::string, std::shared_ptr<ProcessModule> > processes;
	/**
	 * Use either
	 * - ctk::ControlSystemModule cs;
	 *   in combinartion with
	 *   cs["SYS"]("val")
	 * or
	 * - ctk::ControlSystemModule cs{"SYS"};
	 *   in combination with
	 *   cs("val")
	 * In any case SYS defines the location in DOOCS.
	 * SYS/TEST would create another hierarchy that would result
	 * in SYS/TEST.val in DOOCS.
	 *
	 */
	ctk::ControlSystemModule cs;

	void defineConnections();

};

#endif /* INCLUDE_WATCHDOGSERVER_H_ */
