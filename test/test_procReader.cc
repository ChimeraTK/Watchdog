/*
 * test_procReader.cc
 *
 *  Created on: Sep 7, 2017
 *      Author: zenker
 */
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE sysTest

#include "sys_stat.h"
#include <signal.h>

#include <boost/test/unit_test.hpp>
using namespace boost::unit_test_framework;

using namespace std;

BOOST_AUTO_TEST_CASE( testProcessHelper){
	ProcessHandler p;
	int pid = -1;
	try{
		pid = p.startProcess("/bin","ping google.de");
//		pid = p.startProcess("/home/zenker/singenerator_server","singenerator_server");
	} catch (std::logic_error &e){
		cout << e.what() << endl;
	}
	sleep(2);
	BOOST_CHECK_EQUAL(p.isProcessRunning(pid), true);
	p.killProcess(pid,SIGINT);
	sleep(2);
	BOOST_CHECK_EQUAL(p.isProcessRunning(pid), false);
}
