/*===================================================================
 * DHBW Ravensburg - Campus Friedrichshafen
 *
 * Vorlesung Verteilte Systeme
 *
 * Author:  Ralf Reutemann
 *
 *===================================================================*/
#define _XOPEN_SOURCE 700 /* needed for strptime */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>

#include "tinyweb.h"
#include "http.h"
#include "time.h"
#include "error_pages.h"


const int HTTP_METHOD_LIST_SIZE = 11;
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


const int HTTP_STATUS_LIST_SIZE = 10;
http_status_entry_t http_status_list[] = {
    { HTTP_STATUS_OK, 							"OK"                              , ""			},  // HTTP_STATUS_OK
    { HTTP_STATUS_PARTIAL_CONTENT, 				"Partial Content"                 , ""			},  // HTTP_STATUS_PARTIAL_CONTENT
    { HTTP_STATUS_MOVED_PERMANENTLY, 			"Moved Permanently"               , HTML_301	},  // HTTP_STATUS_MOVED_PERMANENTLY
    { HTTP_STATUS_NOT_MODIFIED, 				"Not Modified"                    , ""			},  // HTTP_STATUS_NOT_MODIFIED
    { HTTP_STATUS_BAD_REQUEST, 					"Bad Request"                     , HTML_400	},  // HTTP_STATUS_BAD_REQUEST
    { HTTP_STATUS_FORBIDDEN, 					"Forbidden"                       , HTML_403	},  // HTTP_STATUS_FORBIDDEN
    { HTTP_STATUS_NOT_FOUND, 					"Not Found"                       , HTML_404	},  // HTTP_STATUS_NOT_FOUND
    { HTTP_STATUS_RANGE_NOT_SATISFIABLE, 		"Requested Range Not Satisfiable" , ""			},  // HTTP_STATUS_RANGE_NOT_SATISFIABLE
    { HTTP_STATUS_INTERNAL_SERVER_ERROR, 		"Internal Server Error"           , HTML_500	},  // HTTP_STATUS_INTERNAL_SERVER_ERROR
    { HTTP_STATUS_NOT_IMPLEMENTED,		 		"Not Implemented"                 , HTML_501	}   // HTTP_STATUS_NOT_IMPLEMENTED
};



//Source: http://rosettacode.org/wiki/URL_decoding#C
inline int ishex(int x)
{
	return	(x >= '0' && x <= '9')	||
		(x >= 'a' && x <= 'f')	||
		(x >= 'A' && x <= 'F');
}
int http_decode_url(const char *s, char *dec)
{
	char *o;
	const char *end = s + strlen(s);
	unsigned int c; //Edit of original code!
	for (o = dec; s <= end; o++)
	{
		c = *s++;
		if (c == '+')
		{
			c = ' ';
		}
		else if (c == '%' && (	!ishex(*s++)	|| !ishex(*s++)	|| !sscanf(s - 2, "%2x", &c)))
		{
			return -1;
		}

		if (dec) *o = c;
	}


	return o - dec;
}



char* http_create_header(http_request_t *http_request)
{
	char* response = malloc(HTTP_MAX_HEADERSIZE);
	if (response == NULL)
	{
		err_print("Can't allocate memory!");
		return NULL;
	}
	memcpy(response, "HTTP/1.1 ", 10);


	//Status Code
	int http_status_list_index = 0;
	int http_status_list_size = sizeof(http_status_list)/sizeof(http_status_list[0]);

	for(http_status_list_index = 0; http_status_list_index < http_status_list_size; http_status_list_index++){
		if(http_status_list[http_status_list_index].code == http_request->response_status)
		{
			break;
		}
	}
	if (http_status_list_index >= http_status_list_size)
	{
		err_print("Unknown status code!");
		free(response);response = NULL;
		return NULL;
	}
	sprintf(response + strlen(response), "%d %s\n", http_request->response_status, http_status_list[http_status_list_index].text);


	//Date
	struct tm *my_tm = gmtime(&(http_request->request_time));
	strftime(response + strlen(response), HTTP_MAX_HEADERSIZE - strlen(response), "Date: %a, %d %b %Y %H:%M:%S GMT\n", my_tm);


	//Server
	sprintf(response + strlen(response), "Server: %s\n", http_request->server);


	//Last-Modified
	if (http_request->last_modified != 0)
	{
		my_tm = gmtime(&(http_request->last_modified));
		strftime(response + strlen(response), HTTP_MAX_HEADERSIZE - strlen(response), "Last-Modified: %a, %d %b %Y %H:%M:%S GMT\n", my_tm);
	}


	//Accept-Ranges
	sprintf(response + strlen(response), "Accept-Ranges: bytes\n");


	//Content-Range
	if (http_request->response_status == HTTP_STATUS_PARTIAL_CONTENT)
	{
		if (http_request->range_end == 0)
		{
			http_request->range_end = http_request->file_size - 1;
		}
		sprintf(response + strlen(response), "Content-Range: bytes %u-%u/%d\n", http_request->range_begin, http_request->range_end, (int)http_request->file_size);
	}
	if (http_request->response_status == HTTP_STATUS_RANGE_NOT_SATISFIABLE)
	{
		sprintf(response + strlen(response), "Content-Range: bytes */%d\n", (int)http_request->file_size);
	}


	//Content-Length
	if (http_request->content_length > 0)
	{
		sprintf(response + strlen(response), "Content-Length: %d\n", (int)http_request->content_length);
	}


	//Content-Type
	sprintf(response + strlen(response), "Content-Type: %s\n", http_request->content_type);


	//Location
	if (http_request->location != NULL && strlen(http_request->location) > 0)
	{
		if (strlen(http_request->location)>(HTTP_MAX_HEADERSIZE - strlen(response) - 11 - 19 - 1))
		{
			err_print("Location is to long for the defined header size!");
			free(response);response = NULL;
			return NULL;
		}
		//TODO evtl. encode url?
		sprintf(response + strlen(response), "Location: %s\n", http_request->location);
	}


	//Connection
	sprintf(response + strlen(response), "Connection: close\n\n");
	return response;
}


