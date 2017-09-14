/*
 * ProcessModule.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "ProcessModule.h"

ProcessModule::ProcessModule(EntityOwner *owner, const std::string &name, const std::string &description,
        bool eliminateHierarchy, const std::unordered_set<std::string> &tags):
		ctk::ApplicationModule(owner, name, description, eliminateHierarchy, tags), process(nullptr){

}

#ifdef BOOST_1_64
void ProcessModule::mainLoop(){
	int PID;
	while(true) {
	  startProcess.read();
	  processCMD.read();
	  if(startProcess){
		  if(process.get() == nullptr || !process->running()){
			  std::cout << "Trying to start the process..." << std::endl;
			  try{
				  process.reset(new bp::child((std::string)processCMD));
			  } catch (std::system_error &e){
				  std::cerr << "Failed to start the process with cmd: " << (std::string)processCMD << "\n Message: " << e.what() << std::endl;
			  }

			  processPID = process->id();
			  processPID.write();
			  process->detach();
		  } else {
			  std::cout << "Process is running..." << std::endl;
		  }
	  } else {
		  if(process.get() == nullptr || !process->running()){
			  std::cout << "Process is not running...OK" << std::endl;
		  } else if (process->running()) {
			  std::cout << "Trying to kill the process..." << std::endl;
			  try{
				  process->terminate();
				  process.reset();
			  } catch (std::system_error &e){
				  std::cerr << "Failed to kill the process." << std::endl;
			  }
		  }
	  }
	  usleep(200000);
	}
}
#else

void ProcessModule::mainLoop(){
	processRunning = 0;
	processRunning.write();
	processPID = -1;
	processPID.write();
	process.reset(new ProcessHandler(&processPath, &processCMD, &processPID));
	while(true) {
	  startProcess.read();
	  // reset number of failed tries in case the process is set offline
	  if(!startProcess){
		  processNFailed = 0;
		  processNFailed.write();
	  }

	  // don't do anything in case failed more than 4 times -> to reset turn off/on the process
	  if(processNFailed > 4){
		  usleep(200000);
		  continue;
	  }

	  // \ToDo: Check number of fails
	  if(processPID > 0){
		  if(!isProcessRunning(processPID)){
			  processNFailed = processNFailed + 1;
			  processNFailed.write();
			  processRunning = 0;
			  processRunning.write();
			  processPID = -1;
			  processPID.write();
			  std::cerr << "Child process not running any more, but it should run!" << std::endl;
		  } else {
			  processRunning = 1;
			  processRunning.write();
		  }
	  }

	  if(startProcess){
		  if(processPID < 0){
			  std::cout << "Trying to start the process..." << " PID: " << getpid() <<std::endl;
			  try{
				  process->startProcess();
			  } catch (std::logic_error &e){
				  std::cout << "I'm not the child process." << std::endl;
			  }
		  } else {
			  std::cout << "Process is running..." << processRunning << " PID: " << getpid() << std::endl;
		  }
	  } else {
		  if(processPID < 0 ){
			  std::cout << "Process Running: " << processRunning << std::endl;
			  std::cout << "Process is not running...OK" << " PID: " << getpid() <<std::endl;
		  } else {
			  std::cout << "Trying to kill the process..." << " PID: " << getpid() <<std::endl;
			  process->killProcess();
			  processRunning = 0;
			  processRunning.write();
			  processPID = -1;
			  processPID.write();
		  }
	  }
//	  usleep(200000);
	  sleep(5);
	}
}

#endif


