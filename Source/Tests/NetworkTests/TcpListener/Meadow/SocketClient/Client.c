#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include <netinet/ip.h>

void printerror(const char* action)
{
	printf("An error occurred while %s.\nError code: %i\nError description: %s\n", action, errno, strerror(errno));
}

int main()
{
	printf("Getting protocol.\n");
	fflush(stdout);
	struct protoent* tcp;
	tcp = getprotobyname("tcp");

	printf("Creating socket\n");
	fflush(stdout);
	int sd = socket(PF_INET, SOCK_STREAM, tcp->p_proto);
	if (sd == -1)
	{
		printerror("Creating a TCP socket");
		return(-1);
	}

	printf("Connecting to Meadow.\n");
	fflush(stdout);
	struct sockaddr_in isa;
	isa.sin_family = AF_INET;
	isa.sin_port = htons(11000);
	isa.sin_addr.s_addr = htonl(0xc0a801d0);
	if (connect(sd, (struct sockaddr *) &isa, sizeof(isa)) == -1)
	{
		printerror("Connecting to server");
		return(-1);
	}

	printf("Sending message\n");
	fflush(stdout);
	char msg[] = "Message to be sent<EOF>";
	if (send(sd, (void *) msg, sizeof(msg), MSG_NOSIGNAL) == -1)
	{
		printerror("Sending message to client");
		return(-1);
	}

	printf("Shutting down the socket.\n");
	fflush(stdout);
	shutdown(sd, SHUT_RDWR);

	return(0);
}