/*
 * test_libproc2.cc
 *
 *  Created on: Nov 12, 2024
 *      Author: Klaus Zenker (HZDR)
 */

#include "libproc2/stat.h"
#include "libproc2/pids.h"
#include "libproc2/diskstats.h"
#include "libproc2/meminfo.h"
#include "libproc2/misc.h"
//#include "libproc2/slabinfo.h"
//#include "libproc2/stat.h"
//#include "libproc2/vmstat.h"

#include "sys/types.h"

#include <string>
#include <iostream>

void readMeminfo(){
  struct meminfo_info *info = nullptr;
  struct meminfo_stack *stack;

  enum meminfo_item items[] = {
      MEMINFO_MEM_FREE, MEMINFO_MEM_SHARED, MEMINFO_MEM_TOTAL, MEMINFO_MEM_USED,
      MEMINFO_SWAP_CACHED, MEMINFO_SWAP_FREE, MEMINFO_SWAP_TOTAL, MEMINFO_SWAP_USED};
  procps_meminfo_new(&info);
  stack = procps_meminfo_select(info, items, 8);
  std::cout << "MEM_FREE: " << MEMINFO_VAL(0,ul_int,stack,info)
      << "\nMEM_SHARED: " << MEMINFO_VAL(1,ul_int,stack,info)
      << "\nMEM_TOTAL: " << MEMINFO_VAL(2,ul_int,stack,info)
      << "\nMEM_USED: " << MEMINFO_VAL(3,ul_int,stack,info)
      << "\nSWAP_FREE: " << MEMINFO_VAL(4,ul_int,stack,info)
      << "\nSWAP_SHARED: " << MEMINFO_VAL(5,ul_int,stack,info)
      << "\nSWAP_TOTAL: " << MEMINFO_VAL(6,ul_int,stack,info)
      << "\nSWAP_USED: " << MEMINFO_VAL(7,ul_int,stack,info) << std::endl;

}

void general(){
  long nCPU = procps_cpu_count();
  long hertz = procps_hertz_get();
  double load[3];
  procps_loadavg(&load[0], &load[1], &load[2]);
  std::cout << "nCPU: " << nCPU
      << "\n hertz: " << hertz
      << "\n Load: " << load[0] << "\t" << load[1] << "\t" << load[2] << std::endl;
  double uptime, idletime;
  procps_uptime(&uptime, &idletime);
  std::cout << "Uptime: " << uptime << "\t Idle time: " << idletime << std::endl;
}


 void disk(){
   enum diskstats_item items[] = {DISKSTATS_NAME, DISKSTATS_TYPE};
   struct diskstats_info *info = nullptr;
   procps_diskstats_new(&info);
   char *name;

   struct diskstats_stack* stack;
   stack  = procps_diskstats_select(info, "sda1", items,2);
   char* devname = DISKSTATS_VAL(0,str,stack,info);
   int type = DISKSTATS_VAL(1,s_int,stack,info);
   std::cout << devname << "\t type: " << type << std::endl;
 }
void PID(){
  struct pids_info *info = nullptr;
  struct pids_stack *stack;
  stack = fatal_proc_unmounted(info,0);

  enum pids_item Items[] = { PIDS_ID_PID, PIDS_CMD, PIDS_ID_PGRP};

  if (procps_pids_new(&info, Items, 2) < 0)
    return;
  while ((stack = procps_pids_get(info, PIDS_FETCH_TASKS_ONLY))) {
    ssize_t read;
    int tid = PIDS_VAL(0, s_int, stack, info);
    int gid = PIDS_VAL(2, s_int, stack, info);
    char *pid_comm = PIDS_VAL(1, str, stack, info);
    std::string ss(pid_comm);
    if(tid != gid)
      std::cout << ss.c_str() << " with PID: " << tid << " with grp ID: " << gid << std::endl;
  }
  procps_pids_unref(&info);
}

int main(){
  disk();
  general();
  readMeminfo();
  return 0;
}

