#include "include/pds-dhcpCore.h"



DHCPCore::DHCPCore() : _dhcpMessage(nullptr), _dhcp_xid(0)
{
	// init random generator
	srand(time(nullptr));
}

DHCPCore::~DHCPCore()
{
	cleanDhcpCore();
}

void DHCPCore::cleanDhcpCore()
{
	// free dhcp message if exists
	if (_dhcpMessage != nullptr)
	{
		// TO-DO: THE DELETION OF SOME INNER PART MAY BE NEEDED.
		delete(_dhcpMessage);
	}
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
	// create new dhcp message
	_dhcpMessage = new dhcp_packet();
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
	_dhcpMessage->options[4] = 53;    /* DHCP message type option identifier */
	_dhcpMessage->options[5] = '\x01';               /* DHCP message option length in bytes */
	_dhcpMessage->options[6] = 1;

	// end option
	_dhcpMessage->options[7] = 255;
}

void DHCPCore::waitForAndProcessDHCPOffer()
{
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
		// call function to clean everything and return true
		cleanDhcpCore();
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
	case ERROROPTIONS::OK: // just to have everything covered, should not get here if not error
	default:
		fprintf(stderr, "Something happened! (Windows style error message :).");
	}
}

