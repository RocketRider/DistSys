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
#include <sys/stat.h>
#include "time.h"
#include "http.h"
#include "content.h"
#include "tinyweb.h"

#include "../libsockets/connect_tcp.h"
#include "../libsockets/passive_tcp.h"
#include "../libsockets/socket_io.h"
#include "../libsockets/socket_info.h"




int static server_answer_error_ex(int sd, int error_code, char* location) {
	int ret;
	char* html = "";

	//Status Code
	int http_status_list_index = 0;
	for (http_status_list_index = 0;
			http_status_list_index < HTTP_STATUS_LIST_SIZE;
			http_status_list_index++) {
		if (http_status_list[http_status_list_index].code == error_code) {
			html = http_status_list[http_status_list_index].html;
			break;
		}
	}

	int html_length = (int) strlen(html);
	char* response = http_create_header(error_code, SERV_NAME, NULL,
			"text/html", location, html_length, 0, 0, html_length);
	if (response == NULL) {
		perror("SERVER: create error response header failed!");
		return -1;
	}
	ret = write_to_socket(sd, response, strlen(response), SERV_WRITE_TIMEOUT);
	if (ret < 0) {
		perror("SERVER: failed to send error response header!");
		free(response);
		response = NULL;
		return -1;
	}
	if (html_length > 0) {
		ret = write_to_socket(sd, html, html_length, SERV_WRITE_TIMEOUT);
		if (ret < 0) {
			perror("SERVER: failed to send error response!");
			free(response);
			response = NULL;
			return -1;
		}
	}

	free(response);
	response = NULL;
	return 0;
}
int static server_answer_error(int sd, int error_code)
{
	return server_answer_error_ex(sd, error_code, " ");
}


int static server_answer(int sd, http_header_t request, char* root_dir) {
	int ret;

	//Check http method
	if (request.method == HTTP_METHOD_NOT_IMPLEMENTED
			|| request.method == HTTP_METHOD_UNKNOWN) {
		perror("SERVER: http method is not supported!");
		server_answer_error(sd, HTTP_STATUS_NOT_IMPLEMENTED);
		return -1;
	}


	//Check file
	struct stat sb;
	char* filename = malloc(
			strlen(root_dir) + strlen(request.url)
					+ 1);
	if (filename == NULL)
	{
		perror("SERVER: Allocate memory failed.\n");
		server_answer_error(sd, HTTP_STATUS_INTERNAL_SERVER_ERROR);
		return -1;
	}

	memcpy(filename, root_dir, strlen(root_dir) + 1);
	strcat(filename, request.url);
	printf("Check file '%s'\n", filename);
	if (stat(filename, &sb) == -1) {
		int err = errno;
		if (err == EACCES) {
			server_answer_error(sd, HTTP_STATUS_FORBIDDEN);
		} else {
			server_answer_error(sd, HTTP_STATUS_NOT_FOUND);
		}
		free(filename);
		filename = NULL;
		return -1;
	}
	if (S_ISDIR(sb.st_mode)) {
		if (*(request.url+strlen(request.url)-1) != '/')
		{
			//HOST + URL + index.html
			if (request.host != NULL && request.url != NULL)
			{
				char* newlocation = calloc(strlen(request.host) + strlen(request.url) + 7 + 11, 1);//2
				if (newlocation != NULL)
				{
					strcpy(newlocation, "http://");
					strcat(newlocation, request.host);
					strcat(newlocation, request.url);
					strcat(newlocation, "/");
					server_answer_error_ex(sd, HTTP_STATUS_MOVED_PERMANENTLY, newlocation);
					free(newlocation);
					newlocation = NULL;
				}
				else
				{
					perror("SERVER: Allocate memory failed.\n");
					server_answer_error(sd, HTTP_STATUS_INTERNAL_SERVER_ERROR);
				}
			}
			else
			{
				perror("SERVER: Host or url is not initalized\n");
				server_answer_error(sd, HTTP_STATUS_INTERNAL_SERVER_ERROR);
			}

			free(filename);
			filename = NULL;
			return -1;
		}
		else
		{
			free(filename);
			filename = NULL;

			request.url = realloc(request.url, strlen(request.url)+1+strlen(DEFAULT_HTML_PAGE));
			if (request.url == NULL)
			{
				perror("SERVER: Can't resize memory\n");
				server_answer_error(sd, HTTP_STATUS_INTERNAL_SERVER_ERROR);
				return -1;
			}
			strcat(request.url, DEFAULT_HTML_PAGE);
			return server_answer(sd, request, root_dir); //Retest the new file
		}
	}
	int html_length = (int) sb.st_size;
	int file_size = (int) sb.st_size;
	int http_code = HTTP_STATUS_OK;
	time_t mod_date = (time_t) sb.st_mtim.tv_sec;

	//Get mime type
	char* file_type = get_http_content_type_str(get_http_content_type(filename));


	//Check modified since
	if (request.if_modified_since != 0 )
	{
		if (difftime(request.if_modified_since, mod_date) >= 0)
		{
			server_answer_error(sd, HTTP_STATUS_NOT_MODIFIED);
			free(filename);
			filename = NULL;
			return -1;
		}
	}

	//Check Range
	if (request.is_range_request == 1) //Is Range request
	{
		if (request.range_begin >= request.range_end && request.range_end != 0)//If range end is set, it has to be bigger than begin
		{
			perror("SERVER: range end is before range begin!");
			server_answer_error(sd, HTTP_STATUS_RANGE_NOT_SATISFIABLE);
			free(filename);
			filename = NULL;
			return -1;
		}
		if (request.range_end > html_length || request.range_begin >= html_length)
		{
			perror("SERVER: range is out of file!");
			server_answer_error(sd, HTTP_STATUS_RANGE_NOT_SATISFIABLE);
			free(filename);
			filename = NULL;
			return -1;
		}
		if (request.range_end > 0)
		{
			html_length = request.range_end - request.range_begin;
		}
		else
		{
			html_length = file_size - request.range_begin;
		}
		http_code = HTTP_STATUS_PARTIAL_CONTENT;
		
	}

	//Create header
	char* response = http_create_header(http_code, SERV_NAME, &mod_date, file_type, "", html_length, request.range_begin, request.range_end, file_size);
	if (response == NULL) {
		perror("SERVER: create response header failed!");
		server_answer_error(sd, HTTP_STATUS_INTERNAL_SERVER_ERROR);
		free(filename);
		filename = NULL;
		return -1;
	}

	//Open file
	FILE *fp = fopen(filename, "r"); //open in read mode
	if (fp == NULL) {
		perror("SERVER: Error while opening file.\n");
		server_answer_error(sd, HTTP_STATUS_FORBIDDEN);
		free(response);
		response = NULL;
		free(filename);
		filename = NULL;
		return -1;
	}

	//Allocate send data buffer
	char* buffer = (char*) malloc(SERV_SEND_BUFFER_SIZE);
	if (buffer == NULL) {
		perror("SERVER: can't allocate memory!");
		server_answer_error(sd, HTTP_STATUS_INTERNAL_SERVER_ERROR);
		free(response);
		response = NULL;
		free(filename);
		filename = NULL;
		fclose(fp);
		return -1;
	}

	//Send response header:
	ret = write_to_socket(sd, response, strlen(response), SERV_WRITE_TIMEOUT);
	if (ret < 0) {
		perror("SERVER: failed to send response header!");
		free(response);
		response = NULL;
		free(filename);
		filename = NULL;
		free(buffer);
		buffer = NULL;
		fclose(fp);
		return -1;
	}


	//Send response body
	fseek(fp, request.range_begin, SEEK_SET);
	int read_bytes = 0;
	int send_bytes = 0;
	if (html_length > 0 && request.method != HTTP_METHOD_HEAD) {
		do{
			//If Range is specified, only read the needed part of the file:
			int to_read_bytes = html_length;
			if (request.range_end > 0)//End is specified
			{
				to_read_bytes -= send_bytes;
			}
			if (to_read_bytes > SERV_SEND_BUFFER_SIZE) //Max read with buffer size
			{
				to_read_bytes = SERV_SEND_BUFFER_SIZE;
			}

			read_bytes = fread(buffer, 1, to_read_bytes, fp);
			send_bytes += read_bytes;
			if (read_bytes > 0)
			{
				ret = write_to_socket(sd, buffer, read_bytes, SERV_WRITE_TIMEOUT);
				if (ret < 0) {
					perror("SERVER: failed to send response!");
					free(response);
					response = NULL;
					free(filename);
					filename = NULL;
					free(buffer);
					buffer = NULL;
					fclose(fp);
					return -1;
				}
			}
		}
		while (read_bytes == SERV_SEND_BUFFER_SIZE);
	}

	//Free used resources
	fclose(fp);
	free(buffer);
	buffer = NULL;
	free(response);
	response = NULL;
	free(filename);
	filename = NULL;

	return 0;
}



