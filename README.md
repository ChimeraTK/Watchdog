# Watchdog server
The watchdog server is used to monitor the system, the watchdog itself and addition processes (started by the watchdog) with respect to their memory consumption, runtime and more.

Before starting the watchdog the number of processes to be controlled needs to be fixed. This is done by modifying the config file `watchdog_server_processes.xml`. 
Here you only need to give a name to the process to be added. All process specific settings are done via the watchdog server when it is started. 
You can set a `path` where to execute the program specified in the CMD variable. You can also append command line arguments to the commad set in `cmd`. 
So far it is not possible to add processes dynamically. In order to add a process you need to stop the watchdog server and modify the file  `watchdog_server_processes.xml`.

Further information are given in the doxygen documentation of the project. 

