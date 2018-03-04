/*	Author: Jiri Richter
	Project: pds-scanner
	About: 
*/


#include <string>
#include <iostream>
#include <unistd.h>

#include "tools.h"



int main(int argc, char* argv[]) {
	// parse the parameters using getopt
	std::string interfaceMy = NULL;
	std::string fileName = NULL;
	int c;
	bool errorFlag = false;

	while ((c = getopt(argc, argv, "i:f:")) != -1) {
		switch (c) {
			case 'i':
				interfaceMy = optarg;
				break;
			case 'f':
				fileName = optarg;
				break;
			case ':':
				std::cerr << "Option -" << optopt << "requires an operand" << std::endl;
				errorFlag = true;
				break;
			case '?':
				std::cerr << "Unrecognized option -" << optopt << std::endl;
				errorFlag = true;
				break;
		}
	}

	if (errorFlag) {
		return INCORRECT_PARAMS;
	}

	// ************************ SCANNING ************************************//
	// scan IPv4


	// scan IPv6


	// ********************* WRITING TO FILE ********************************//

	
	return OK;
}