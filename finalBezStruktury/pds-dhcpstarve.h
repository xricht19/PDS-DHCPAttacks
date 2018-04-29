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
#include <csignal>

// general DHCP settings
#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67
#define DHCP_SERVER_ADDRESS "255.255.255.255"
#define DHCP_CLIENT_ADDRESS "255.255.255.255"
#define ETHERNET_MTU 1500

// time client wait before check if response from server was received
#define WAIT_BEFORE_CHECK_SOCKET_AGAIN 100	// 0.1 sec
// time wait for response from DHCP server, the client give up after time elapsed
#define WAIT_FOR_RESPONSE_TIME 30000		// 30 sec

// number of threads
#define MAX_THREADS_COUNT 50
// attack is stopped if these number of clients fail to obtain IP address
#define STOP_TIMEOUT_LIMIT -1	// -1 the attack is never stopped

enum ERR_CODES {
	OK = 0,
	INCORRECT_PARAMS,
};


#endif
