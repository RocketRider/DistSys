/*
 * connect_tcp.c
 *
 * $Id: connect_tcp.c,v 1.2 2004/12/29 00:37:29 ralf Exp $
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>

#include "connect_tcp.h"


int
connect_tcp(const char *host, unsigned short port)
{
	struct protoent  *ppe;        		/* pointer to protocol information entry */
	int s = 0;                        	/* socket descriptor                     */
	int retcode = -1;
	char sport[15];
    struct addrinfo hints, *servinfo;
 
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; 		/* use AF_INET6 to force IPv6 */
    hints.ai_socktype = SOCK_STREAM;
 
	
	snprintf(sport, 15, "%d", port);
    if ( (retcode = getaddrinfo( host , sport , &hints , &servinfo)) != 0) 
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retcode));
        return 1;
    }
    

  ppe = getprotobyname("tcp");
  if (ppe == 0) {
    perror("ERROR: client getprotobyname(\"tcp\") ");
  }
  else
  {

	  s = socket(PF_INET6, SOCK_STREAM, ppe->p_proto);
	  if (s < 0) {
		perror("ERROR: client socket() ");
	  }
	  else
	  {
		    retcode = connect(s, servinfo->ai_addr, servinfo->ai_addrlen);
		    if (retcode < 0) {
				perror("ERROR: client connect() ");
		    }
	  }
  }

  freeaddrinfo(servinfo); /* all done with this structure */
  
  if (retcode < 0) {
	  return -1;
  }
  return s;
} /* end of connect_tcp */
