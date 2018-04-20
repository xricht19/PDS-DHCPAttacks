#include "include/pds-dhcpstarve.h"
#include "include/pds-dhcpCore.h"
#include "include/pds-RawSocketHelper.h"



// 12 bytes pseudo header for udp checksum calculation
struct pseudoUDPHeader
{
	uint32_t source_address;
	uint32_t dest_address;
	uint8_t placeholder;
	uint8_t protocol;
	uint16_t udp_length;
};

// checksum calculation function
unsigned short csum(unsigned short *ptr, int nbytes)
{
	register long sum;
	unsigned short oddbyte;
	register short answer;

	sum = 0;
	while (nbytes>1) {
		sum += *ptr++;
		nbytes -= 2;
	}
	if (nbytes == 1) {
		oddbyte = 0;
		*((u_char*)&oddbyte) = *(u_char*)ptr;
		sum += oddbyte;
	}

	sum = (sum >> 16) + (sum & 0xffff);
	sum = sum + (sum >> 16);
	answer = (short)~sum;

	return(answer);
}


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

	// SOCKET FOR SENDING --------------------------------------------------------------------
	// create socket and send message 
	int senderSocket = 0;
	if ((senderSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		fprintf(stderr, "Cannot create socket.");
		delete(dhcpCoreInstance);
		exit(1);
	}

	// bind socket to given interface
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	memcpy(&ifr.ifr_name, chosenInterface.c_str(), sizeof(ifr.ifr_name));
	if(setsockopt(senderSocket, SOL_SOCKET, SO_BINDTODEVICE, (void*)&ifr, sizeof(ifr)) < 0)
	{
		fprintf(stderr, "Cannot assign socket to interface: %s. %s\n", chosenInterface.c_str(), strerror(errno));
		delete(dhcpCoreInstance);
		exit(1);
	}
	// enable broadcast on socket
	int optVal = 1;
	if(setsockopt(senderSocket, SOL_SOCKET, SO_BROADCAST, &optVal, sizeof(optVal)) < 0)
	{
		fprintf(stderr, "Cannot set socket to broadcast: %s. %s\n", chosenInterface.c_str(), strerror(errno));
		delete(dhcpCoreInstance);
		exit(1);
	}
	if(setsockopt(senderSocket, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(optVal)) < 0)
	{
		fprintf(stderr, "Cannot set socket to broadcast: %s. %s\n", chosenInterface.c_str(), strerror(errno));
		delete(dhcpCoreInstance);
		exit(1);
	}	
	struct sockaddr_in interfaceSettings;
	// set parameter and bind to socket to specific interface
	interfaceSettings.sin_family = AF_INET;
	interfaceSettings.sin_port = htons(DHCP_CLIENT_PORT);
	interfaceSettings.sin_addr.s_addr = INADDR_ANY;
	if (bind(senderSocket, (sockaddr*)&interfaceSettings, sizeof(interfaceSettings)) < 0)
	{ // cannot bind socket to given address and port
		fprintf(stderr, "Cannot bind socket to interface: %s. %s\n", chosenInterface.c_str(), strerror(errno));
		delete(dhcpCoreInstance);
		exit(1);
	}
	
	// set parameters for server address and connect socket to it
	struct sockaddr_in serverSettings;
	memset(&serverSettings, 0, sizeof serverSettings);
	serverSettings.sin_family = AF_INET;
	serverSettings.sin_port = htons(DHCP_SERVER_PORT);
	// set IPv4 broadcast
	if ((inet_pton(AF_INET, DHCP_SERVER_ADDRESS, &serverSettings.sin_addr)) <= 0)
	{
		fprintf(stderr, "Cannot set server address (%s) -> inet_pton error.\n", DHCP_SERVER_ADDRESS);
		delete(dhcpCoreInstance);
		exit(1);
	}

	// create buffer to catch the respons, TO-DO: MAYBE CATCH USING BUFFER IN DHCP CORE ------------------------
	char *response = 0;
	response = new char[ETHERNET_MTU];
	// address of dhcp server -> filled together with response
	sockaddr_in si_other;
	unsigned slen = sizeof(sockaddr);

	int i = 0;
	while (i < 1) // repeate until pool dried out
	{
		// init buffer for response
		std::fill_n(response, ETHERNET_MTU, 0);

		// create DHCP Discover message
		dhcpCoreInstance->createDHCPDiscoverMessage();
		if (dhcpCoreInstance->isError())
		{
			// error occured tidy up and exit
			delete(dhcpCoreInstance);
			exit(1);
		}

		// send message
		sendto(senderSocket, dhcpCoreInstance->getMessage(), dhcpCoreInstance->getSizeOfMessage(), 0, (sockaddr*)&serverSettings, sizeof(serverSettings));
		fprintf(stdout, "Packet send.\n");

		// get the response -> is blocking if no response, the code will get stack
		// TO-DO: Check what happend if pool is dried out
		//int receivedSize = recvfrom(receiverSocket, response, ETHERNET_MTU, 0, (sockaddr *)&si_other, &slen);
		int receivedSize = recvfrom(senderSocket, response, ETHERNET_MTU, 0, (sockaddr *)&si_other, &slen);
		struct sockaddr_in *s = (struct sockaddr_in *)&si_other;
		int port = ntohs(s->sin_port);
		char ipstr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
		printf("address: %s , port: %d\n", ipstr, port);

		// load response to dhcp_packet struct
		dhcpCoreInstance->ProcessDHCPOfferMessage(response, receivedSize);
		if (dhcpCoreInstance->isError())
		{
			// error occured tidy up and exit
			delete(dhcpCoreInstance);
			exit(1);
		}
		else
		{
			// send DHCP request
		}
		// wait for DHCPACK

		i++;
	}
	// free the array for response
	delete(response);

	delete(dhcpCoreInstance);
	exit(0);
}
