
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
#include "ChimeraTK/ApplicationCore/TestFacility.h"
#include <mtca4u/RegisterPath.h>

#include <boost/test/unit_test.hpp>
using namespace boost::unit_test_framework;

/**
 * Define a test app to test the LoggingModule.
 */
struct testApp : public ChimeraTK::Application {
  testApp() : Application("test"){ }
  ~testApp() {
    shutdown();
  }

  LoggingModule logging { this, "LoggingModule",
      "LoggingModule test" };

  ChimeraTK::ControlSystemModule cs;


  void defineConnections(){
    /**
     * Connect a control system variable to the ProcessControlModule instead of the trigger module.
     * Now there is a blocking read in the ProcessControlModule, which is used to step through the
     * application.
     */
    cs("LogLevel") >> logging.logLevel;
//    cs("LogFile") >> logging.logFile;
    cs("LogTailLenght") >> logging.tailLength;
    logging.findTag("CS").connectTo(cs["Logging"]);

    cs("message") >> logging.message;
    cs("messageLevel") >> logging.messageLevel;
  }
};

BOOST_AUTO_TEST_CASE( testLogging) {
  testApp app;
  app.defineConnections();
  ChimeraTK::TestFacility tf;

  // Get the trigger variable thats blocking the application (i.e. ProcessControlModule)
  auto logLevel = tf.getScalar<uint>("LogLevel/");
//  auto logFile = tf.getScalar<std::string>("LogFile");
  auto tailLength = tf.getScalar<uint>("LogTailLenght");
  auto msg = tf.getScalar<std::string>("message");
  auto msgLevel = tf.getScalar<uint>("messageLevel");

  logLevel = 0;
  logLevel.write();
//  logFile = std::string("test.log");
//  logFile.write();
  tailLength = 2;
  tailLength.write();
  tf.runApplication();
  msgLevel = 0;
  msgLevel.write();
  msg = std::string("1st test message");
  msg.write();
  tf.stepApplication();
  msg = std::string("2nd test message");
  msg.write();
  msgLevel = 2;
  msgLevel.write();
  tf.stepApplication();
  auto tail = tf.readScalar<std::string>("Logging/LogfileTail");
  std::vector< std::string > result;
  boost::algorithm::split(result, tail, boost::is_any_of("\n"));
  // result length should be 3 not 2, because new line is used to split, which results in 3 items although there are only two messages.
  BOOST_CHECK_EQUAL(result.size(), 3);
  logLevel = 2;
  logLevel.write();
  msgLevel = 0;
  msgLevel.write();
  msg = std::string("3st test message");
  msg.write();
  tf.stepApplication();
  // should still be 3 because log level was too low!
  BOOST_CHECK_EQUAL(result.size(), 3);
  msgLevel = 2;
  msgLevel.write();
  msg = std::string("3st test message");
  msg.write();
  tf.stepApplication();
  // should still be 3 because tailLength is 2!
  BOOST_CHECK_EQUAL(result.size(), 3);

}


