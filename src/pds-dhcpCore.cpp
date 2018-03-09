#include "include/pds-dhcpCore.h"




DHCPCore::DHCPCore()
{
}

DHCPCore::~DHCPCore()
{
}


/*int DHCPCore::startDHCPStarvation()
{
	// TO-DO: is everything send using broadcast??


	// send dhcp discover

	// wait for response dhcp offer - more than one DHCP server?? || dhcp in different net

	// send dhcp request

	// wait for dhcp pack for confirmation, that the IP adrress is really blocked

	return 0;
}*/   //TO-DO: Move to DHCP Starve

void DHCPCore::getDeviceIPAddressNetMask(std::string deviceName)
{
	struct ifaddrs * ifAddrStruct = NULL;
	struct ifaddrs * ifa = NULL;
	void * tmpAddrPtr = NULL;

	// get info about all devices on this computer -> linked list
	getifaddrs(&ifAddrStruct);

	// go through all items in linked list and check which one has correct name, save its IP Adress and net mask
	for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
		// if the device dont have IP address, skip it
		if (!ifa->ifa_addr) { 
			continue;
		}
		// check if it is valid IP4 address
		if (ifa->ifa_addr->sa_family == AF_INET) { 
			if (strcmp(ifa->ifa_name, deviceName.c_str()) == 0)
			{
				printf("Found correct device. YEY!\n");
				tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
				// load address to buffer
				char addressBuffer[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
				printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
				// save info
				_deviceInfo = ifa;
				_deviceIP = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;

			}
		}
		// check if it is valid IP6 address -> not using, skip
		else if (ifa->ifa_addr->sa_family == AF_INET6) {
			tmpAddrPtr = &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
			char addressBuffer[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
			//printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
		}
	}
	if (ifAddrStruct != NULL) freeifaddrs(ifAddrStruct);
}
