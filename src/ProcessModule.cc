/*
 * ProcessModule.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "ProcessModule.h"

ProcessModule::ProcessModule(EntityOwner *owner, const std::string &name, const std::string &description,
        bool eliminateHierarchy, const std::unordered_set<std::string> &tags):
		ctk::ApplicationModule(owner, name, description, eliminateHierarchy, tags){

}

#ifdef BOOST_1_64
void ProcessModule::mainLoop(){
	SetOffline();
	processRestarts = 0;
	processRestarts.write();
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

	  if(startProcess){
		  if(process.get() == nullptr || !process->running()){
			  std::cout << "Trying to start the process..." << std::endl;
			  processCMD.read();
			  try{
				  process.reset(new bp::child((std::string)processPath + (std::string)processCMD));
				  SetOnline(process->id());
				  process->detach();
			  } catch (std::system_error &e){
				  std::cerr << "Failed to start the process with cmd: " << (std::string)processPath + (std::string)processCMD << "\n Message: " << e.what() << std::endl;
				  Failed();
			  }

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
				  SetOffline();
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
	SetOffline();
	processRestarts = 0;
	processRestarts.write();
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

	  if(processPID > 0 && startProcess){
		  if(!process.isProcessRunning(processPID)){
			  Failed();
			  std::cerr << "Child process not running any more, but it should run!" << std::endl;
			  processRestarts += 1;
			  processRestarts.write();

		  } else {
			  processRunning = 1;
			  processRunning.write();
		  }
	  }

	  if(startProcess){
		  if(processPID < 0){
			  std::cout << "Trying to start the process..." << " PID: " << getpid() <<std::endl;
			  try{
				  processPath.read();
				  processCMD.read();
				  SetOnline(process.startProcess((std::string)processPath, (std::string)processCMD));
			  } catch (std::runtime_error &e){
				  std::cout << e.what() << std::endl;
				  Failed();
			  }
		  } else {
#ifdef DEBUG
			  std::cout << "Process is running..." << processRunning << " PID: " << getpid() << std::endl;
#endif
		  }
	  } else {
		  if(processPID < 0 ){
#ifdef DEBUG
			  std::cout << "Process Running: " << processRunning << std::endl;
			  std::cout << "Process is not running...OK" << " PID: " << getpid() <<std::endl;
#endif
		  } else {
			  std::cout << "Trying to kill the process..." << " PID: " << getpid() <<std::endl;
			  process.killProcess(processPID);
			  SetOffline();
		  }
	  }
//	  usleep(200000);
	  sleep(5);
	}
}

void ProcessModule::SetOnline(const int &pid){
	processPID = pid;
	processPID.write();
	processRunning = 1;
	processRunning.write();
}

void ProcessModule::SetOffline(){
	processPID = -1;
	processPID.write();
	processRunning = 0;
	processRunning.write();
}

void ProcessModule::Failed(){
	processNFailed = processNFailed + 1;
    processNFailed.write();
    SetOffline();
    sleep(2);
}

#endif


