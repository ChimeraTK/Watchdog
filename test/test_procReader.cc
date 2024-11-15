// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

/*
 * test_procReader.cc
 *
 *  Created on: Sep 7, 2017
 *      Author: Klaus Zenker (HZDR)
 */
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE sysTest

#include "ProcessHandler.h"
#include "sys_stat.h"

#include <boost/test/unit_test.hpp>

#include <iostream>

using namespace boost::unit_test_framework;

using namespace std;

BOOST_AUTO_TEST_CASE(testProcessHelper) {
  ProcessHandler::setupHandler();
#ifdef WITH_PROCPS
  std::unique_ptr<ProcessHandler> p(new ProcessHandler("", "test"));
#else
  struct pids_info* infoptrPID{nullptr};
  fatal_proc_unmounted(infoptrPID, 0);
  if(!infoptrPID) {
    std::runtime_error("ProcessInfoModule::Failed to access proc data.");
  }
  enum pids_item ItemsPID[] = {PIDS_ID_PID, PIDS_ID_PGRP};
  if(procps_pids_new(&infoptrPID, ItemsPID, 1) < 0) {
    ctk::runtime_error("Failed to prepare procps PID in ProcessInfoModule");
  }
  std::unique_ptr<ProcessHandler> p(new ProcessHandler("", infoptrPID, "test"));
#endif
  size_t pid = -1;
  try {
    pid = p->startProcess("/bin", "ping google.de", "test.log");
  }
  catch(std::logic_error& e) {
    cout << e.what() << endl;
  }
#ifdef WITH_PROCPS
  sleep(2);
  BOOST_CHECK_EQUAL(proc_util::isProcessRunning(pid), true);
  p.reset(0);
  sleep(2);
  BOOST_CHECK_EQUAL(proc_util::isProcessRunning(pid), false);
#else
  sleep(2);
  BOOST_CHECK_EQUAL(proc_util::isProcessRunning(pid, infoptrPID), true);
  p.reset(0);
  sleep(2);
  BOOST_CHECK_EQUAL(proc_util::isProcessRunning(pid, infoptrPID), false);
#endif
}

BOOST_AUTO_TEST_CASE(testPIDTest) {
  size_t pid = -1;
#ifdef WITH_PROCPS
  BOOST_CHECK_EQUAL(proc_util::isProcessRunning(pid), false);
#else
  struct pids_info* infoptrPID{nullptr};
  fatal_proc_unmounted(infoptrPID, 0);
  if(!infoptrPID) {
    std::runtime_error("ProcessInfoModule::Failed to access proc data.");
  }
  enum pids_item ItemsPID[] = {PIDS_ID_PID, PIDS_ID_PGRP};
  if(procps_pids_new(&infoptrPID, ItemsPID, 1) < 0) {
    ctk::runtime_error("Failed to prepare procps PID in ProcessInfoModule");
  }
  BOOST_CHECK_EQUAL(proc_util::isProcessRunning(pid, infoptrPID), false);
#endif
}
