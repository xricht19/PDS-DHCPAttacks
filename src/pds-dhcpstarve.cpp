#include <iostream>

#include "include/pds-dhcpstarve.h"
#include "include/pds-dhcpCore.h"


int main(int argc, char* argv[])
{
	// parse the parameters using getopt
	std::string chosenInterface = "";
	int c;
	bool errorFlag = false;

	if (argc != 3) {
		std::cerr << "Incorrect number of params, exiting program." << std::endl;
		return INCORRECT_PARAMS;
	}

	while ((c = getopt(argc, argv, "i:")) != -1) {
		switch (c) {
		case 'i':
			chosenInterface = optarg;
			break;
		case ':':
			std::cerr << "Option -" << optopt << "requires an operand" << std::endl;
			errorFlag = true;
			break;
		case '?':
			std::cerr << "Unrecognized option: " << optopt << std::endl;
			errorFlag = true;
			break;
		}
	}

	if (errorFlag || chosenInterface.empty()) {
		std::cerr << "Incorrect params, exiting program." << std::endl;
		return INCORRECT_PARAMS;
	}

	std::cout << "Interface: " << chosenInterface << std::endl;

	// create instance of dhcp starve core class
	DHCPCore* dhcpCoreInstance = new DHCPCore();
	dhcpCoreInstance->getDeviceIPAddressNetMask(chosenInterface);
	if (dhcpCoreInstance->isError())
	{
		// error occured tidy up and exit
		delete(dhcpCoreInstance);
		exit(1);
	}

	// create DHCP Discover message
	dhcpCoreInstance->createDHCPDiscoverMessage();
	if (dhcpCoreInstance->isError())
	{
		// error occured tidy up and exit
		delete(dhcpCoreInstance);
		exit(1);
	}

	// create socket and send message
	int sock = 0;
	if ((sock = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		fprintf(stderr, "Cannot create socket.");
		delete(dhcpCoreInstance);
		exit(1);
	}
	struct sockaddr_in address;
	struct sockaddr_in serverAddress;
	serverAddress.sin_addr = dhcpCoreInstance->getDeviceIP();
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(DHCP_SERVER_PORT);
	// set IPv4 broadcast
	if ((inet_pton(AF_INET, DHCP_SERVER_ADDRESS, &serverAddress.sin_addr)) <= 0)
	{
		fprintf(stderr, "Cannot set server address (%s) -> inet_pton error.", DHCP_SERVER_ADDRESS);
		delete(dhcpCoreInstance);
		exit(1);
	}
	// connect socket
	if (connect(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
	{
		fprintf(stderr, "Cannot connect socket to address.");
		delete(dhcpCoreInstance);
		exit(1);
	}

	// BEFORE SENDIND - create socket to catch response


	// send message
	send(sock, dhcpCoreInstance->getMessage(), dhcpCoreInstance->getSizeOfMessage(), 0);

	// wait for response


	delete(dhcpCoreInstance);
	exit(0);
}
