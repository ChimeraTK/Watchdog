/*
 * sys_stst.h
 *
 *  Created on: Sep 6, 2017
 *      Author: zenker
 */

#ifndef INCLUDE_SYS_STAT_H_
#define INCLUDE_SYS_STAT_H_

#include <map>
#include <string.h>
#include <memory>
#include <proc/readproc.h>

#undef GENERATE_XML
#include <ChimeraTK/ApplicationCore/ScalarAccessor.h>

/**
 * Handler used to start, stop processes and check their status.
 * When a process is started fork + execv is used. The parent
 * process does not know the child process and its pid. Therefore,
 * the child created a temporary file that contains the PID. The parent
 * process reads the PID from that file and deletes it afterwards.
 * The pid of the last started process is stored.
 * If a new process is started or the object is destroyed a check is performed
 * if a process with the stored pid is still running.
 * If this is the case it is killed. First the process is killed using SIGINT
 * and if that fails SIGKILL is used.
 */
struct ProcessHandler{
private:
  /**
   * Read the PID from a temporary file that is created by the child process.
   */
	bool readTempPID(size_t &PID);
	/**
	 * If a process was started using the ProcessHandler this method can be used
	 * to kill it in case it is still running.
	 * If this is the case it is killed. First the process is killed using SIGINT
	 * and if that fails SIGKILL is used.
	 */
	void cleanup();
	size_t pid; ///< The pid of the last process that was started.
	std::string pidFile; ///< Name of the temporary file that holds the child PID

public:
	~ProcessHandler();
	/**
	 * Start a process.
	 * The process is started using fork + execv. Usually the copied process will
	 * have the same process group id (GPID) as the process how calls this function.
	 * But here we set the GPID of the new process to the process id of the new
	 * process. Thus if the child process itself starts new processes they all will
	 * have the same GPID. This allows to kill all of them at the same time calling
	 * \code
	 * kill -PID
	 * \endcode
	 * where PID is the PID of the child process.
	 * The child process creates a file that is called "pid". In case of the watchdog
	 * this file is created in the directory where the watchdog is run. Thus it could
	 * in principle happen that more than one process is started at the same time
	 * and file would be overwritten before the PID is read from the temporary file.
	 * \remark In that case use a unique string to be appended to the file name!
	 *
	 * \param path Path where to find the cmd you are about to call.
	 * \param cmd CMD including command line options.
	 * \param append Append a unique string to the temporary file name "pid"
	 * \return PID of the created process
	 * \remark The command will not be executated in the working directory of the
	 * calling process but in the given path!
	 */
	size_t startProcess(const std::string &path, const std::string &cmd, const std::string &append = "");
	/**
	 * Kill a certain process. If that process stared other processes also they are
	 * killed since
	 * \code
	 * kill -pid
	 * \endcode
	 * is called.
	 * \param PID Process ID of the process to be killed
	 * \param sig Signal used to kill the process (e.g. SIGINT = 2, SIGKILL = 9)
	 */
	void killProcess(const size_t &PID, const int &sig);
	/**
	 * Use system folder \c /proc to search for a process with the given process ID.
	 * If a directory with the given PID is found the process is running.
	 * \param PID Process ID to look for
	 * \return True if the process is running and registred in the \c /proc folder
	 */
	bool isProcessRunning(const size_t &PID);
	std::shared_ptr<proc_t> getInfo(const size_t &PID);
	/**
	 * Read the number of processes that belong to the same process group id (PGID).
	 * \param PGID The process group id used to look for processes
	 */
	size_t getNChilds(const size_t &PGID);
};

std::string space2underscore(std::string text);
std::vector<std::string> split_arguments(const std::string &arguments);

/**
 * Struct used to store static system information that should not change
 * during runtime.
 */
struct cpu_info {
	std::map<std::string,std::string> sysData;
	std::map<std::string,std::string>::iterator ibegin;
	std::map<std::string,std::string>::iterator iend;
	int count;
	cpu_info(){
		sysData["vendor"] = "";
		sysData["family"] = "";
		sysData["cpu family"] = "";
		sysData["model"] = "";
		sysData["model name"] = "";
		sysData["stepping"] = "";
		sysData["cpu MHz"] = "";
		sysData["bogomips"] = "";
		ibegin = sysData.begin();
		iend = sysData.end();
		count = -1;
	};
	void fill(const std::string &val, const std::string &pattern){
		if(sysData.count(pattern))
			sysData.at(pattern) = val;
	}
	std::string getInfo(const std::string &pattern){
		return sysData.at(pattern);
	}

};

class SysInfo{
private:
	bool lookup(const std::string &line, const std::string &pattern);
	/**
	 * Read information about the system.
	 * \throws runtime_error Exception is thrown in case one of the /proc/ files could not be read.
	 */
	void cpu_info_read();

public:
	cpu_info nfo;

	SysInfo();
};


#endif /* INCLUDE_SYS_STAT_H_ */
