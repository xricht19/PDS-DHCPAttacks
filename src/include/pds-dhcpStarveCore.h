#ifndef _PDSDHCPSTARVECORE_H
#define _PDSDHCPSTARVECORE_H

class DHCPStarveCore
{
public:
	DHCPStarveCore();
	~DHCPStarveCore();

	// start starve attack
	int startDHCPStarvation();

private:
	// create dhcp discover message with spoffed MAC adrress
	// create dhcp request message


	// device IP adress
	// net mask

};

#endif