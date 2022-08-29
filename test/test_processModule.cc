// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

/*
 * test_processModule.cc
 *
 *  Created on: Jan 15, 2018
 *      Author: zenker
 *
 *  Call with option --log_level=message to see messages from the tests.
 *  Select tests using e.g. --run_test=testFailOnWrongDirectory
 *
 */
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE ProcessModuleTest

#include "ChimeraTK/ApplicationCore/Logging.h"
#include "ChimeraTK/ApplicationCore/TestFacility.h"
#include "ProcessModule.h"

#include <ChimeraTK/RegisterPath.h>

#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>
using namespace boost::unit_test_framework;

/**
 * Define a test app to test the ProcessControlModule.
 * The trigger will be given by the control system to realize a
 * blocking read!
 */
struct testApp : public ChimeraTK::Application {
  testApp() : Application("test") { ProcessHandler::setupHandler(); }
  ~testApp() { shutdown(); }

  ProcessControlModule process{this, "Process", "ProcessControlModule test"};

  void initialise() override {
    Application::initialise();
    //    dumpConnections();
  }
};

void prepareTest(
    ChimeraTK::TestFacility* tf, int maxFails, int maxRestarts, std::string cmd, std::string path, bool enable = 1) {
  tf->setScalarDefault<std::string>("Process/config/command", cmd);
  tf->setScalarDefault<std::string>("Process/config/path", path);
  tf->setScalarDefault<ChimeraTK::Boolean>("Process/enableProcess", enable);
  tf->setScalarDefault<uint>("Process/config/maxRestarts", maxRestarts);
  tf->setScalarDefault<uint>("Process/config/maxFails", maxFails);
  tf->runApplication();
}

BOOST_AUTO_TEST_CASE(testStart) {
  BOOST_TEST_MESSAGE("Start test process starting by the watchdog.");
  testApp app;
  ChimeraTK::TestFacility tf;

  prepareTest(&tf, 2, 2, "sleep 2", "/bin/");
  tf.writeScalar("Trigger/tick", (uint64_t)0);
  tf.stepApplication();
  usleep(200000);
  BOOST_CHECK_EQUAL(tf.readScalar<ChimeraTK::Boolean>("Process/status/isRunning"), true);
  tf.writeScalar("Process/enableProcess", (ChimeraTK::Boolean)0);
  tf.writeScalar("Trigger/tick", (uint64_t)0);
  tf.stepApplication();
  usleep(200000);
}

BOOST_AUTO_TEST_CASE(testProcessFailLimit) {
  BOOST_TEST_MESSAGE("Test maxrestarts==2, maxfails==2 with failing process.");
  testApp app;
  ChimeraTK::TestFacility tf;
  prepareTest(&tf, 2, 2, std::string("sleep 1"), std::string("/etc/bin"), false);
  BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nFailed"), 0);
  BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nRestarts"), 0);
  for(size_t i = 1; i < 5; i++) {
    tf.writeScalar("Trigger/tick", (uint64_t)0);
    tf.writeScalar("Process/enableProcess", (ChimeraTK::Boolean) true);
    tf.stepApplication();
    if(i > 2)
      BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nFailed"), 2);
    else
      BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nFailed"), i);
    if(i > 2)
      BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nRestarts"), 1);
    else
      BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nRestarts"), i - 1);
  }
}

BOOST_AUTO_TEST_CASE(testProcessRestartLimit) {
  BOOST_TEST_MESSAGE("Test maxrestarts==2, maxfails==5 with failing process.");
  testApp app;
  ChimeraTK::TestFacility tf;
  prepareTest(&tf, 5, 2, std::string("sleep 1"), std::string("/etc/bin"));
  for(int i = 0; i < 4; i++) {
    tf.writeScalar("Trigger/tick", (uint64_t)0);
    tf.stepApplication();
  }
  BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nFailed"), 3);
  BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nRestarts"), 2);
}

