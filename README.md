# Watchdog server
The watchdog server is used to monitor the system, the watchdog itself and addition processes (started by the watchdog) with respect to their memory consumption, runtime and more.

## Details about the watchdog server and its operation

Before starting the watchdog server the number of processes to be controlled needs to be fixed. This is done by modifying the config file `WatchdogServer_processes.xml` (installed in `/etc/chimeratk/watchdog-server/`). 
Here you only need to give a name to the process to be added. All process specific settings are done via the watchdog server when it is started.
 
In the server settings you can set a `path` where to execute the program specified in the CMD variable. You can also append command line arguments to the commad set in `cmd`. 
So far it is not possible to add processes dynamically. 

Further information are given in the doxygen documentation of the project.

**In order to add a process you need to stop (see below) the watchdog server and modify the file  WatchdogServer_processes.xml.**

For integration tasks all server variables can be found in `/etc/chimeratk/watchdog-server/WatchdogServer.xml`. 
It is created during the install process. Once you edit `WatchdogServer_processes.xml` the variable tree of the server changes. In that case use `watchdog-server-xmlGenerator` to recreate `/etc/chimeratk/watchdog-server/WatchdogServer.xml` with an updates variable tree.
This is the reason why also `watchdog-server-xmlGenerator` is installed and not just a single `/etc/chimeratk/watchdog-server/WatchdogServer.xml` file.

## Watchdog server service
The watchdog server will be started automatically after the package is installed if systemd is available on the host system. 
In order to check the watchdog service status use the command:
    
    sudo systemctl status chimeratk-watchdog.service

Other options are `stop` used to disable the watchdog service or `start` used to enable it again. If the watchdog server dies for some reason it is restarted 1 minute after it exited. Furthermore the watchdog server will be started automatically during booting the system. 

## Logging
The watchdog server logging system uses different severity levels (*DEBUG*, *INFO*, *WARNING*, *ERROR*, *SILENT*). The default severity level is *DEBUG*. The severity level can be set for all process and the watchdog itself individually. Furthermore, the output stream of the logging messages can be set for all preccesses and the watchdog itself individually. By default messages are send to stdout and stderr. If a logfile name for the watchdog is set (*watchdog/SetLogFile*), messages are written to the corresponding logfile in addition. All processes and the watchdog write to the same logfile. The tail of this logfile can be inspected using *watchdog/LogfileTailExternal*. The length of the tail is controlled using *watchdog/SetTailLenght*. In contrast to this tail, the variable *watchdog/LogTail* only includes messages by the watchdog process. Similar each process provides his own messages: *process/LogTail*. The *LogFileTailExternal* variable includes the outpout of the process stared by the watchdog. This requires setting a logfile name for the process to be started (*process/SetLogfileExternal*). Again, for each process you can choose the output stream of the output produced by the started process using (*process/SetTargetStream*) and the severity level (*process/SetLogLevel*).

If the watchdog server is started as a service (see above) the latest messages can be seen when checking its status as described above. More output can be inspected using

    journalctl -ex
    
Remember this output could be supressed by choosing the logfile as target output stream and setting a logfile name. In this case nothing will be seen using the `journalctl` command, but all output will end up in the logfile. 
   

 

