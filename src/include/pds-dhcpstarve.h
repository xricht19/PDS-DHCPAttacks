#ifndef _PDSDHCPSTARVE_H
#define _PDSDHCPSTARVE_H

#include <unistd.h>
#include <string>

// general DHCP settings
#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67
#define DHCP_SERVER_ADDRESS "255.255.255.255"

enum ERR_CODES {
	OK = 0,
	INCORRECT_PARAMS,

};




#endif
