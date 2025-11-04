#include <iostream>
#include "serial.h"
#include "PPP.h"

int main() 
{
	Serial s;
	if(s.autoconnect(460800) == false)
		return -1;
	

	
    return 0;
}
