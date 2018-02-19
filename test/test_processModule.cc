
/*
 * test_processModule.cc
 *
 *  Created on: Jan 15, 2018
 *      Author: zenker
 */
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE ProcessModuleTest

#include <iostream>
#include <string>

#include "ProcessModule.h"
#include "LoggingModule.h"
#include "WatchdogServer.h"
#include "ChimeraTK/ApplicationCore/TestFacility.h"
#include <mtca4u/RegisterPath.h>

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

  void defineConnections(){
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
  auto writeTrigger = tf.getScalar<uint>("trigger/");
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
  writeTrigger = 1;
  writeTrigger.write();
  tf.stepApplication();
  BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/IsRunning"), 1);
}


BOOST_AUTO_TEST_CASE( testFailOnWrongDirectory) {
  BOOST_TEST_MESSAGE("Start testing if wrong path does not trigger failed status.");
  testApp app;
  app.defineConnections();
  ChimeraTK::TestFacility tf;

  // Get the trigger variable thats blocking the application (i.e. ProcessControlModule)
  auto writeTrigger = tf.getScalar<uint>("trigger/");
  auto processCMD = tf.getScalar<std::string>("Process/SetCMD");
  auto processPath = tf.getScalar<std::string>("Process/SetPath");
  auto enable = tf.getScalar<uint>("Process/enableProcess");
  auto logLevel = tf.getScalar<uint>("SetLogLevel/");
  auto maxFails = tf.getScalar<uint>("Process/maxFails");

  tf.runApplication();
  processPath = std::string("/etc/bin");
  processPath.write();
  processCMD = std::string("sleep 3");
  processCMD.write();
  enable = 1;
  enable.write();
  logLevel = 0;
  logLevel.write();
  maxFails = 5;
  maxFails.write();
  //  test status two times, because it happened that it only worked the first time
  for(size_t i = 1; i < 3 ; i++){
    writeTrigger = i;
    writeTrigger.write();
    tf.stepApplication();
    usleep(10000);
    BOOST_CHECK_EQUAL(tf.readScalar<uint>("Process/Failed"), i);
  }
}
