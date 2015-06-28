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
	#define SERV_BUFSIZE				1024
	#define SERV_WRITE_TIMEOUT			5 			//timeout for 'select()' in seconds
	#define SERV_NAME					"Tinyweb"
	
	extern int server_accept_clients(int sd);
	extern int server_start(int port);

#endif

