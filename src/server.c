/*===================================================================
 * DHBW Ravensburg - Campus Friedrichshafen
 *
 * Vorlesung Verteilte Systeme
 *
 * Author:  Ralf Reutemann
 *
 *===================================================================*/
 
#include "server.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include "time.h"
#include "http.h"

#include "../libsockets/connect_tcp.h"
#include "../libsockets/passive_tcp.h"
#include "../libsockets/socket_io.h"
#include "../libsockets/socket_info.h"



char *html=
"<!DOCTYPE html PUBLIC '-//IETF//DTD HTML 2.0//EN'>\n"
"<HTML>\n"
"   <HEAD>\n"
"      <TITLE>\n"
"         A Small Hello\n"
"      </TITLE>\n"
"   </HEAD>\n"
"<BODY>\n"
"   <H1>Hi</H1>\n"
"   <P>This is very minimal 'hello world' HTML document.</P>\n"
"   <P>Time: Sun, 22 Aug 2004 19:07:45 GMT</P>\n"
"</BODY>\n"
"</HTML>\n"
"\r\n"
"\r\n";


int server_handle_client(int sd)
{
	char buf[SERV_BUFSIZE];
	int cc; //charachter count
	struct socket_info info;
	int ret;
	
	printf("New Clinet...\n");
	get_socket_name(sd, &info);
	printf("IP: %s, Port: %d\n\n", info.addr, info.port);
	
	//Read message:
	while ((cc = read(sd, buf, SERV_BUFSIZE)) > 0)
	{
		printf("Request: \n%.*s\n", cc,buf);

		
		int html_legth = (int)strlen(html);
		char* response = http_create_header(html_legth, 200);


		
		printf("Response:\n");
		ret = write(sd, response, strlen(response));
		ret = write(sd, html, html_legth);//TODO!!!
		printf("ret write %d\n", ret);
		
		printf("%.*s", (int)strlen(response), response);
		printf("%.*s", (int)strlen(html), html);
		free(response);
		
	}
	if (cc < 0)
	{
		//TODO: Error
	}
	
	close(sd);
	return sd;
}


int server_accept_clients(int sd)
{
	int retcode, newsd; //new socket descriptor
	struct sockaddr_in from_client;
	socklen_t from_client_len = 0;
	
	printf("wait for client...\n");
	
	//while (server_running)
	//{
		from_client_len = sizeof(from_client);//Is overwritten, so needs to be reset...
		newsd = accept(sd, /*INOUT*/ (struct sockaddr*) &from_client, /*INOUT*/ &from_client_len);
		printf("new client\n");
		if (newsd < 0)//Server Problem
		{
			retcode = newsd;
			//break;
			return 0;
			//TODO: Error!!
		}
		else
		{
			int pid = -1;
			if((pid = fork())<0)
			{
				perror("fork()");
				exit(1);
			
			} else if(pid == 0)/* child process */
			{
				close(sd);

				//printf("child process: [%i]\n",getpid());			

				server_handle_client(newsd);
				//free(processStartTimeBuffer);
				exit(0);
			} else /* parent process */
			{
				close(newsd);
				/*
				struct timespec tp;
				clock_gettime(CLOCK_REALTIME, &tp);

				int i;
				int *int_ptr;
				for(i = 0; i < TIMEBUFFER; i += 17)
				{
					int_ptr = processStartTimeBuffer + i;
					if(*int_ptr == 0)
					{
						*int_ptr = pid;
						memcpy(int_ptr + 1, &tp, sizeof(tp));
						break;
					}
				}
				*/ 
			}
		}
	//}
	
	
	
	
	return retcode;
}






int server_start(int port)
{
	int sd;
	
	sd = passive_tcp(port, SERV_QLEN);
	if (sd < 0)
	{
		//TODO: ERROR
		
		//exit(1);//Add Error codes
	}
	
	return sd;
}
