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
	int senderSocket = 0;
	if ((senderSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		fprintf(stderr, "Cannot create socket.");
		delete(dhcpCoreInstance);
		exit(1);
	}
	struct sockaddr_in interfaceSettings;
	struct sockaddr_in serverSettings;
	// set parameter and bind to socket to specific interface
	interfaceSettings.sin_family = AF_INET;
	interfaceSettings.sin_addr = dhcpCoreInstance->getDeviceIP();
	interfaceSettings.sin_port = htons(DHCP_CLIENT_PORT);
	if (bind(senderSocket, (sockaddr*)&interfaceSettings, sizeof(interfaceSettings)) < 0)
	{ // cannot bind socket to given address and port
		fprintf(stderr, "Cannot bind socket to interface: %s.", chosenInterface.c_str());
		printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
		delete(dhcpCoreInstance);
		exit(1);
	}
	// set parameters for server address and connect socket to it
	serverSettings.sin_family = AF_INET;
	serverSettings.sin_port = htons(DHCP_SERVER_PORT);
	// set IPv4 broadcast
	if ((inet_pton(AF_INET, DHCP_SERVER_ADDRESS, &serverSettings.sin_addr)) <= 0)
	{
		fprintf(stderr, "Cannot set server address (%s) -> inet_pton error.", DHCP_SERVER_ADDRESS);
		delete(dhcpCoreInstance);
		exit(1);
	}
	// connect socket
	if (connect(senderSocket, (struct sockaddr *)&serverSettings, sizeof(sockaddr)) < 0)
	{
		fprintf(stderr, "Cannot connect socket to address.");
		delete(dhcpCoreInstance);
		exit(1);
	}

	// BEFORE SENDIND - create socket to catch response on Broadcast
	int receiverSocket = 0;
	if ((receiverSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		fprintf(stderr, "Cannot create socket for receiving broadcast response.");
		delete(dhcpCoreInstance);
		exit(1);
	}
	int broadcast = 1;
	setsockopt(receiverSocket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof (broadcast));

	struct sockaddr_in receivingAddress;
	receivingAddress.sin_addr.s_addr = INADDR_ANY; // broadcast
	receivingAddress.sin_family = AF_INET;
	receivingAddress.sin_port = htons(DHCP_SERVER_PORT);
	// connect socket
	if (bind(receiverSocket, (struct sockaddr *)&receivingAddress, sizeof(sockaddr)) < 0)
	{
		fprintf(stderr, "Cannot connect socket to address.");
		delete(dhcpCoreInstance);
		exit(1);
	}
	// create buffer to catch the respons, TO-DO: MAYBE CATCH USING BUFFER IN DHCP CORE
	char *response = 0;
	response = new char[ETHERNET_MTU];
	sockaddr_in si_other;
	unsigned slen = sizeof(sockaddr);

	// send message
	send(senderSocket, dhcpCoreInstance->getMessage(), dhcpCoreInstance->getSizeOfMessage(), 0);

	// wait for response
	while (1)
	{
		int receivedSize = recvfrom(receiverSocket, response, ETHERNET_MTU, 0, (sockaddr *)&si_other, &slen);
		printf("recv: %d | %s\n", receivedSize, response);
		struct sockaddr_in *s = (struct sockaddr_in *)&si_other;
		int port = ntohs(s->sin_port);
		char ipstr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
		printf("address: %s , port: %d\n", ipstr, port);
		printf("NEXT\n");
	}
	/*int readedBytes = recv(receiverSocket, &response, sizeof(response) - 1200, 0);
	std::cout << "Readed Bytes: " << readedBytes << std::endl;*/

	delete(dhcpCoreInstance);
	exit(0);
}
