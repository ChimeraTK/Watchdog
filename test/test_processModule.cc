
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

#include <iostream>
#include <string>

#include "ProcessModule.h"
#include "LoggingModule.h"
#include "WatchdogServer.h"
#include "ChimeraTK/ApplicationCore/TestFacility.h"
#include <ChimeraTK/RegisterPath.h>

#include <boost/test/unit_test.hpp>
using namespace boost::unit_test_framework;

/**
 * Define a test app to test the ProcessControlModule.
 * The trigger will be given by the control system to realise a
 * blocking read!
 */
struct testApp : public ChimeraTK::Application {
  testApp() : Application("test"){
    process.logStream = 0;
    ProcessHandler::setupHandler();
  }
  ~testApp() {
    shutdown();
  }

  ProcessControlModule process { this, "ProcessControlModule",
      "ProcessControlModule test" };

  LoggingModule logging {this, "Logging", "Logging module"};

  ChimeraTK::ControlSystemModule cs;

  void defineConnections() override{
    /**
     * Connect a control system variable to the ProcessControlModule instead of the trigger module.
     * Now there is a blocking read in the ProcessControlModule, which is used to step through the
     * application.
     */
    cs("trigger") >> process.trigger;

    /**
     * Define all other connections as done in the ProcessControlModule
     */
//    cs["Process"]("enableProcess") >> process.enableProcess;
//    cs["Process"]("SetCMD") >> process.config.cmd;
//    cs["Process"]("SetPath") >> process.config.path;
//    cs["Process"]("killSig") >> process.config.killSig;
//    cs["Process"]("pidOffset") >> process.config.pidOffset;
//    cs["Process"]("externalLogFile") >> process.config.externalLogfile;
//    cs["Process"]("maxFails") >> process.config.maxFails;
//    cs["Process"]("maxRestarts") >> process.config.maxRestarts;
    process.findTag("CS").connectTo(cs["Process"]);

//    cs("logLevel") >> logging.config.logLevel;
//    cs("logTailLenght") >> logging.config.tailLength;
//    cs("targetStream") >> logging.config.targetStream;
//    cs("logFile") >> logging.config.logFile;
    logging.findTag("CS").connectTo(cs["Logging"]);
    process.logging.connectTo(logging.input);
  }
};

void prepareTest(ChimeraTK::TestFacility *tf, int maxFails, int maxRestarts, std::string cmd, std::string path){
  auto writeTrigger = tf->getScalar<uint64_t>("trigger/");
  auto processCMD = tf->getScalar<std::string>("Process/config/command");
  auto processPath = tf->getScalar<std::string>("Process/config/path");
  auto enable = tf->getScalar<uint>("Process/enableProcess");
  auto pmaxFails = tf->getScalar<uint>("Process/config/maxFails");
  auto pmaxRestarts = tf->getScalar<uint>("Process/config/maxRestarts");
  BOOST_TEST_MESSAGE("Test maxrestarts==2, maxfails==2 with failing process.");
  processPath = path;
  processPath.write();
  processCMD = cmd;
  processCMD.write();
  enable = 1;
  enable.write();
  pmaxFails = maxFails;
  pmaxFails.write();
  pmaxRestarts = maxRestarts;
  pmaxRestarts.write();
}

BOOST_AUTO_TEST_CASE( testStart) {
  BOOST_TEST_MESSAGE("Start test process starting by the watchdog.");
  testApp app;
  app.defineConnections();
  ChimeraTK::TestFacility tf;

  // Get the trigger variable thats blocking the application (i.e. ProcessControlModule)
  auto writeTrigger = tf.getScalar<uint64_t>("trigger/");
  auto processCMD = tf.getScalar<std::string>("Process/config/command");
  auto processPath = tf.getScalar<std::string>("Process/config/path");
  auto enable = tf.getScalar<uint>("Process/enableProcess");

  processPath = std::string("/bin/");
  processPath.write();
  processCMD = std::string("sleep 2");
  processCMD.write();
  enable = 1;
  enable.write();
  tf.runApplication();
  writeTrigger.write();
  tf.stepApplication();
  usleep(200000);
  BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/isRunning"), 1);
  enable = 0;
  enable.write();
  writeTrigger.write();
  tf.stepApplication();
  usleep(200000);
}

