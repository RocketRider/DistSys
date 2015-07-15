/*===================================================================
 * DHBW Ravensburg - Campus Friedrichshafen
 *
 * Vorlesung Verteilte Systeme
 *
 * Authoren:  Ralf Reutemann
 * 			  Michael Möbius (person in charge)
 * 			  Maximilian Schmitz
 *
 * Modul: Verwaltung der Socket verbindung zum Client und Abarbeitung der Requests
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
#include <sys/stat.h>
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
#include <unistd.h>
#include "time.h"
#include "http.h"
#include "content.h"
#include "tinyweb.h"
#include "sem_print.h"
#include "../libsockets/connect_tcp.h"
#include "../libsockets/passive_tcp.h"
#include "../libsockets/socket_io.h"
#include "../libsockets/socket_info.h"

/*
 * Name: server_send_response_header
 * Zweck: Sendet Http Response Header
 * In-Parameter: http_request_t *request (Request inklusiv Response Header)
 * Out-Parameter: -
 * Globale Variablen: -
 * Rückgabewert: int Statuscode (Anzahl geschriebener Zeichen, bei Fehler negativ)
 */
int static server_send_response_header(http_request_t *request) {
	int ret = -1;

	if (request->request_buffer != NULL && request->ip != NULL) {
		size_t req_len = strstr(request->request_buffer, "\r\n")
				- request->request_buffer;

		char time[64] = "";
		struct tm *my_tm = (struct tm*) calloc(sizeof(struct tm), 1);
		localtime_r(&(request->request_time), my_tm);
		strftime(time, 64, "%d/%b/%Y:%H:%M:%S %z", my_tm);
		free(my_tm);

		print_log("%s - - %s \"%.*s\" %d %d\n", request->ip, time, req_len,
				request->request_buffer, request->response_status,
				request->content_length);
	} else {
		err_print("No request buffer received!");
	}

	if (request->response_buffer != NULL) {
		print_http_header("RESPONSE", request->response_buffer);
		ret = write_to_socket(request->sd, request->response_buffer,
				strlen(request->response_buffer), SERV_WRITE_TIMEOUT);
	} else {
		err_print("No response buffer to send!");
	}
	return ret;
}

/*
 * Name: server_answer_error
 * Zweck: Sendet Http Error Response
 * In/Out-Parameter: http_request_t *request (Request der mit Fehler beantwortet wird)
 * Globale Variablen: -
 * Rückgabewert: int Statuscode (-1 = Fehler, 0 = ok)
 */
int static server_answer_error(http_request_t *request) {
	int ret;
	char* html = "";

	//If main code already generated a header, and then happened a error, so we must free the old one first...
	if (request->response_buffer != NULL) {
		free(request->response_buffer);
		request->response_buffer = NULL;
	}

	//Status Code
	int http_status_list_index = 0;
	for (http_status_list_index = 0;
			http_status_list_index < HTTP_STATUS_LIST_SIZE;
			http_status_list_index++) {
		if (http_status_list[http_status_list_index].code
				== request->response_status) {
			html = http_status_list[http_status_list_index].html;
			break;
		}
	}

	request->content_length = (int) strlen(html);
	request->file_size = request->content_length;
	request->response_buffer = http_create_header(request);
	if (request->response_buffer == NULL) {
		err_print("Create error response header failed!");
		return -1;
	}

	ret = server_send_response_header(request);
	if (ret < 0) {
		print_debug("Failed to send error response header!\n"); //Connection is closed
		return -1;
	}
	if (request->content_length > 0) {
		ret = write_to_socket(request->sd, html, request->content_length,
				SERV_WRITE_TIMEOUT); //Connection is closed
		if (ret < 0) {
			print_debug("Failed to send error response!\n");
			return -1;
		}
	}
	return 0;
}

/*
 * Name: server_execute_cgi
 * Zweck: Ausführen einer CGI Anfrage
 * In/Out-Parameter: http_request_t *request (erhaltener Http Request)
 * Globale Variablen: -
 * Rückgabewert: int Statuscode (-1 = Fehler, 0 = ok)
 */