BOOST_AUTO_TEST_CASE(testProcessDefaultLimit) {
  BOOST_TEST_MESSAGE("Test maxrestarts==0, maxfails==0 with failing process.");
  testApp app;
  ChimeraTK::TestFacility tf;
  prepareTest(&tf, 0, 0, std::string("sleep 1"), std::string("/etc/bin"));
  for(int i = 0; i < 3; i++) {
    tf.writeScalar("Trigger/tick", (uint64_t)0);
    tf.stepApplication();
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nFailed"), 1);
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nRestarts"), 0);
  }
}

BOOST_AUTO_TEST_CASE(testProcess) {
  BOOST_TEST_MESSAGE("Test maxrestarts==0, maxfails==0.");
  testApp app;
  ChimeraTK::TestFacility tf;
  prepareTest(&tf, 0, 0, std::string("sleep 1"), std::string("/bin"));
  for(int i = 0; i < 3; i++) {
    tf.writeScalar("Trigger/tick", (uint64_t)0);
    tf.stepApplication();
    if(i == 0)
      BOOST_CHECK_EQUAL(tf.readScalar<ChimeraTK::Boolean>("Process/status/isRunning"), true);
    else
      BOOST_CHECK_EQUAL(tf.readScalar<ChimeraTK::Boolean>("Process/status/isRunning"), false);
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nFailed"), 0);
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nRestarts"), 0);
    sleep(2);
  }
}

BOOST_AUTO_TEST_CASE(testProcessRestartCounter1) {
  BOOST_TEST_MESSAGE("Reset and test maxrestarts==2, maxfails==0 + check status after last restart/process end, when "
                     "process terminates between two triggers.");
  testApp app;
  ChimeraTK::TestFacility tf;
  prepareTest(&tf, 0, 2, std::string("sleep 1"), std::string("/bin"));
  tf.writeScalar("Trigger/tick", (uint64_t)0);
  tf.stepApplication();
  for(int i = 0; i < 3; i++) {
    tf.writeScalar("Trigger/tick", (uint64_t)0);
    tf.stepApplication();
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nFailed"), 0);
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nRestarts"), i);
    BOOST_CHECK_EQUAL(tf.readScalar<ChimeraTK::Boolean>("Process/status/isRunning"), true);
    sleep(2);
  }
  // after max restarts is reached the process terminates and after the next trigger is fired
  tf.writeScalar("Trigger/tick", (uint64_t)0);
  tf.stepApplication();
  BOOST_CHECK_EQUAL(tf.readScalar<ChimeraTK::Boolean>("Process/status/isRunning"), false);
}

BOOST_AUTO_TEST_CASE(testProcessRestartCounter2) {
  BOOST_TEST_MESSAGE("Reset and test maxrestarts==2, maxfails==0 + check status after last restart/process end, when "
                     "process terminates a few triggers after max restarts was reached.");
  testApp app;
  ChimeraTK::TestFacility tf;
  prepareTest(&tf, 0, 2, std::string("sleep 1"), std::string("/bin"));
  tf.writeScalar("Trigger/tick", (uint64_t)0);
  tf.stepApplication();
  for(int i = 0; i < 3; i++) {
    tf.writeScalar("Trigger/tick", (uint64_t)0);
    tf.stepApplication();
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nFailed"), 0);
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nRestarts"), i);
    BOOST_CHECK_EQUAL(tf.readScalar<ChimeraTK::Boolean>("Process/status/isRunning"), true);
    if(i < 2) sleep(2);
  }
  // after max restarts is reached process is running
  tf.writeScalar("Trigger/tick", (uint64_t)0);
  tf.stepApplication();
  BOOST_CHECK_EQUAL(tf.readScalar<ChimeraTK::Boolean>("Process/status/isRunning"), true);
  sleep(2);
  // now after some time it is not running any more
  tf.writeScalar("Trigger/tick", (uint64_t)0);
  tf.stepApplication();
  BOOST_CHECK_EQUAL(tf.readScalar<ChimeraTK::Boolean>("Process/status/isRunning"), false);
  tf.writeScalar("Trigger/tick", (uint64_t)0);
  tf.stepApplication();
  BOOST_CHECK_EQUAL(tf.readScalar<ChimeraTK::Boolean>("Process/status/isRunning"), false);
}
