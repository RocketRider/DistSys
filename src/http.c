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



char* http_create_header(int status_code, char* server, time_t* last_modified, char* content_type, char* location, int content_length, unsigned int range_begin, unsigned int range_end, int file_size)
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
		if(http_status_list[http_status_list_index].code == status_code)
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


	//Content-Range
	if (status_code == HTTP_STATUS_PARTIAL_CONTENT)
	{
		if (range_end == 0)
		{
			range_end = file_size - 1;
		}
		sprintf(response + strlen(response), "Content-Range: bytes %u-%u/%d\n", range_begin, range_end, file_size);
	}
	if (status_code == HTTP_STATUS_RANGE_NOT_SATISFIABLE)
	{
		sprintf(response + strlen(response), "Content-Range: bytes */%d\n", file_size);
	}


	//Content-Length
	if (content_length > 0)
	{
		sprintf(response + strlen(response), "Content-Length: %d\n", content_length);
	}


	//Content-Type
	sprintf(response + strlen(response), "Content-Type: %s\n", content_type);


	//Location
	if (location != NULL && strlen(location) > 0)
	{
		if (strlen(location)>(HTTP_MAX_HEADERSIZE - strlen(response) - 11 - 19 - 1))
		{
			err_print("Location is to long for the defined header size!");
			free(response);response = NULL;
			return NULL;
		}
		//TODO evtl. encode url?
		sprintf(response + strlen(response), "Location: %s\n", location);
	}


	//Connection
	sprintf(response + strlen(response), "Connection: close\n\n");
	return response;
}


http_header_t http_parse_header(char * header)
{
	http_header_t header_struct;
	header_struct.range_begin = 0;	//From byte 0
	header_struct.range_end = 0;	//To the end of the file (0 => end of file)
	header_struct.is_range_request = 0;
	header_struct.method = HTTP_METHOD_UNKNOWN;
	header_struct.url = NULL;
	header_struct.host = NULL;
	header_struct.if_modified_since = 0;

	//Read HTTP method
	for (int i = 0; i < HTTP_METHOD_LIST_SIZE-1 ; i++)
	{
		if (memcmp(header, http_method_list[i].name, strlen(http_method_list[i].name)) == 0)
		{
			header_struct.method = http_method_list[i].method;
			break;
		}
	}


	//Read Host
	char* host = strstr(header,"\r\nHost: ");
	size_t host_size = 0;
	if (host != NULL)
	{
		host += 8;
		char* host_end = strstr(host,"\r\n");
		host_size = host_end - host;
		header_struct.host = calloc(host_size + 1, 1);//Set Null byte at the end of the string
		if (header_struct.host != NULL)
		{
			//memset(header_struct.host, 0, host_size + 1);
			memcpy(header_struct.host, host, host_size);
		}
		else
		{
			err_print("Can't allocate memory!");
		}

	}


	//Read URL
	char* url = strstr(header," ") + 1;
	char* url_end = strstr(url," ");
	size_t url_size = url_end - url;
	header_struct.url = calloc(url_size + 1, 1);//Set Null byte at the end of the string
	if (header_struct.url != NULL)
	{
		memcpy(header_struct.url, url, url_size);
		http_decode_url(header_struct.url, header_struct.url);
		url_size = strlen(header_struct.url);

		//If is absolute path, remove host part
		if (header_struct.host != NULL && url_size >= host_size)
		{
			if (memcmp(header_struct.url, header_struct.host, host_size) == 0)
			{
				memmove(header_struct.url, header_struct.url + host_size, url_size - host_size + 1);
			}
		}

		//printf("HEADER_URL: '%s'\n", header_struct.url);
	}
	else
	{
		err_print("Can't allocate memory!");
	}


	//Read Range:
	char* range = strstr(header,"\r\nRange: bytes=");//Only bytes are accepted
	if (range != NULL)
	{
		range += 15;
		char* range_end = strstr(range,"\r\n");
		char* range_hyphen = strstr(range,"-");
		if (range_end != NULL && range_hyphen != NULL)
		{
			if (range_hyphen != range)//Does not beginn with '-' (start range is set)
			{
				sscanf(range, "%u", &header_struct.range_begin);
			}
			if ((range_hyphen+1) != range_end)//Does not end with '-' (end range is set)
			{
				sscanf(range_hyphen+1, "%u", &header_struct.range_end);
			}

			header_struct.is_range_request = 1;
		}
	}


	//If modified since
	char* modified_since = strstr(header,"\r\nIf-Modified-Since: ");//Only bytes are accepted
	if (modified_since != NULL)
	{
		modified_since += 21;

		struct tm my_tm;
		strptime(modified_since, "%a, %d %b %Y %H:%M:%S GMT", &my_tm);
		header_struct.if_modified_since = mktime(&my_tm);


	}


	return header_struct;
}



