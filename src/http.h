/*===================================================================
 * DHBW Ravensburg - Campus Friedrichshafen
 *
 * Vorlesung Verteilte Systeme
 *
 * Author:  Ralf Reutemann
 *
 *===================================================================*/

#ifndef _HTTP_H
#define _HTTP_H

#define HTTP_MAX_HEADERSIZE					8190 //Also used by apache server

typedef enum http_method {
    HTTP_METHOD_GET = 0,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_TEST,
    HTTP_METHOD_ECHO,
    HTTP_METHOD_NOT_IMPLEMENTED,
    HTTP_METHOD_UNKNOWN
} http_method_t;


typedef enum http_status {
    HTTP_STATUS_OK					 	= 200,
    HTTP_STATUS_PARTIAL_CONTENT		 	= 206,
    HTTP_STATUS_MOVED_PERMANENTLY	 	= 301,
    HTTP_STATUS_NOT_MODIFIED		 	= 304,
    HTTP_STATUS_BAD_REQUEST			 	= 400,
    //HTTP_STATUS_FORBIDDEN			 	= 401,
    HTTP_STATUS_FORBIDDEN			 	= 403,
    HTTP_STATUS_NOT_FOUND				= 404,
    HTTP_STATUS_RANGE_NOT_SATISFIABLE	= 416,
    HTTP_STATUS_INTERNAL_SERVER_ERROR 	= 500,
    HTTP_STATUS_NOT_IMPLEMENTED 		= 501
} http_status_t;


typedef struct http_method_entry {
    char          *name;
    http_method_t  method;
} http_method_entry_t;


typedef struct http_status_entry {
    unsigned short   code;
    char            *text;
    char			*html;
} http_status_entry_t;


typedef struct http_header {
	http_method_t   method;
    char            *url;
    unsigned int	range_begin;
    unsigned int	range_end;
    int				is_range_request;
    char			*host;
    time_t			if_modified_since;
} http_header_t;



extern http_method_entry_t http_method_list[];
extern http_status_entry_t http_status_list[];
extern const int HTTP_STATUS_LIST_SIZE;
extern char* http_create_header(int status_code, char* server, time_t* last_modified, char* content_type, char* location, int html_length, unsigned int range_begin, unsigned int range_end, int file_size);
extern http_header_t http_parse_header(char * header);

#endif

