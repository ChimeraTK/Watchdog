
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
    process.logging = 0;
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
    cs["Process"]("enableProcess") >> process.enableProcess;
    cs["Process"]("SetCMD") >> process.processSetCMD;
    cs["Process"]("SetPath") >> process.processSetPath;
    cs["Process"]("killSig") >> process.killSig;
    cs["Process"]("pidOffset") >> process.pidOffset;
    cs["Process"]("externalLogFile") >> process.processSetExternalLogfile;
    cs["Process"]("maxFails") >> process.processMaxFails;
    cs["Process"]("maxRestarts") >> process.processMaxRestarts;
    process.findTag("CS").connectTo(cs["Process"]);

    cs("SetLogLevel") >> logging.logLevel;
    cs("SetLogTailLenght") >> logging.tailLength;
    cs("SetTargetStream") >> logging.targetStream;
    cs("SetLogFile") >> logging.logFile;
    logging.findTag("CS").connectTo(cs["Logging"]);
    process.findTag("Logging").connectTo(logging);
  }
};

BOOST_AUTO_TEST_CASE( testStart) {
  BOOST_TEST_MESSAGE("Start test process starting by the watchdog.");
  testApp app;
  app.defineConnections();
  ChimeraTK::TestFacility tf;

  // Get the trigger variable thats blocking the application (i.e. ProcessControlModule)
  auto writeTrigger = tf.getScalar<uint64_t>("trigger/");
  auto processCMD = tf.getScalar<std::string>("Process/SetCMD");
  auto processPath = tf.getScalar<std::string>("Process/SetPath");
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
  BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/IsRunning"), 1);
  enable = 0;
  enable.write();
  writeTrigger.write();
  tf.stepApplication();
  usleep(200000);
}


BOOST_AUTO_TEST_CASE( testCounter) {
  BOOST_TEST_MESSAGE("Start restart and fail counter.");
  testApp app;
  app.defineConnections();
  ChimeraTK::TestFacility tf;

  // Get the trigger variable thats blocking the application (i.e. ProcessControlModule)
  auto writeTrigger = tf.getScalar<uint64_t>("trigger/");
  auto processCMD = tf.getScalar<std::string>("Process/SetCMD");
  auto processPath = tf.getScalar<std::string>("Process/SetPath");
  auto enable = tf.getScalar<uint>("Process/enableProcess");
  auto maxFails = tf.getScalar<uint>("Process/maxFails");
  auto maxRestarts = tf.getScalar<uint>("Process/maxRestarts");
  BOOST_TEST_MESSAGE("Test maxrestarts==2, maxfails==2 with failing process.");
  processPath = std::string("/etc/bin");
  processPath.write();
  processCMD = std::string("sleep 1");
  processCMD.write();
  enable = 1;
  enable.write();
  maxFails = 2;
  maxFails.write();
  maxRestarts = 2;
  maxRestarts.write();
  tf.runApplication();
  for(size_t i = 1; i < 5 ; i++){
    writeTrigger.write();
    tf.stepApplication();
    if(i > 2)
      BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/Failed"), 2);
    else
      BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/Failed"), i);\
    if(i > 2)
      BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/Restarts"), 1);
    else
      BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/Restarts"), i-1);
  }

  BOOST_TEST_MESSAGE("Reset and test maxrestarts==2, maxfails==5 with failing process.");
  enable = 0;
  enable.write();
  writeTrigger = 1;
  writeTrigger.write();
  tf.stepApplication();
  maxFails = 5;
  maxFails.write();
  enable = 1;
  enable.write();
  for(int i = 0; i < 4; i++){
    writeTrigger.write();
    tf.stepApplication();
  }
  BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/Failed"), 3);
  BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/Restarts"), 2);


  BOOST_TEST_MESSAGE("Reset and test maxrestarts==0, maxfails==0 with failing process.");
  enable = 0;
  enable.write();
  writeTrigger.write();
  tf.stepApplication();
  enable = 1;
  enable.write();
  maxFails = 0;
  maxFails.write();
  maxRestarts = 0;
  maxRestarts.write();
  writeTrigger.write();
  tf.stepApplication();
  for(int i = 0; i < 3; i++){
    writeTrigger.write();
    tf.stepApplication();
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/Failed"), 1);
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/Restarts"), 0);
  }
  BOOST_TEST_MESSAGE("Reset and test maxrestarts==0, maxfails==0.");
  enable = 0;
  enable.write();
  processPath = std::string("/bin");
  processPath.write();
  writeTrigger.write();
  tf.stepApplication();
  enable = 1;
  enable.write();
  for(int i = 0; i < 3; i++){
    writeTrigger.write();
    tf.stepApplication();
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/Failed"), 0);
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/Restarts"), 0);
    sleep(2);
  }

  BOOST_TEST_MESSAGE("Reset and test maxrestarts==2, maxfails==0 + check status after last restart/process end, when process terminates between two triggers.");
  enable = 0;
  enable.write();
  writeTrigger.write();
  tf.stepApplication();
  enable = 1;
  enable.write();
  maxFails = 0;
  maxFails.write();
  maxRestarts = 2;
  maxRestarts.write();
  writeTrigger.write();
  tf.stepApplication();
  for(int i = 0; i < 3; i++){
    writeTrigger.write();
    tf.stepApplication();
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/Failed"), 0);
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/Restarts"), i);
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/IsRunning"), 1);
    sleep(2);
  }
  // after max restarts is reached the process terminates and after the next trigger is fired
  writeTrigger.write();
  tf.stepApplication();
  BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/IsRunning"), 0);

  BOOST_TEST_MESSAGE("Reset and test maxrestarts==2, maxfails==0 + check status after last restart/process end, when process terminates a few triggers after max restarts was reached.");
  enable = 0;
  enable.write();
  writeTrigger.write();
  tf.stepApplication();
  enable = 1;
  enable.write();
  writeTrigger.write();
  tf.stepApplication();
  for(int i = 0; i < 3; i++){
    writeTrigger.write();
    tf.stepApplication();
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/Failed"), 0);
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/Restarts"), i);
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/IsRunning"), 1);
    if (i <2)
      sleep(2);
  }
  // after max restarts is reached process is running
  writeTrigger.write();
  tf.stepApplication();
  BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/IsRunning"), 1);
  sleep(2);
  // now after some time it is not running any more
  writeTrigger.write();
  tf.stepApplication();
  BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/IsRunning"), 0);

}
