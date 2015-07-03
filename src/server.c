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

#include "../libsockets/connect_tcp.h"
#include "../libsockets/passive_tcp.h"
#include "../libsockets/socket_io.h"
#include "../libsockets/socket_info.h"

int static server_answer_error(int sd, int error_code) {
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
			"text/html", " ", html_length);
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

int static server_answer(int sd, http_header_t request, char* root_dir) {
	int ret;

	if (request.method == HTTP_METHOD_NOT_IMPLEMENTED
			|| request.method == HTTP_METHOD_UNKNOWN) {
		perror("SERVER: http method is not supported!");
		server_answer_error(sd, HTTP_STATUS_NOT_IMPLEMENTED);
		return -1;
	}

	struct stat sb;
	char* filename = malloc(
			strlen(root_dir) * sizeof(char) + strlen(request.url) * sizeof(char)
					+ sizeof(char));
	memcpy(filename, root_dir, strlen(root_dir) * sizeof(char) + sizeof(char));
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
		//TODO: send new url...
		server_answer_error(sd, HTTP_STATUS_MOVED_PERMANENTLY);
		free(filename);
		filename = NULL;
		return -1;
	}
	int html_length = (int) sb.st_size;
	time_t mod_date = (time_t) sb.st_mtim.tv_sec;

	//int html_length = (int)strlen(html);
	char* response = http_create_header(200, SERV_NAME, &mod_date, "text/html",
			" ", html_length);
	if (response == NULL) {
		perror("SERVER: create response header failed!");
		server_answer_error(sd, HTTP_STATUS_INTERNAL_SERVER_ERROR);
		free(filename);
		filename = NULL;
		return -1;
	}

	//printf("Response:\n");

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

	char* buffer = (char*) malloc(html_length);
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

	//copy the file into the buffer:
	int result = fread(buffer, 1, html_length, fp);
	if (result != html_length) {
		perror("SERVER: create response header failed!");
		server_answer_error(sd, HTTP_STATUS_INTERNAL_SERVER_ERROR);
		free(response);
		response = NULL;
		free(filename);
		filename = NULL;
		fclose(fp);
		return -1;
	}
	fclose(fp);

	ret = write_to_socket(sd, response, strlen(response), SERV_WRITE_TIMEOUT);
	if (ret < 0) {
		perror("SERVER: failed to send error response header!");
		free(response);
		response = NULL;
		free(filename);
		filename = NULL;
		return -1;
	}

	if (html_length > 0 && request.method != HTTP_METHOD_HEAD) {
		ret = write_to_socket(sd, buffer, html_length, SERV_WRITE_TIMEOUT);
		if (ret < 0) {
			perror("SERVER: failed to send error response!");
			free(response);
			response = NULL;
			free(filename);
			filename = NULL;
			return -1;
		}
	}
	free(buffer);

	//printf("%.*s", (int)strlen(response), response);
	//printf("%.*s", (int)strlen(html), html);

	free(response);
	response = NULL;
	free(filename);
	filename = NULL;

	return 0;
}

int server_handle_client(int sd, char* root_dir) {
	char buf[HTTP_MAX_HEADERSIZE] = "";
	int cc = 0; //charachter count
	struct socket_info info;
	int ret;

	printf("New Clinet...\n");
	get_socket_name(sd, &info);
	printf("IP: %s, Port: %d\n\n", info.addr, info.port);

	clock_t t1, t2; //Make sure the max timeout is not more then 2x SERV_READ_TIMEOUT
	float diff;
	t1 = clock();
	do {
		ret = read_from_socket(sd, buf + cc, HTTP_MAX_HEADERSIZE - cc,
				SERV_READ_TIMEOUT);
		cc += ret;
		t2 = clock();
		diff = (((float) t2 - (float) t1) / CLOCKS_PER_SEC);
	} while (ret >= 0 && strstr(buf, "\r\n\r\n") == NULL
			&& cc < HTTP_MAX_HEADERSIZE && diff < SERV_READ_TIMEOUT);
	printf("Time to read the request: %f sec\n", diff);

	if (ret < 0 || cc <= 0) {
		perror("SERVER: receiving request failed!");
		server_answer_error(sd, HTTP_STATUS_BAD_REQUEST);
	} else {
		printf("Request: \n%.*s\n", cc, buf);

		//TODO Parse HTTP Header
		http_header_t request = http_parse_header(buf);
		ret = server_answer(sd, request, root_dir);
		free(request.url);
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
