//
// Core header files for C and IoTHub layer
//
#include <stdio.h>
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/xlogging.h"
#include "pnp_device.h"

#ifdef SET_TRUSTED_CERT_IN_CODE
#include "certs.h"
#else
static const char* certificates = NULL;
#endif // SET_TRUSTED_CERT_IN_CODE
long q_atol(const char *nptr);

int azure_Task(char* lat, char* lon)
{
	/*
	if (argc != 2)
	{
		LogError("USAGE: pnp_9205_sample [IoTHub device connection string]");
		return 1;
	}
*/


	pnp_device_run(lat,  lon);
	return 0;
}

int azure_setup(char * symm_auth)
{

	return pnp_device_initialize(symm_auth,certificates);
}

long q_atol(const char *nptr)  
{  
	int c;				/* current char */	
	long total; 		/* current total */  
	int sign;			/* if '-', then negative, otherwise positive */  

	/* skip whitespace */  
	while ( isspace((int)(unsigned char)*nptr) )  
		++nptr;  

	c = (int)(unsigned char)*nptr++;  
	sign = c;			/* save sign indication */	
	if (c == '-' || c == '+')  
		c = (int)(unsigned char)*nptr++;	/* skip sign */  

	total = 0;	

	while (isdigit(c)) {  
		total = 10 * total + (c - '0'); 	/* accumulate digit */	
		c = (int)(unsigned char)*nptr++;	/* get next char */  
	}  

	if (sign == '-')  
		return -total;	
	else  
		return total;	/* return result, negated if necessary */  
}

