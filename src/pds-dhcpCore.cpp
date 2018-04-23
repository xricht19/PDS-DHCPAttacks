#include "include/pds-dhcpCore.h"


DHCPCore::DHCPCore() : _dhcpMessage(nullptr), _dhcpMessageResponse(nullptr), _dhcp_xid(0),
	_dhcpMessageResponseLength(0), _state(0)
{
	// init random generator
	srand(time(nullptr));
}

DHCPCore::~DHCPCore()
{
	cleanDHCPCore();
}

void DHCPCore::cleanDHCPCore()
{
	// free dhcp message if exists
	if (_dhcpMessage != nullptr)
	{
		// TO-DO: THE DELETION OF SOME INNER PART MAY BE NEEDED.
		delete(_dhcpMessage);
	}
	if (_dhcpMessageResponse != nullptr)
	{
		delete(_dhcpMessageResponse);
	}
}

void DHCPCore::initDHCPCore()
{
	// free dhcp message if exists
	if (_dhcpMessage != nullptr)
	{
		// TO-DO: THE DELETION OF SOME INNER PART MAY BE NEEDED.
		delete(_dhcpMessage);
	}
	if (_dhcpMessageResponse != nullptr)
	{
		delete(_dhcpMessageResponse);
	}

	_dhcpMessage = new dhcp_packet();
	_dhcpMessageResponse = new dhcp_packet();
}

char* DHCPCore::getMessage()
{
	return (char*) (_dhcpMessage);
}

int DHCPCore::getSizeOfMessage()
{
	return sizeof(dhcp_packet);
}

void DHCPCore::genAndSetMACAddress()
{
	// randomly generate 6 octet and store it in uchar array, set rest to DHCP_CHADDR_SIZE to zero
	for (auto octetNumber = 0; octetNumber < DHCPCORE_HLEN; octetNumber++)
	{
		uint8_t oct = 0;
		if (octetNumber < DHCPCORE_HLEN)
		{
			oct = rand() % 255;
			// generate number 0-255
			_dhcpMessage->chaddr[octetNumber] = oct;
		}
		_dhcpMessage->chaddr[octetNumber] = oct;
	}
}


/**
 * \brief Function create dhcp discover packet and pointer to this structure is saved to _dhcpMessage variable.
 * \return True, if the packet was succesfully created. False otherwise.
 */
void DHCPCore::createDHCPDiscoverMessage()
{
	// init dhcp core to prepare it for new message
	initDHCPCore();
	// start set values
	_dhcpMessage->op = DHCPCORE_DISCOVER_OP;
	_dhcpMessage->htype = DHCPCORE_HTYPE;
	_dhcpMessage->hlen = DHCPCORE_HLEN;
	_dhcpMessage->hops = DHCPCORE_HOPS;
	// generate random id and set it to message
	_dhcp_xid = (rand() % UINT32_MAX);
	_dhcpMessage->xid = _dhcp_xid;
	_dhcpMessage->secs = 0;
	_dhcpMessage->flags = htons(DHCPCORE_BROADCAST_FLAG);
	// convert IPv4 addresses in number and dots notation to byte notation -> inet_aton, 
	// return 1 if succesfully created, 0 otherwise
	// try set all ip addresses , if error occured, raise the flag and stop creating message
	int noError = inet_pton(AF_INET, DHCPCORE_DISCOVER_CLIENT_IP, &_dhcpMessage->ciaddr);
	if (!noError) { _errorType = INET_ATON_ERROR; return; }
	noError = inet_pton(AF_INET, DHCPCORE_DISCOVER_YOUR_IP, &_dhcpMessage->yiaddr);
	if (!noError) { _errorType = INET_ATON_ERROR; return; }
	noError = inet_pton(AF_INET, DHCPCORE_DISCOVER_SERVER_IP, &_dhcpMessage->siaddr);
	if (!noError) { _errorType = INET_ATON_ERROR; return; }
	noError = inet_pton(AF_INET, DHCPCORE_DISCOVER_GATEWAY_IP, &_dhcpMessage->giaddr);
	if(!noError) {_errorType = INET_ATON_ERROR; return;	}
	
	// generate and set spoffed MAC address
	genAndSetMACAddress();

	// set options
	/* first four bytes of options field is magic cookie (as per RFC 2132) */
	_dhcpMessage->options[0] = '\x63';
	_dhcpMessage->options[1] = '\x82';
	_dhcpMessage->options[2] = '\x53';
	_dhcpMessage->options[3] = '\x63';

	/* DHCP message type is embedded in options field */
	_dhcpMessage->options[4] = 53;					/* DHCP message type option identifier */
	_dhcpMessage->options[5] = '\x01';              /* DHCP message option length in bytes */
	_dhcpMessage->options[6] = DHCP_TYPE_DISCOVER;

	// end option
	_dhcpMessage->options[7] = 255;					/* DHCP message end of options */

	_state = DHCP_TYPE_DISCOVER;

	/*fprintf(stdout, "Op: %d\n", _dhcpMessage->op);
	fprintf(stdout, "Type: %d\n", _dhcpMessage->htype);
	fprintf(stdout, "hlen: %d\n", _dhcpMessage->hlen);
	fprintf(stdout, "hops: %d\n", _dhcpMessage->hops);
	fprintf(stdout, "xid: 0x%08X\n", _dhcpMessage->xid);
	fprintf(stdout, "secs: %d\n", _dhcpMessage->secs);
	fprintf(stdout, "flags: %d\n", _dhcpMessage->flags);*/
}