int server_handle_client(int sd, char* root_dir) {
	char buf[HTTP_MAX_HEADERSIZE] = "";
	int cc = 0; //char count
	struct socket_info info;
	int ret;

	get_socket_name(sd, &info);

	clock_t t1, t2; //Make sure the max timeout is not more then 2x SERV_READ_TIMEOUT
	float diff;
	t1 = clock();
	do {
		ret = read_from_socket(sd, buf + cc, HTTP_MAX_HEADERSIZE - cc,
				SERV_READ_TIMEOUT);
		if (ret > 0)
		{
			cc += ret;
		}
		t2 = clock();
		diff = (((float) t2 - (float) t1) / CLOCKS_PER_SEC);
	} while (ret >= 0 && strstr(buf, "\r\n\r\n") == NULL
			&& cc < HTTP_MAX_HEADERSIZE && diff < SERV_READ_TIMEOUT);


	if (ret < 0 || cc <= 0) {
		perror("SERVER: receiving request failed!");
		server_answer_error(sd, HTTP_STATUS_BAD_REQUEST);
	} else {
		printf("Request: \n%.*s\n", cc, buf);
		http_header_t request = http_parse_header(buf);
		ret = server_answer(sd, request, root_dir);
		free(request.url);
		free(request.host);
	}

	close(sd);
	return sd;
}



int server_accept_clients(int sd, char* root_dir) {
	int retcode = 0, newsd; //new socket descriptor
	struct sockaddr_in from_client;
	socklen_t from_client_len = 0;


	from_client_len = sizeof(from_client);
	newsd = accept(sd, /*INOUT*/(struct sockaddr*) &from_client, /*INOUT*/
			&from_client_len);
	printf("new client\n");
	if (newsd < 0) //Server Problem
			{
		retcode = newsd;
		//TODO: Error!!
	} else {
		int pid = -1;
		if ((pid = fork()) < 0) {
			perror("fork()");
			exit(1);

		} else if (pid == 0)/* child process */
		{
			close(sd);
			server_handle_client(newsd, root_dir);
			retcode = 1;
		} else /* parent process */
		{
			close(newsd);
		}
	}


	return retcode;
}



int server_start(int port) {
	int sd;

	sd = passive_tcp(port, SERV_QLEN);
	if (sd < 0) {
		perror("Failed to create socket!\n");
		//TODO: ERROR
	}

	return sd;
}
