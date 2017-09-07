/*
 * ProcessModule.cc
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#include "ProcessModule.h"

ProcessModule::ProcessModule(){
	h.reset(new Helper());
}
void ProcessModule::mainLoop(){
	int PID;
	while(true) {
	  startProcess.read();
	  processCMD.read();
	  std::string CMD = processCMD;
	  std::cout << "Before checking status." << std::endl;
	  bool isRunning = h->checkIsRunning(CMD, PID);
	  std::cout << "After checking status: " << isRunning << std::endl;
	  if(startProcess){
		  if(!isRunning){
			  std::cout << "Trying to start the process..." << std::endl;
			  std::stringstream ss;
			  ss << CMD << "&";
			  int ret = system(ss.str().c_str());
			  if(ret){
				  std::cerr << "Starting of process " << CMD << " returned code: " << ret << std::endl;
			  }
			  usleep(2000000);
			  if(h->checkIsRunning(CMD, PID)){
				  std::cout << "Process sucessfully started. PID: " << PID << std::endl;
				  processPID = PID;
				  processPID.write();
			  }
		  } else {
			  std::cout << "Process is running..." << std::endl;
		  }
	  } else {
		  if(!isRunning){
			  std::cout << "Process is not running...OK" << std::endl;
		  } else {
			  std::cout << "Trying to kill the process..." << std::endl;
			  processPID.read();
			  std::stringstream ss;
			  ss << "kill " << processPID;
			  int ret = system(ss.str().c_str());
			  if(ret){
				  std::cerr << "Killing the process returned code: " << ret << std::endl;
			  }
			  usleep(200000);
			  if(!h->checkIsRunning(CMD, PID)){
				  std::cout << "Process sucessfully killed." << std::endl;
				  processPID = -1;
				  processPID.write();
			  }
		  }
	  }
	  usleep(200000);
	}
}

