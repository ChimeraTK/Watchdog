# Watchdog server
The watchdog server is used to monitor the system, the watchdog itself and addition processes (started by the watchdog) with respect to their memory consumption, runtime and more.

## Details about the watchdog server and its operation

Before starting the watchdog server the number of processes to be controlled needs to be fixed. This is done by modifying the config file `WatchdogServerConfig.xml` (installed in `/etc/chimeratk/watchdog-server/`). 
Here you only need to set the number of process to be added. All process specific settings are done via the watchdog server when it is started.
 
In the server settings you can set a path (`config/path`), where to execute the program specified in the `config/command` variable. You can also append command line arguments to the commad set in `config/command`. In order to add environment settings use `config/environment`, e.g. `"ENSHOST=localhost"`. Separate multiple variables in the environment with a comma and multiple entries per variable with a colon, e.g. `"ENSHOST=localhost,PYTHONPATH=/locationA:/locationB"`. A process is started using `enableProcess=1` and stopped using  `enableProcess=0`. Stopping a process means sending the signal defined in `config/killSig` (default: `SIGINT`) to the process. If the process is not stopped by that signal after the defined `config/killTimeout` (default: 1s) the process will be killed using `SIGKILL`. If stopping your process needs longer than 1s adjust `config/killTimeout` in order to end your process in a defined way. You can even set `config/killTimeout` to a long time, since the watchdog is testing the process status during the kill timeout periodically every second and stopps when the process exited.
So far it is not possible to add processes dynamically. 

Further information are given in the doxygen documentation of the project.

**In order to add further processes you need to stop (see below) the watchdog server and modify the file  WatchdogServerConfig.xml.**

For integration tasks all server variables can be found in `/etc/chimeratk/watchdog-server/WatchdogServer.xml`. 
It is created during the install process. Once you edit `WatchdogServerConfig.xml` the variable tree of the server changes. In that case use `watchdog-server-xmlGenerator` to recreate `/etc/chimeratk/watchdog-server/WatchdogServer.xml` with an updated variable tree.
This is the reason why also `watchdog-server-xmlGenerator` is installed in addition to the  `/etc/chimeratk/watchdog-server/WatchdogServer.xml` file.

When upgrading the watchdag server package the persist file is temporaryly copied to `/etc/chimeratk/watchdog-server/` and treated as configuration file. In consequence the persist file is the same after upgrading the watchdog server packge. This is also true if the watchdog server package is removed and installed again later. Only purging the watchdog server package will remove the persist file and all the other configuration files.

### MicroDAQ

It also possible to aquire parameters of the watchdog in a file, produced by the `MicroDAQ` module from `ApplicationCore`. In order to enable this feature the  `WatchdogServerConfig.xml` file needs to be changed accordingly.

    <variable name="enableMicroDAQ" type="int32" value="1" />
    
If MicroDAQ is enabled the collected data are stored in `/var/lib/chimeratk/watchdog-server/uDAQ`. In order to change this location put a symbilc link named `uDAQ` there. If you upgrade the watchdog server the uDAQ folder will be kept, whereas in case of removing the watchdog server package the folder or symlink `uDAQ` is removed too.

### Server history

It is possible to create a history of selected variables by the server. The history of variables is enabled in the code by adding the tag `History`. In the `WatchdogServerConfig.xml` file the history can be enabled and the length can be set:

    <variable name="enableServerHistory" type="int32" value="1" />
    <variable name="serverHistoryLength" type="int32" value="1200" /> 
    
All history traces are put into `history` and named like the feeding server variables. In case of feeding arrays a history arry for each array element is created and the element
index is appended to the hiostory trace name.

## Watchdog server service
The watchdog server will be started automatically after the package is installed if systemd is available on the host system. 
In order to check the watchdog service status use the command:
    
    sudo systemctl status chimeratk-watchdog.service

Other options are `stop` used to disable the watchdog service or `start` used to enable it again. If the watchdog server dies for some reason it is restarted 1 minute after it exited. Furthermore the watchdog server will be started automatically during booting the system. In some cases, certain processes require other processes to be running before being started. This can be achieved by using the watchdog server variable `config/bootDelay`, which introduces a delayed process start with respect to the Watchdog server start. The delay time needs to be tuned by the user. One could also consider to test the status of certain processes started by the Watchdog server directly. But the indication (`status/isRunning`) given by the Watchdog server does not guarantee that the started service is actuall set up completely. E.g. after starting the x2timer it takes some time until its Macropulse number is available to other servers.  

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
The watchdog server logging system uses different severity levels (*DEBUG*, *INFO*, *WARNING*, *ERROR*, *SILENT*). The default severity level is *DEBUG*. The severity level can be set for all process and the watchdog itself individually (`config/logLevel`). Furthermore, the output stream of the logging messages can be set for all preccesses and the watchdog itself individually (`config/targetStream`). By default messages are send to stdout and stderr. If a logfile name for the watchdog is set (`watchdog/config/logFile`), messages are written to the corresponding logfile in addition. All processes and the watchdog write to the same logfile. The tail of this logfile can be inspected using `watchdog/status/logTailExternal`. The length of the tail is controlled using `watchdog/config/logTailLenghtExternal`. In contrast to this tail, the variable `watchdog/status/logTail` only includes messages by the watchdog process. Similar each process provides his own messages: `process/0/status/logTail`. The `process/0/status/logTailExternal` variable includes the outpout of the process stared by the watchdog. This requires setting a logfile name for the process to be started (`process/0/config/logfileExternal`). Again, for each process you can choose the output stream of the output produced by the started process using (`process/0/config/targetStream`) and the severity level (`process/0/config/logLevel`).

If the watchdog server is started as a service (see above) the latest messages can be seen when checking its status as described above. More output can be inspected using

    journalctl -ex
    
Remember this output could be supressed by choosing the logfile as target output stream and setting a logfile name. In this case nothing will be seen using the `journalctl` command, but all output will end up in the logfile. 
   

 

