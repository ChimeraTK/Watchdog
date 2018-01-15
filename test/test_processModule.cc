
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
  testApp() : Application("test"){ }
  ~testApp() {
    shutdown();
  }

  ProcessControlModule process { this, "ProcessControlModule",
      "ProcessControlModule test" };

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
    cs["Process"]("CMD") >> process.processCMD;
    cs["Process"]("Path") >> process.processPath;
    cs["Process"]("killSig") >> process.killSig;
    cs["Process"]("pidOffset") >> process.pidOffset;
    process.findTag("CS").connectTo(cs["Process"]);
  }
};

BOOST_AUTO_TEST_CASE( testStart) {
  testApp app;
  app.defineConnections();
  ChimeraTK::TestFacility tf;

  // Get the trigger variable thats blocking the application (i.e. ProcessControlModule)
  auto writeTrigger = tf.getScalar<int>("trigger/");
  auto processCMD = tf.getScalar<std::string>("Process/CMD");
  auto processPath = tf.getScalar<std::string>("Process/Path");
  auto enable = tf.getScalar<int>("Process/enableProcess");

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
  BOOST_CHECK_EQUAL(tf.readScalar<int>("Process/IsRunning"), 1);
}


BOOST_AUTO_TEST_CASE( testFailOnWrongDirectory) {
  testApp app;
  app.defineConnections();
  ChimeraTK::TestFacility tf;

  // Get the trigger variable thats blocking the application (i.e. ProcessControlModule)
  auto writeTrigger = tf.getScalar<int>("trigger/");
  auto processCMD = tf.getScalar<std::string>("Process/CMD");
  auto processPath = tf.getScalar<std::string>("Process/Path");
  auto enable = tf.getScalar<int>("Process/enableProcess");

  processPath = std::string("/etc/bin");
  processPath.write();
  processCMD = std::string("sleep 3");
  processCMD.write();
  enable = 1;
  enable.write();
  tf.runApplication();
  //  test status two times, because it happened that it only worked the first time
  for(size_t i = 1; i < 3; i++){
    usleep(10000);
    writeTrigger = i;
    writeTrigger.write();
    tf.stepApplication();
    BOOST_CHECK_EQUAL(tf.readScalar<int>("Process/Failed"), i);
  }
}


