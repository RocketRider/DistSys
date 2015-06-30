/*
 * passive_tcp.c
 *
 * $Id: passive_tcp.c,v 1.4 2004/12/29 21:23:23 ralf Exp $
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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "passive_tcp.h"


unsigned short
get_port_from_name(const char *service)
{
   /*
   * Map service name to port number
   */
  
    unsigned short port = 0;
    int retcode;
    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; /* use AF_INET6 to force IPv6 */
    hints.ai_socktype = SOCK_STREAM;
    if ( (retcode = getaddrinfo( NULL , service , &hints , &servinfo)) != 0) 
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retcode));
        return 1;
    }
    if (servinfo != NULL)
    {
		if (servinfo->ai_family == AF_INET) {
			port = ntohs(((struct sockaddr_in*)(servinfo->ai_addr))->sin_port);
		}
		else
		{
			port = ntohs(((struct sockaddr_in6*)(servinfo->ai_addr))->sin6_port);
		}
		freeaddrinfo(servinfo); /* all done with this structure */
    }
  
    if (port == 0) {
      fprintf(stderr, "ERROR: cannot get '%s' serivce entry\n", service);
    } /* end if */

  return port;
} /* end of get_port_from_name */


int
passive_tcp(unsigned short port, int qlen)
{
  int retcode;
  int sd;                    /* socket descriptor */
  struct protoent *ppe;      /* pointer to protocol information entry */
  struct sockaddr_in6 server;
  const int on = 1;          /* used to set socket option */


  memset(&server, 0, sizeof(server));
  server.sin6_family = AF_INET6;
  server.sin6_addr = in6addr_any;
  server.sin6_port = htons(port);

  /*
   * Get protocol information entry.
   */
  ppe = getprotobyname("tcp");
  if (ppe == 0) {
    perror("ERROR: server getprotobyname(\"tcp\")");
    return -1;
  } /* end if */

  /*
   * Create a socket.
   */
  sd = socket(PF_INET6, SOCK_STREAM, ppe->p_proto);
  if (sd < 0) {
    perror("ERROR: server socket()");
    return -1;
  } /* end if */

  /*
   * Set socket options.
   */
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  
  /*
   * Bind the socket to the provided port.
   */
  retcode = bind(sd, (struct sockaddr *)&server, sizeof(server));
  if (retcode < 0) {
    perror("ERROR: server bind()");
    return -1;
  } /* end if */

  /*
   * Place the socket in passive mode.
   */
  retcode = listen (sd, qlen);
  if (retcode < 0) {
    perror("ERROR: server listen()");
    return -1;
  } /* end if */

  return sd;
} /* end of passive_tcp */
