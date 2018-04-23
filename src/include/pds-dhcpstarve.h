#ifndef _PDSDHCPSTARVE_H
#define _PDSDHCPSTARVE_H

#include <unistd.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string>
#include <stdint.h>

#include <mutex>
#include <chrono>
#include <thread>
#include <iostream>

// general DHCP settings
#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67
#define DHCP_SERVER_ADDRESS "255.255.255.255"
#define DHCP_CLIENT_ADDRESS "255.255.255.255"
#define ETHERNET_MTU 1500

// times constant
#define WAIT_BEFORE_CHECK_SOCKET_AGAIN 100	// 0.1 sec
#define WAIT_FOR_RESPONSE_TIME 30000			// 30 sec


enum ERR_CODES {
	OK = 0,
	INCORRECT_PARAMS,
};



#endif