int http_parse_header(http_request_t *http_request)
{

	if (http_request->request_buffer == NULL)
	{
		err_print("Header is NULL!");
		return -1;
	}

	//Set request time
	http_request->request_time = time(NULL);


	//Read HTTP method
	for (int i = 0; i < HTTP_METHOD_LIST_SIZE-1 ; i++)
	{
		if (memcmp(http_request->request_buffer, http_method_list[i].name, strlen(http_method_list[i].name)) == 0)
		{
			http_request->method = http_method_list[i].method;
			break;
		}
	}


	//Read Host
	char* host = strstr(http_request->request_buffer,"\r\nHost: ");
	size_t host_size = 0;
	if (host != NULL)
	{
		host += 8;
		char* host_end = strstr(host,"\r\n");
		host_size = host_end - host;
		http_request->host = calloc(host_size + 1, 1);//Set Null byte at the end of the string
		if (http_request->host != NULL)
		{
			//memset(header_struct.host, 0, host_size + 1);
			memcpy(http_request->host, host, host_size);
		}
		else
		{
			err_print("Can't allocate memory!");
		}

	}


	//Read URL
	char* url = strstr(http_request->request_buffer," ") + 1;
	char* url_end = strstr(url," ");
	size_t url_size = url_end - url;
	http_request->url = calloc(url_size + 1, 1);//Set Null byte at the end of the string
	if (http_request->url != NULL)
	{
		memcpy(http_request->url, url, url_size);
		http_decode_url(http_request->url, http_request->url);
		url_size = strlen(http_request->url);

		//If is absolute path, remove host part
		if (http_request->host != NULL && url_size >= host_size)
		{
			if (memcmp(http_request->url, http_request->host, host_size) == 0)
			{
				memmove(http_request->url, http_request->url + host_size, url_size - host_size + 1);
			}
		}

		//printf("HEADER_URL: '%s'\n", header_struct.url);
	}
	else
	{
		err_print("Can't allocate memory!");
	}


	//Read Range:
	char* range = strstr(http_request->request_buffer,"\r\nRange: bytes=");//Only bytes are accepted
	if (range != NULL)
	{
		range += 15;
		char* range_end = strstr(range,"\r\n");
		char* range_hyphen = strstr(range,"-");
		if (range_end != NULL && range_hyphen != NULL)
		{
			if (range_hyphen != range)//Does not beginn with '-' (start range is set)
			{
				sscanf(range, "%u", &(http_request->range_begin));
			}
			if ((range_hyphen+1) != range_end)//Does not end with '-' (end range is set)
			{
				sscanf(range_hyphen+1, "%u", &(http_request->range_end));
			}

			http_request->is_range_request = 1;
		}
	}


	//If modified since
	char* modified_since = strstr(http_request->request_buffer, "\r\nIf-Modified-Since: ");//Only bytes are accepted
	if (modified_since != NULL)
	{
		modified_since += 21;

		struct tm my_tm;
		memset(&my_tm,0,sizeof(my_tm)); //strptime does not fill everything... so we get an uninitalized memory problem with mktime... so just reset it first
		if (strptime(modified_since, "%a, %d %b %Y %H:%M:%S GMT", &my_tm) != NULL)
		{
			http_request->if_modified_since = mktime(&my_tm);
		}
	}


	return 0;
}

http_request_t http_create_struct()
{
	http_request_t http_request;
	//Input
	http_request.request_buffer = NULL;
	http_request.ip = NULL;
	http_request.request_time = 0;
	http_request.server = NULL;
	http_request.sd = 0;
	http_request.root_dir = NULL;

	//Parsed data
	http_request.method = HTTP_METHOD_UNKNOWN;
	http_request.url = NULL;
	http_request.range_begin = 0;
	http_request.range_end = 0;
	http_request.is_range_request = 0;
	http_request.host = NULL;
	http_request.if_modified_since = 0;

    //Response
	http_request.response_status = HTTP_STATUS_OK;
	http_request.last_modified = 0;
	http_request.content_type = "text/html";
	http_request.location = NULL;
	http_request.content_length = 0;
	http_request.file_size = 0;
	http_request.file_name = NULL;
	http_request.response_buffer = NULL;

	return http_request;
}

void http_free_struct(http_request_t *http_request)
{
	if (http_request->request_buffer != NULL)
	{
		free(http_request->request_buffer);
		http_request->request_buffer = NULL;
	}

/*	if (http_request->ip != NULL)
	{
		free(http_request->ip);
		http_request->ip = NULL;
	}*/

	if (http_request->url != NULL)
	{
		free(http_request->url);
		http_request->url = NULL;
	}

	if (http_request->host != NULL)
	{
		free(http_request->host);
		http_request->host = NULL;
	}

	if (http_request->location != NULL)
	{
		free(http_request->location);
		http_request->location = NULL;
	}

	if (http_request->file_name != NULL)
	{
		free(http_request->file_name);
		http_request->file_name = NULL;
	}

	if (http_request->response_buffer != NULL)
	{
		free(http_request->response_buffer);
		http_request->response_buffer = NULL;
	}

}






