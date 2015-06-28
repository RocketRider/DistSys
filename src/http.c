/*===================================================================
 * DHBW Ravensburg - Campus Friedrichshafen
 *
 * Vorlesung Verteilte Systeme
 *
 * Author:  Ralf Reutemann
 *
 *===================================================================*/

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>

#include "tinyweb.h"
#include "http.h"
#include "time.h"


http_method_entry_t http_method_list[] = {
    { "GET",         HTTP_METHOD_GET             },
    { "HEAD",        HTTP_METHOD_HEAD            },
    { "TEST",        HTTP_METHOD_TEST            },
    { "ECHO",        HTTP_METHOD_ECHO            },
    { "OPTIONS",     HTTP_METHOD_NOT_IMPLEMENTED },
    { "POST",        HTTP_METHOD_NOT_IMPLEMENTED },
    { "PUT",         HTTP_METHOD_NOT_IMPLEMENTED },
    { "DELETE",      HTTP_METHOD_NOT_IMPLEMENTED },
    { "TRACE",       HTTP_METHOD_NOT_IMPLEMENTED },
    { "CONNECT",     HTTP_METHOD_NOT_IMPLEMENTED },
    { NULL,          HTTP_METHOD_NOT_IMPLEMENTED }
};


http_status_entry_t http_status_list[] = {
    { 200, "OK"                              },  // HTTP_STATUS_OK
    { 206, "Partial Content"                 },  // HTTP_STATUS_PARTIAL_CONTENT
    { 301, "Moved Permanently"               },  // HTTP_STATUS_MOVED_PERMANENTLY
    { 304, "Not Modified"                    },  // HTTP_STATUS_NOT_MODIFIED
    { 400, "Bad Request"                     },  // HTTP_STATUS_BAD_REQUEST
    { 403, "Forbidden"                       },  // HTTP_STATUS_FORBIDDEN
    { 404, "Not Found"                       },  // HTTP_STATUS_NOT_FOUND
    { 416, "Requested Range Not Satisfiable" },  // HTTP_STATUS_RANGE_NOT_SATISFIABLE
    { 500, "Internal Server Error"           },  // HTTP_STATUS_INTERNAL_SERVER_ERROR
    { 501, "Not Implemented"                 }   // HTTP_STATUS_NOT_IMPLEMENTED
};


char* http_create_header(int status_code, char* server, time_t* last_modified, char* content_type, char* location, int html_length)
{
	char* response = malloc(HTTP_MAX_HEADERSIZE);
	if (response == NULL)
	{
		perror("HTTP_HEADER: Can't allocate memory!");
		return NULL;
	}
	memcpy(response, "HTTP/1.1 ", 10);


	//Status Code
	int http_status_list_index = 0;
	int http_status_list_size = sizeof(http_status_list)/sizeof(http_status_list[0]);

	for(http_status_list_index = 0; http_status_list_index < http_status_list_size; http_status_list_index++){
		if(http_status_list[http_status_list_index].code == status_code)
		{
			break;
		}
	}
	if (http_status_list_index >= http_status_list_size)
	{
		perror("HTTP_HEADER: Unknown status code!");
		free(response);response = NULL;
		return NULL;
	}
	sprintf(response + strlen(response), "%d %s\n", status_code, http_status_list[http_status_list_index].text);


	//Date
	time_t t = time(NULL);
	struct tm *my_tm = gmtime(&t);
	strftime(response + strlen(response), HTTP_MAX_HEADERSIZE - strlen(response), "Date: %a, %d %b %Y %H:%M:%S GMT\n", my_tm);

	//Server
	sprintf(response + strlen(response), "Server: %s\n", server);

	//Last-Modified
	if (last_modified != NULL)
	{
		my_tm = gmtime(last_modified);
		strftime(response + strlen(response), HTTP_MAX_HEADERSIZE - strlen(response), "Last-Modified: %a, %d %b %Y %H:%M:%S GMT\n", my_tm);
	}

	//Accept-Ranges
	sprintf(response + strlen(response), "Accept-Ranges: bytes\n");

	//Content-Length
	sprintf(response + strlen(response), "Content-Length: %d\n", html_length);

	//Content-Type
	sprintf(response + strlen(response), "Content-Type: %s\n", content_type);

	//Location
	if (strlen(location)>(HTTP_MAX_HEADERSIZE - strlen(response) - 11 - 20 - 1))
	{
		perror("HTTP_HEADER: Location is to long for the defined header size!");
		free(response);response = NULL;
		return NULL;
	}
	sprintf(response + strlen(response), "Location: %s\n", location);

	//Connection
	sprintf(response + strlen(response), "Connection: close \n\n");
	return response;
}