void DHCPCore::ProcessDHCPOfferMessage(char* message, int messageLength)
{
	if (messageLength <= 0)
	{
		_errorType = NON_VALID_MESSAGE;
		return;
	}
	// init dhcp class and fill it with message
	std::memset(_dhcpMessageResponse, 0, sizeof(dhcp_packet));
	std::memcpy(_dhcpMessageResponse, message, messageLength * sizeof(char));
	_dhcpMessageResponseLength = messageLength;

	uint32_t dhcpType = 0;
	getOptionValue(53, dhcpType);
	if(dhcpType == DHCP_TYPE_OFFER)
	{
		createDHCPRequestMessage();
	}
	else
	{
		// unexpected message type
		fprintf(stderr, "Unexpected dhcp message type: %d\n", dhcpType);
	}
}

void DHCPCore::createDHCPOfferMessage()
{

}

void DHCPCore::createDHCPRequestMessage()
{
	// edit _dhcpMessage to request
	std::memset(&_dhcpMessage->options, 0, sizeof(_dhcpMessage->options));
	// set options
	// first four bytes of options field is magic cookie (as per RFC 2132) 
	_dhcpMessage->options[0] = '\x63';
	_dhcpMessage->options[1] = '\x82';
	_dhcpMessage->options[2] = '\x53';
	_dhcpMessage->options[3] = '\x63';

	// DHCP message type is embedded in options field 
	_dhcpMessage->options[4] = 53;					/* DHCP message type option identifier */
	_dhcpMessage->options[5] = '\x01';              /* DHCP message option length in bytes */
	_dhcpMessage->options[6] = DHCP_TYPE_REQUEST;

	// fill options for dhcp request
	_dhcpMessage->options[7] = 50;
	_dhcpMessage->options[8] = '\x04';
	std::memcpy(&_dhcpMessage->options[9], &_dhcpMessageResponse->yiaddr.s_addr, 4);

	// end option
	_dhcpMessage->options[13] = 255;					/* DHCP message end of options */

	_state = DHCP_TYPE_REQUEST;
}

void DHCPCore::ProcessDHCPAckMessage(char* message, int messageLength)
{
	if (messageLength <= 0)
	{
		_errorType = NON_VALID_MESSAGE;
		return;
	}
	// init dhcp class and fill it with message
	std::memset(_dhcpMessageResponse, 0, sizeof(dhcp_packet));
	std::memcpy(_dhcpMessageResponse, message, messageLength * sizeof(char));
	// check if it is correct message
	uint32_t dhcpType = 0;
	getOptionValue(53, dhcpType);
	if(dhcpType == DHCP_TYPE_ACK)
	{ 	// finish
		_state = DHCP_TYPE_ACK;
	}
	else
	{
		// unexpected message type
		fprintf(stderr, "Unexpected dhcp message type: %d\n", dhcpType);
	}
}


void DHCPCore::getDeviceIPAddressNetMask(std::string deviceName)
{
	// set error to true
	_errorType = CANNOT_FIND_GIVEN_DEVICE_IP;

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
				tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
#ifdef _DEBUG_
				// load address to buffer
				printf("Found correct device. YEY!\n");
				char addressBuffer[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
				printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
#endif
				// save info
				_deviceInfo = *ifa;
				_deviceIP = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
				_errorType = OK;
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


/**
 * \brief Check if the error occured in dhcpCore. If the error really occur, the cleaning is performed and the instance cannot be used anymore.
 * \return True if the error occur, false otherwise.
 */
bool DHCPCore::isError()
{
	if(_errorType != ERROROPTIONS::OK)
	{
		printDHCPCoreError();
		return true;
	}
	return false;
}

void DHCPCore::printDHCPCoreError() const
{
	switch (_errorType)
	{
	case ERROROPTIONS::INET_ATON_ERROR:
		fprintf(stderr, "Cannot convert string IP Address to byte order using 'inet_aton'!/n");
		break;
	case ERROROPTIONS::CANNOT_FIND_GIVEN_DEVICE_IP:
		fprintf(stderr, "Cannot get IP Address of given device!\n");
		break;
	case ERROROPTIONS::NON_VALID_MESSAGE:
		fprintf(stderr, "Message received but is not valid!\n");
		break;
	case INCORRECT_XID:
		fprintf(stderr, "Message has invalid xid!\n");
		break;
	default:
		fprintf(stderr, "Something happened! Sadly, I cannot continue.");
	}
}

int DHCPCore::getCurrentXID()
{
	return _dhcpMessage->xid;
}

void DHCPCore::getOptionValue(const int optionNumber, uint32_t &value)
{
	int currentOption = 0; 
	uint8_t currentOptionSize = 0;
	int position = 4; // skip magic cookies
	unsigned char* options = _dhcpMessageResponse->options;
	while(currentOption != 255)
	{
		currentOption = options[position];
		currentOptionSize = options[++position];
		//fprintf(stdout, "Founded options and size: %d | %d\n", currentOption, currentOptionSize);
		if(currentOption == optionNumber)
		{
			if(currentOptionSize > 4)
			{
				fprintf(stderr, "Too large option, cannot process!\n");
				return;
			}
			for(auto i = 0; i < currentOptionSize; ++i)
			{
				int partVal = options[position+1+i];
				value = (partVal << (8*i));
			}
			return;
		}
		else
		{
			position += currentOptionSize;
		}
	}
}

int DHCPCore::getXID(char* message, int messageLength)
{
	uint32_t xid = 0;
	/*if(messageLength > 0)
	{
		dhcp_packet* packet = new dhcp_packet();
		std::memset(packet, 0, sizeof(dhcp_packet));
		std::memcpy(packet, message, messageLength * sizeof(char));
		xid = packet->xid; 
		delete(packet);
	}*/
	std::memcpy(&xid, &message[4], 4);
	return xid;
}
