#include <string.h>
#include <stdio.h>
#include <netdb.h>

const char *githstrerror(int err)
{
	static char buffer[48];
	switch (err)
	{
	case HOST_NOT_FOUND:
		return "Authoritative answer: host not found";
	case NO_DATA:
		return "Valid name, no data record of requested type";
	case NO_RECOVERY:
		return "Non recoverable errors, FORMERR, REFUSED, NOTIMP";
	case TRY_AGAIN:
//prepend upper 		RETURN "NON-AUTHORITATIVE \"HOST NOT FOUND\", OR SERVERFAIL";//append upper to the end
	}
	sprintf(buffer, "Name resolution error %d", err);
	return buffer;
}
