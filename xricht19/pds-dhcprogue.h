#ifndef _PDSDHCPROGUE_H
#define _PDSDHCPROGUE_H

#include <unistd.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string>
#include <stdint.h>
#include <mutex>
#include <chrono>
#include <iostream>
#include <csignal>
#include <vector>

#include "pds-dhcpCore.h"

// general DHCP settings
#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67
#define DHCP_SERVER_ADDRESS "255.255.255.255"
#define DHCP_CLIENT_ADDRESS "255.255.255.255"
#define ETHERNET_MTU 1500 // max message size read from socket at once

#ifndef DHCP_MSG_TYPES
	#define DHCP_MSG_TYPES
	#define DHCP_TYPE_DISCOVER 1
	#define DHCP_TYPE_OFFER 2
	#define DHCP_TYPE_REQUEST 3
	#define DHCP_TYPE_DECLINE 4
	#define DHCP_TYPE_ACK 5
	#define DHCP_TYPE_NACK 6
	#define DHCP_TYPE_RELEASE 7
	#define DHCP_TYPE_INFORM 8
#endif

#define ADDRESS_POOL_MANAGING_COUNT 20 // determine the number of address from pool which are checked if lease time elapsed between each try to serve client 
#define BLOCK_ADDRESS_AFTER_DISCOVER_TIME 120 // server wait 2 minutes for dhcp request after offer, then the address is set free again

// run example: ./pds-dhcprogue -i eth1 -p 192.168.1.100-192.168.1.199 -g 192.168.1.1 -n 8.8.8.8 -d fit.vutbr.cz -l 600

enum ERRORS
{
	OK = 0,
	INCORRECT_PARAMS,
	INET_PTON_ERROR, 
};


#endif