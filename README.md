# Watchdog server
The watchdog server is used to monitor the system, the watchdog itself and addition processes (started by the watchdog) with respect to their memory consumption, runtime and more.

## Details about the watchdog server and its operation

Before starting the watchdog server the number of processes to be controlled needs to be fixed. This is done by modifying the config file `WatchdogServerConfig.xml` (installed in `/etc/chimeratk/watchdog-server/`). 
Here you only need to give a name to the process to be added. All process specific settings are done via the watchdog server when it is started.
 
In the server settings you can set a path (`SetPath`), where to execute the program specified in the CMD variable. You can also append command line arguments to the commad set in `SetCMD`. In order to add environment settings use `SetEnvironment`, e.g. `"ENSHOST=localhost"`. Separate multiple variables in the environment with a comma and multiple entries per variable with a colon, e.g. `"ENSHOST=localhost,PYTHONPATH=/locationA:/locationB"`.  
So far it is not possible to add processes dynamically. 

Further information are given in the doxygen documentation of the project.

**In order to add a process you need to stop (see below) the watchdog server and modify the file  WatchdogServerConfig.xml.**

For integration tasks all server variables can be found in `/etc/chimeratk/watchdog-server/WatchdogServer.xml`. 
It is created during the install process. Once you edit `WatchdogServerConfig.xml` the variable tree of the server changes. In that case use `watchdog-server-xmlGenerator` to recreate `/etc/chimeratk/watchdog-server/WatchdogServer.xml` with an updated variable tree.
This is the reason why also `watchdog-server-xmlGenerator` is installed in addition to the  `/etc/chimeratk/watchdog-server/WatchdogServer.xml` file.

When upgrading the watchdag server package the persist file is temporaryly copied to `/etc/chimeratk/watchdog-server/` and treated as configuration file. In consequence the persist file is the same after upgrading the watchdog server packge. This is also true if the watchdog server package is removed and installed again later. Only purging the watchdog server package will remove the persist file and all the other configuration files.

### MicroDAQ

It also possible to aquire parameters of the watchdog in a file, produced by the `MicroDAQ` module from `ApplicationCore`. In order to enable this feature the  `WatchdogServerConfig.xml` file needs to be cahnged accordingly.

    <variable name="enableMicroDAQ" type="int32" value="1" />
    
If MicroDAQ is enabled the collected data are stored in `/var/lib/chimeratk/watchdog-server/uDAQ`. In order to change this location put a symbilc link named `uDAQ` there. If you upgrade the watchdog server the uDAQ folder will be kept, whereas in case of removing the watchdog server package the folder or symlink `uDAQ` is removed too.

## Watchdog server service
The watchdog server will be started automatically after the package is installed if systemd is available on the host system. 
In order to check the watchdog service status use the command:
    
    sudo systemctl status chimeratk-watchdog.service

Other options are `stop` used to disable the watchdog service or `start` used to enable it again. If the watchdog server dies for some reason it is restarted 1 minute after it exited. Furthermore the watchdog server will be started automatically during booting the system. In some cases, certain processes require other processes to be running before being started. This can be achieved by using the watchdog server variable `BootDelay`, which introduces a delayed process start with respect to the Watchdog server start. The delay time needs to be tuned by the user. One could also consider to test the status of certain processes started by the Watchdog server directly. But the indication (`isRunning`) given by the Watchdog server does not guarantee that the started service is actuall set up completely. E.g. after starting the x2timer it takes some time until its Macropulse number is available to other servers.  

### RPC bind 
If some processes started by the watchdog server require `rpcbind` the service file has to adopted in order to start `rpcbind` before the watchdog server.

    #/etc/chimeratk/watchdog-server/chimeratk-watchdog.service
    [Unit]
    Description=ChimeraTK Watchdog Server
    Wants=rpcbind.service
    After=network.target local-fs.target rpcbind.target
    ...

After editing the service file update systemd:

    systemctl daemon-reload

## Logging
The watchdog server logging system uses different severity levels (*DEBUG*, *INFO*, *WARNING*, *ERROR*, *SILENT*). The default severity level is *DEBUG*. The severity level can be set for all process and the watchdog itself individually. Furthermore, the output stream of the logging messages can be set for all preccesses and the watchdog itself individually. By default messages are send to stdout and stderr. If a logfile name for the watchdog is set (*watchdog/SetLogFile*), messages are written to the corresponding logfile in addition. All processes and the watchdog write to the same logfile. The tail of this logfile can be inspected using *watchdog/LogfileTailExternal*. The length of the tail is controlled using *watchdog/SetTailLenght*. In contrast to this tail, the variable *watchdog/LogTail* only includes messages by the watchdog process. Similar each process provides his own messages: *process/LogTail*. The *LogFileTailExternal* variable includes the outpout of the process stared by the watchdog. This requires setting a logfile name for the process to be started (*process/SetLogfileExternal*). Again, for each process you can choose the output stream of the output produced by the started process using (*process/SetTargetStream*) and the severity level (*process/SetLogLevel*).

If the watchdog server is started as a service (see above) the latest messages can be seen when checking its status as described above. More output can be inspected using

    journalctl -ex
    
Remember this output could be supressed by choosing the logfile as target output stream and setting a logfile name. In this case nothing will be seen using the `journalctl` command, but all output will end up in the logfile. 
   

 

