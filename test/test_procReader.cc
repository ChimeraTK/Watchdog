/*
 * test_procReader.cc
 *
 *  Created on: Sep 7, 2017
 *      Author: zenker
 */
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE sysTest

#include "sys_stat.h"

#include <boost/test/unit_test.hpp>
using namespace boost::unit_test_framework;

BOOST_AUTO_TEST_CASE( testProcessHelper){
	std::string CMD = "ping alpsdaq0";
	std::stringstream ss;
	ss << CMD << "&";
	system(ss.str().c_str());
	usleep(200000);
	Helper h;
	int PID;
	bool status = h.checkIsRunning(CMD,PID);
	BOOST_CHECK_EQUAL(status, true);

	ss.clear();
	ss << "kill " << PID;
	system(ss.str().c_str());
	usleep(200000);
	status = h.checkIsRunning(CMD,PID);
	BOOST_CHECK_EQUAL(status, false);
}
