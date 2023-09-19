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
  std::unique_ptr<ProcessHandler> p(new ProcessHandler("", "test"));
  size_t pid = -1;
  try {
    pid = p->startProcess("/bin", "ping google.de", "test.log");
  }
  catch(std::logic_error& e) {
    cout << e.what() << endl;
  }
  sleep(2);
  BOOST_CHECK_EQUAL(proc_util::isProcessRunning(pid), true);
  p.reset(0);
  sleep(2);
  BOOST_CHECK_EQUAL(proc_util::isProcessRunning(pid), false);
}

BOOST_AUTO_TEST_CASE(testPIDTest) {
  size_t pid = -1;
  BOOST_CHECK_EQUAL(proc_util::isProcessRunning(pid), false);
}
