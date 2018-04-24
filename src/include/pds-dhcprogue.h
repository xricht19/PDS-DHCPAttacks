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
#include <thread>
#include <iostream>
#include <csignal>
#include <vector>

#include "pds-dhcpCore.h"

#define MAX_CLIENTS_CONCURRENTLY 100

// general DHCP settings
#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67
#define DHCP_SERVER_ADDRESS "255.255.255.255"
#define DHCP_CLIENT_ADDRESS "255.255.255.255"
#define ETHERNET_MTU 1500

#define CHADDR_LENGTH 16

// run example: ./pds-dhcprogue -i eth1 -p 192.168.1.100-192.168.1.199 -g 192.168.1.1 -n 8.8.8.8 -d fit.vutbr.cz -l 600

enum ERRORS
{
	OK = 1,
	INCORRECT_PARAMS,
	INET_PTON_ERROR,
};


#endif