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

Other options are `stop` used to disable the watchdog service or `start` used to enable it again. If the watchdog server dies for some reason it is restarted 1 minute after it exited. Furthermore the watchdog server will be started automatically during booting the system. The command above can be issued without entering the sudo password, because an according command is added to `/etc/sudoers` during the installation process.

 

