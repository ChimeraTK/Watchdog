
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
#include <boost/algorithm/string.hpp>

#include "LoggingModule.h"
#include "SystemInfoModule.h"
#include "ChimeraTK/ApplicationCore/TestFacility.h"
#include <ChimeraTK/RegisterPath.h>

#include <boost/test/unit_test.hpp>
using namespace boost::unit_test_framework;

/**
 * Define a test app to test the LoggingModule.
 */
struct testApp : public ChimeraTK::Application {
  testApp() : Application("test") {}
  ~testApp() { shutdown(); }

  LoggingModule logging{this, "LoggingModule", "LoggingModule test"};

  ChimeraTK::ControlSystemModule cs;

  void defineConnections() override {
    /**
     * Connect a control system variable to the ProcessControlModule instead of the trigger module.
     * Now there is a blocking read in the ProcessControlModule, which is used to step through the
     * application.
     */
    logging.findTag("CS").connectTo(cs["Logging"]);

    cs("message") >> logging.input.message;
    cs("messageLevel") >> logging.input.messageLevel;

    //    dumpConnections();
  }
};

BOOST_AUTO_TEST_CASE(testLogging) {
  testApp app;
  app.defineConnections();
  ChimeraTK::TestFacility tf;
  tf.setScalarDefault<uint>("Logging/config/logLevel", 0);
  tf.setScalarDefault<uint>("Logging/config/logTailLength", 3);
  auto logLevel = tf.getScalar<uint>("Logging/config/logLevel");
  auto msg = tf.getScalar<std::string>("message");
  auto msgLevel = tf.getScalar<uint>("messageLevel");

  tf.runApplication();
  msgLevel = 0;
  msgLevel.write();
  msg = std::string("1st test message\n");
  msg.write();
  tf.stepApplication();
  msg = std::string("2nd test message\n");
  msg.write();
  msgLevel = 2;
  msgLevel.write();
  tf.stepApplication();
  auto tail = tf.readScalar<std::string>("Logging/status/logTail");
  std::vector<std::string> result;
  boost::algorithm::split(result, tail, boost::is_any_of("\n"));
  // result length should be 3 not 2, because new line is used to split, which results in 3 items although there are only two messages.
  BOOST_CHECK_EQUAL(result.size(), 3);

  /**** Test log level ****/
  logLevel = 2;
  logLevel.write();
  msgLevel = 0;
  msgLevel.write();
  msg = std::string("3rd test message\n");
  msg.write();
  tf.stepApplication();
  tail = tf.readScalar<std::string>("Logging/status/logTail");
  boost::algorithm::split(result, tail, boost::is_any_of("\n"));
  // should still be 3 because log level was too low!
  BOOST_CHECK_EQUAL(result.size(), 3);

  /**** Test tail length ****/
  msgLevel = 2;
  msgLevel.write();
  msg = std::string("4th test message\n");
  msg.write();
  tf.stepApplication();
  tail = tf.readScalar<std::string>("Logging/status/logTail");
  boost::algorithm::split(result, tail, boost::is_any_of("\n"));
  BOOST_CHECK_EQUAL(result.size(), 4);

  msg = std::string("5th test message\n");
  msg.write();
  tf.stepApplication();
  tail = tf.readScalar<std::string>("Logging/status/logTail");
  boost::algorithm::split(result, tail, boost::is_any_of("\n"));
  // should still be 4 because tailLength is 3!
  BOOST_CHECK_EQUAL(result.size(), 4);
}
