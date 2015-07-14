/*===================================================================
 * DHBW Ravensburg - Campus Friedrichshafen
 *
 * Vorlesung Verteilte Systeme
 *
 * Author:  Ralf Reutemann
 *
 *===================================================================*/


#ifndef _SERVER_H
#define _SERVER_H
	#define SERV_QLEN					8
	#define SERV_WRITE_TIMEOUT			5 			//timeout for 'select()' in seconds
	#define SERV_READ_TIMEOUT			5 			//timeout for 'select()' in seconds
	#define SERV_NAME					"Tinyweb"
	#define SERV_SEND_BUFFER_SIZE		1024
	
	extern int server_accept_clients(int sd, char* root_dir);
	extern int server_start(int port);

#endif

