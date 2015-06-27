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


const char *http_reply_header=
"HTTP/1.1 200 OK \n"
"Date: Sun, 22 Aug 2004 19:07:45 GMT \n"
"Server: DistSys \n"
//"Last-Modified: Thu, 22 Apr 2004 15:40:42 GMT \n"
//"ETag: 'ec637-12ee-4087e77a' \n"
"Accept-Ranges: bytes \n"
"Content-Length:            \n"
"Connection: close \n"
"Content-Type: text/html \n"
"\n";


char* http_create_header(int html_legth, int status_code)
{
	//TODO Status Code

	//Format date:
	char time_str[30];
	time_t t = time(NULL);
	struct tm *my_tm = gmtime(&t);
	strftime(time_str, 29, "%a, %d %b %Y %H:%M:%S GMT", my_tm);

	//create response
	//char response[strlen(http_reply_header)+1];
	char* response = malloc(strlen(http_reply_header)+1);
	memcpy(response, http_reply_header, strlen(http_reply_header) + 1);
	//char html_res[strlen(html)+1];
	//memcpy(html_res, html, strlen(html) + 1);

	//Set date field
	char *date_pos = strstr(response, "Date:");
	memcpy(date_pos + 6, time_str, strlen(time_str));//29);

	//Set Content-Length field
	char *length_pos = strstr(response, "Content-Length:");
	char temp[11]; //max 10GB
	sprintf(temp, "%d", html_legth);
	memcpy(length_pos + 16, temp, strlen(temp));


	//char *time_pos = strstr(html_res, "Time:");
	//memcpy(time_pos + 6, time_str, strlen(time_str));

	return response;
}









