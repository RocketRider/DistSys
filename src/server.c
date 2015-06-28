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



char *html_test=
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

char *html_500=
"<!DOCTYPE html PUBLIC '-//IETF//DTD HTML 2.0//EN'>\n"
"<HTML>\n"
"   <HEAD>\n"
"      <TITLE>\n"
"         500 - Internal Server Error\n"
"      </TITLE>\n"
"   </HEAD>\n"
"<BODY>\n"
"   <H1>500 - Internal Server Error</H1>\n"
"   <P>The server had an internal error!</P>\n"
"</BODY>\n"
"</HTML>\n"
"\r\n"
"\r\n";





int static server_answer_error(int sd, int error_code)
{
	int ret;
	char* html;
	switch (error_code)
	{
	//TODO
	default:
	case HTTP_STATUS_INTERNAL_SERVER_ERROR:
		error_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
		html = html_500;
		break;
	}

	int html_length = (int)strlen(html);
	char* response = http_create_header(error_code, SERV_NAME, NULL, "text/html", " ", html_length);
	if (response == NULL)
	{
		perror("SERVER: create error response header failed!");
		return -1;
	}
	ret = write_to_socket(sd, response, strlen(response), SERV_WRITE_TIMEOUT);
	if (ret < 0)
	{
		perror("SERVER: failed to send error response header!");
		free(response);response = NULL;
		return -1;
	}
	ret = write_to_socket(sd, html, html_length, SERV_WRITE_TIMEOUT);
	if (ret < 0)
	{
		perror("SERVER: failed to send error response!");
		free(response);response = NULL;
		return -1;
	}

	free(response);response = NULL;
	return 0;
}


int static server_answer(int sd, char* url)
{
	int ret;
	int html_length = (int)strlen(html_test);
	char* response = http_create_header(200, SERV_NAME, NULL, "text/html", " ", html_length);
	if (response == NULL)
	{
		perror("SERVER: create response header failed!");
		server_answer_error(sd, HTTP_STATUS_INTERNAL_SERVER_ERROR);
		return -1;
	}

	printf("Response:\n");
	ret = write_to_socket(sd, response, strlen(response), SERV_WRITE_TIMEOUT);
	ret = write_to_socket(sd, html_test, html_length, SERV_WRITE_TIMEOUT);//TODO check return code!!!
	printf("ret write %d\n", ret);

	printf("%.*s", (int)strlen(response), response);
	printf("%.*s", (int)strlen(html_test), html_test);

	free(response);response = NULL;

	return 0;
}


int server_handle_client(int sd)
{
	char buf[HTTP_MAX_HEADERSIZE] = "";
	int cc = 0; //charachter count
	struct socket_info info;
	int ret;
	
	printf("New Clinet...\n");
	get_socket_name(sd, &info);
	printf("IP: %s, Port: %d\n\n", info.addr, info.port);
	
	do
	{
		ret = read_from_socket (sd, buf + cc, HTTP_MAX_HEADERSIZE - cc, SERV_READ_TIMEOUT);
		cc += ret;
	}
	while (ret >= 0 && strstr(buf, "\r\n\r\n") == NULL && cc < HTTP_MAX_HEADERSIZE);

	if (ret < 0 || cc <= 0)
	{
		perror("SERVER: receiving request failed!");
		server_answer_error(sd, HTTP_STATUS_BAD_REQUEST);
	}
	else
	{
		printf("Request: \n%.*s\n", cc, buf);
		
		//TODO Parse HTTP Header
		ret = server_answer(sd, "");
	}
	
	close(sd);
	return sd;
}


int server_accept_clients(int sd)
{
	int retcode = 0, newsd; //new socket descriptor
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
			//return 0;
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
				retcode = 1;
				//free(processStartTimeBuffer);
				//exit(0);
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