int static server_execute_cgi(http_request_t *request) //'Is executable' is already checked!
{
	request->response_buffer = malloc(HTTP_MAX_HEADERSIZE);
	if (request->response_buffer == NULL) {
		err_print("Can't allocate memory!");
		return -1;
	}
	memcpy(request->response_buffer, "HTTP/1.1 ", 10);
	sprintf(request->response_buffer + strlen(request->response_buffer),
			"%d %s\n", HTTP_STATUS_OK, "OK");

	//Date
	struct tm *my_tm = gmtime(&(request->request_time));
	strftime(request->response_buffer + strlen(request->response_buffer),
			HTTP_MAX_HEADERSIZE - strlen(request->response_buffer),
			"Date: %a, %d %b %Y %H:%M:%S GMT\n", my_tm);

	//Server
	sprintf(request->response_buffer + strlen(request->response_buffer),
			"Server: %s\n", SERV_NAME);

	//Send response header:
	int ret = server_send_response_header(request);
	if (ret < 0) {
		err_print("Failed to send response header!");
		return -1;
	}

	int saved_stdout = dup(1);
	dup2(request->sd, STDOUT_FILENO);
	execl(request->file_name, request->file_name, NULL);

	//execl failed:
	dup2(saved_stdout, STDOUT_FILENO);
	err_print("execl failed!");
	request->response_status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
	server_answer_error(request);
	return -1;
}

/*
 * Name: server_answer
 * Zweck: Erzeugen und Senden der Http Response des Servers
 * In/Out-Parameter: http_request_t *request (erhaltener Http Request)
 * Globale Variablen: -
 * Rückgabewert: int Statuscode (-1 = Fehler, 0 = ok)
 */
