/*
 * test_procReader.cc
 *
 *  Created on: Feb 5, 2020
 *      Author: Klaus Zenker (HZDR)
 */
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE sysTest

#include "sys_stat.h"
#include <boost/test/unit_test.hpp>
using namespace boost::unit_test_framework;

BOOST_AUTO_TEST_CASE( testAMD64){
  SysInfo info("cpuinfo_amd64");
  BOOST_CHECK_EQUAL(info.getNCpu(), 4);
  BOOST_CHECK_EQUAL(info.getInfo("model name"), "Intel(R) Core(TM) i5-6200U CPU @ 2.30GHz");
  BOOST_CHECK_EQUAL(info.getInfo("bogomips"), "4800.00");
}

BOOST_AUTO_TEST_CASE( testArm){
  SysInfo info("cpuinfo_arm");
  BOOST_CHECK_EQUAL(info.getNCpu(), 4);
  BOOST_CHECK_EQUAL(info.getInfo("model name"), "ARMv7 Processor rev 4 (v7l)");
  BOOST_CHECK_EQUAL(info.getInfo("BogoMIPS"), "38.40");
  BOOST_CHECK_EQUAL(info.getInfo("Hardware"), "BCM2835");
}