BOOST_AUTO_TEST_CASE( testProcessFailLimit) {
  BOOST_TEST_MESSAGE("Test maxrestarts==2, maxfails==2 with failing process.");
  testApp app;
  app.defineConnections();
  ChimeraTK::TestFacility tf;
  prepareTest(&tf, 2, 2, std::string("sleep 1"), std::string("/etc/bin"));
  auto writeTrigger = tf.getScalar<uint64_t>("trigger/");
  tf.runApplication();
  for(size_t i = 1; i < 5 ; i++){
    writeTrigger.write();
    tf.stepApplication();
    if(i > 2)
      BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nFailed"), 2);
    else
      BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nFailed"), i);\
    if(i > 2)
      BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nRestarts"), 1);
    else
      BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nRestarts"), i-1);
  }
}

BOOST_AUTO_TEST_CASE( testProcessRestartLimit) {
  BOOST_TEST_MESSAGE("Test maxrestarts==2, maxfails==5 with failing process.");
  testApp app;
  app.defineConnections();
  ChimeraTK::TestFacility tf;
  prepareTest(&tf, 5, 2, std::string("sleep 1"), std::string("/etc/bin"));
  auto writeTrigger = tf.getScalar<uint64_t>("trigger/");
  tf.runApplication();
  for(int i = 0; i < 4; i++){
    writeTrigger.write();
    tf.stepApplication();
  }
  BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nFailed"), 3);
  BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nRestarts"), 2);
}

BOOST_AUTO_TEST_CASE( testProcessDefaultLimit) {
  BOOST_TEST_MESSAGE("Test maxrestarts==0, maxfails==0 with failing process.");
  testApp app;
  app.defineConnections();
  ChimeraTK::TestFacility tf;
  prepareTest(&tf, 0, 0, std::string("sleep 1"), std::string("/etc/bin"));
  auto writeTrigger = tf.getScalar<uint64_t>("trigger/");
  tf.runApplication();
  for(int i = 0; i < 3; i++){
    writeTrigger.write();
    tf.stepApplication();
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nFailed"), 1);
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nRestarts"), 0);
  }
}

BOOST_AUTO_TEST_CASE( testProcess){
  BOOST_TEST_MESSAGE("Test maxrestarts==0, maxfails==0.");
  testApp app;
  app.defineConnections();
  ChimeraTK::TestFacility tf;
  prepareTest(&tf, 0, 0, std::string("sleep 1"), std::string("/bin"));
  auto writeTrigger = tf.getScalar<uint64_t>("trigger/");
  tf.runApplication();
  for(int i = 0; i < 3; i++){
    writeTrigger.write();
    tf.stepApplication();
    if(i == 0)
      BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/isRunning"), 1);
    else
      BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/isRunning"), 0);
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nFailed"), 0);
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nRestarts"), 0);
    sleep(2);
  }
}

BOOST_AUTO_TEST_CASE( testProcessRestartCounter1){
  BOOST_TEST_MESSAGE("Reset and test maxrestarts==2, maxfails==0 + check status after last restart/process end, when process terminates between two triggers.");
  testApp app;
  app.defineConnections();
  ChimeraTK::TestFacility tf;
  prepareTest(&tf, 0, 2, std::string("sleep 1"), std::string("/bin"));
  auto writeTrigger = tf.getScalar<uint64_t>("trigger/");
  tf.runApplication();
  writeTrigger.write();
  tf.stepApplication();
  for(int i = 0; i < 3; i++){
    writeTrigger.write();
    tf.stepApplication();
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nFailed"), 0);
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nRestarts"), i);
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/isRunning"), 1);
    sleep(2);
  }
  // after max restarts is reached the process terminates and after the next trigger is fired
  writeTrigger.write();
  tf.stepApplication();
  BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/isRunning"), 0);
}

BOOST_AUTO_TEST_CASE( testProcessRestartCounter2){
  BOOST_TEST_MESSAGE("Reset and test maxrestarts==2, maxfails==0 + check status after last restart/process end, when process terminates a few triggers after max restarts was reached.");
  testApp app;
  app.defineConnections();
  ChimeraTK::TestFacility tf;
  prepareTest(&tf, 0, 2, std::string("sleep 1"), std::string("/bin"));
  auto writeTrigger = tf.getScalar<uint64_t>("trigger/");
  tf.runApplication();
  writeTrigger.write();
  tf.stepApplication();
  for(int i = 0; i < 3; i++){
    writeTrigger.write();
    tf.stepApplication();
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nFailed"), 0);
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/nRestarts"), i);
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/isRunning"), 1);
    if (i <2)
      sleep(2);
  }
  // after max restarts is reached process is running
  writeTrigger.write();
  tf.stepApplication();
  BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/isRunning"), 1);
  sleep(2);
  // now after some time it is not running any more
  writeTrigger.write();
  tf.stepApplication();
  BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/isRunning"), 0);
  writeTrigger.write();
  tf.stepApplication();
  BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/status/isRunning"), 0);
}