int static server_answer(http_request_t *request) {
	int ret;

	//If these buffers are already used, we need to free the memory first...
	if (request->file_name != NULL) {
		free(request->file_name);
		request->file_name = NULL;
	}
	if (request->location != NULL) {
		free(request->location);
		request->location = NULL;
	}
	if (request->response_buffer != NULL) {
		free(request->response_buffer);
		request->response_buffer = NULL;
	}

	//Check http method
	if (request->method == HTTP_METHOD_NOT_IMPLEMENTED
			|| request->method == HTTP_METHOD_UNKNOWN) {
		print_debug("HTTP method is not supported!\n");
		request->response_status = HTTP_STATUS_NOT_IMPLEMENTED;
		server_answer_error(request);
		return -1;
	}

	//Check file
	struct stat sb;
	request->file_name = malloc(
			strlen(request->root_dir) + strlen(request->url) + 1);
	if (request->file_name == NULL) {
		err_print("Allocate memory failed!");
		request->response_status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
		server_answer_error(request);
		return -1;
	}
	memcpy(request->file_name, request->root_dir,
			strlen(request->root_dir) + 1);
	strcat(request->file_name, request->url);
	//printf("Check file '%s'\n", filename);
	if (stat(request->file_name, &sb) == -1) {
		int err = errno;
		if (err == EACCES) {
			print_debug("File access is prohibited!\n");
			request->response_status = HTTP_STATUS_FORBIDDEN;
			server_answer_error(request);
		} else {
			request->response_status = HTTP_STATUS_NOT_FOUND;
			server_answer_error(request);
		}
		return 0;
	}

	//Requestet url is a directory?
	if (S_ISDIR(sb.st_mode)) {
		if (*(request->url + strlen(request->url) - 1) != '/') {
			//HOST + URL + index.html
			if (request->host != NULL && request->url != NULL) {
				request->location = calloc(
						strlen(request->host) + strlen(request->url) + 7 + 11,
						1); //2
				if (request->location != NULL) {
					strcpy(request->location, "http://");
					strcat(request->location, request->host);
					strcat(request->location, request->url);
					strcat(request->location, "/");
					request->response_status = HTTP_STATUS_MOVED_PERMANENTLY;
					server_answer_error(request);
					return 0;
				} else {
					err_print("Allocate memory failed!");
					request->response_status =
							HTTP_STATUS_INTERNAL_SERVER_ERROR;
					server_answer_error(request);
				}
			} else {
				err_print("Host or url is not initalized!");
				request->response_status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
				server_answer_error(request);
			}

			return -1;
		} else {
			request->url = realloc(request->url,
					strlen(request->url) + 1 + strlen(DEFAULT_HTML_PAGE));
			if (request->url == NULL) {
				err_print("Can't resize memory!");
				request->response_status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
				server_answer_error(request);
				return -1;
			}
			strcat(request->url, DEFAULT_HTML_PAGE);
			return server_answer(request); //Retest the new file
		}
	}
	request->content_length = (int) sb.st_size;
	request->file_size = (int) sb.st_size;
	request->last_modified = (time_t) sb.st_mtim.tv_sec;

	//Test for cgi-bin path
	if (memcmp(request->url, "/cgi-bin/", 9) == 0) {
		if (S_IXOTH & sb.st_mode) {
			ret = server_execute_cgi(request);
			return ret;
		} else {
			print_debug("CGI script is not executable\n");
			request->response_status = HTTP_STATUS_FORBIDDEN;
			server_answer_error(request);
			return 0;
		}
	}

	//Get mime type
	request->content_type = get_http_content_type_str(
			get_http_content_type(request->file_name));

	//Check modified since
	if (request->if_modified_since != 0 && request->last_modified != 0) {
		if (difftime(request->if_modified_since, request->last_modified) >= 0) {
			request->response_status = HTTP_STATUS_NOT_MODIFIED;
			server_answer_error(request);
			return 0;
		}
	}

	//Check Range
	if (request->is_range_request == 1) //Is Range request
			{
		if (request->range_begin >= request->range_end
				&& request->range_end != 0) //If range end is set, it has to be bigger than begin
						{
			print_debug("Range end is before range begin!\n");
			request->response_status = HTTP_STATUS_RANGE_NOT_SATISFIABLE;
			server_answer_error(request);
			return -1;
		}
		if (request->range_end > request->content_length
				|| request->range_begin >= request->content_length) {
			print_debug("Range is out of file!\n");
			request->response_status = HTTP_STATUS_RANGE_NOT_SATISFIABLE;
			server_answer_error(request);
			return -1;
		}
		if (request->range_end > 0) {
			request->content_length = request->range_end - request->range_begin;
		} else {
			request->content_length = request->file_size - request->range_begin;
		}
		request->response_status = HTTP_STATUS_PARTIAL_CONTENT;

	}

	//Create header
	request->response_buffer = http_create_header(request);
	if (request->response_buffer == NULL) {
		request->response_status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
		server_answer_error(request);
		return -1;
	}

	//Open file
	FILE *fp = fopen(request->file_name, "r"); //open in read mode
	if (fp == NULL) {
		print_debug("Error while opening file!\n");
		request->response_status = HTTP_STATUS_FORBIDDEN;
		server_answer_error(request);
		return -1;
	}

	//Allocate send data buffer
	char* buffer = (char*) malloc(SERV_SEND_BUFFER_SIZE);
	if (buffer == NULL) {
		err_print("Can't allocate memory!");
		request->response_status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
		server_answer_error(request);
		fclose(fp);
		return -1;
	}

	//Send response header:
	ret = server_send_response_header(request);
	if (ret < 0) {
		err_print("Failed to send response header!");
		free(buffer);
		buffer = NULL;
		fclose(fp);
		return -1;
	}

	//Send response body
	fseek(fp, request->range_begin, SEEK_SET);
	int read_bytes = 0;
	int send_bytes = 0;
	if (request->content_length > 0 && request->method != HTTP_METHOD_HEAD) {
		do {
			//If Range is specified, only read the needed part of the file:
			int to_read_bytes = request->content_length;
			if (request->range_end > 0) //End is specified
					{
				to_read_bytes -= send_bytes;
			}
			if (to_read_bytes > SERV_SEND_BUFFER_SIZE) //Max read with buffer size
			{
				to_read_bytes = SERV_SEND_BUFFER_SIZE;
			}

			read_bytes = fread(buffer, 1, to_read_bytes, fp);
			send_bytes += read_bytes;
			if (read_bytes > 0) {
				ret = write_to_socket(request->sd, buffer, read_bytes,
						SERV_WRITE_TIMEOUT);
				if (ret < 0) {
					err_print("Failed to send response!");
					free(buffer);
					buffer = NULL;
					fclose(fp);
					return -1;
				}
			}
		} while (read_bytes == SERV_SEND_BUFFER_SIZE);
	}

	//Free used resources
	free(buffer);
	buffer = NULL;
	fclose(fp);
	return 0;
}

/*
 * Name: server_handle_client
 * Zweck: Abarbeiten der Requests eines Clients
 * In/Out-Parameter: http_request_t *request (erhaltener Http Request)
 * Globale Variablen: -
 * Rückgabewert: int Statuscode (-1 = Fehler, 0 = ok)
 */
int server_handle_client(http_request_t *http_request) {
	int ret = 0;
	char* buf = calloc(HTTP_MAX_HEADERSIZE, 1);
	if (buf != NULL) {
		int cc = 0; //char count
		struct socket_info info;

		get_socket_name(http_request->sd, &info);

		clock_t t1, t2; //Make sure the max timeout is not more then 2x SERV_READ_TIMEOUT
		float diff;
		t1 = clock();
		do {
			ret = read_from_socket(http_request->sd, buf + cc,
					HTTP_MAX_HEADERSIZE - cc, SERV_READ_TIMEOUT);
			if (ret > 0) {
				cc += ret;
			}
			t2 = clock();
			diff = (((float) t2 - (float) t1) / CLOCKS_PER_SEC);
		} while (ret >= 0 && strstr(buf, "\r\n\r\n") == NULL
				&& cc < HTTP_MAX_HEADERSIZE && diff < SERV_READ_TIMEOUT);

		if (ret < 0 || cc <= 0) {
			print_debug("Receiving request failed!\n");
			http_request->response_status = HTTP_STATUS_BAD_REQUEST;
			server_answer_error(http_request);
			free(buf);
			buf = NULL;
			ret = -1;
		} else {
			print_http_header("REQUEST", buf);
			http_request->request_buffer = buf; //buffer will be fread from http_free_struct
			http_parse_header(http_request);
			ret = server_answer(http_request);
		}

		close(http_request->sd);
	} else {
		err_print("Failed to allocate memory!");
		ret = -1;
	}
	return ret;
}

/*
 * Name: server_accept_clients
 * Zweck: Nimmt Client-Verbindungen an und führt fork aus
 * In-Parameter: int sd (Socketdeskriptor)
 char *root_dir (Pfad des Wurzelverzeichnis)
 * Out-Parameter: -
 * Globale Variablen: -
 * Rückgabewert: int Statuscode (0=ok, 1=parent(accept next), -1=Fehler)
 */
int server_accept_clients(int sd, char* root_dir) {
	int retcode = 0, newsd; //new socket descriptor
	struct sockaddr_in6 from_client;
	socklen_t from_client_len = 0;

	from_client_len = sizeof(from_client);
	newsd = accept(sd, /*INOUT*/(struct sockaddr*) &from_client, /*INOUT*/
	&from_client_len);
	print_debug("New client\n");
	if (newsd < 0) //Server Problem
			{
		print_debug("Failed to accept client! / Server gets shut down... \n");
		retcode = 0;
	} else {
		int pid = -1;
		if ((pid = fork()) < 0) {
			err_print("fork failed!");
			exit(1);

		} else if (pid == 0)/* child process */
		{
			close(sd);
			http_request_t http_request = http_create_struct();

			struct socket_info info;
			//get_socket_name(newsd, &info);
			get_socket_info(from_client, &info);
			http_request.ip = info.addr;

			http_request.server = SERV_NAME;
			http_request.sd = newsd;
			http_request.root_dir = root_dir;
			retcode = server_handle_client(&http_request);
			http_free_struct(&http_request);
		} else /* parent process */
		{
			close(newsd);
			retcode = 1;
		}
	}

	return retcode;
}

/*
 * Name: server_start
 * Zweck: Starten des Servers
 * In-Parameter: int port
 * Out-Parameter: -
 * Globale Variablen: -
 * Rückgabewert: int Socketdeskriptor des Servers (-1 bei Fehler)
 */
int server_start(int port) {
	int sd;

	sd = passive_tcp(port, SERV_QLEN);
	if (sd < 0) {
		err_print("Failed to create socket!");
		return -1;
	}

	return sd;
}
