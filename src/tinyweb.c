/*===================================================================
 * DHBW Ravensburg - Campus Friedrichshafen
 *
 * Vorlesung Verteilte Systeme
 *
 * Authoren:  Ralf Reutemann (person in charge)
 * 			  Michael Möbius
 * 			  Maximilian Schmitz
 *
 * Modul: Hauptprogram, verarbeitet Parameter, startet den server und sammelt Kindprozesse wieder ein
 *
 *===================================================================*/
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <netdb.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <getopt.h>

#include "tinyweb.h"
#include "connect_tcp.h"
#include "passive_tcp.h"

#include "safe_print.h"
#include "sem_print.h"
#include "server.h"

// Must be true for the server accepting clients,
// otherwise, the server will terminate
static volatile sig_atomic_t server_running = false;

#define IS_ROOT_DIR(mode)   (S_ISDIR(mode) && ((S_IROTH || S_IXOTH) & (mode)))

/*
 * Name: sig_handler
 * Zweck: Signalhandler, um auf Signale zu reagieren
 * In-Parameter: int sig (Signal, auf das reagiert werden soll)
 * Out-Parameter: -
 * Globale Variablen: -
 * Rückgabewert: -
 */
static void sig_handler(int sig) {
	switch (sig) {
	case SIGINT:
		// use our own thread-safe implemention of printf
		safe_printf("\n[%d] Server terminated due to keyboard interrupt\n",
				getpid());
		server_running = false;
		break;
		// TODO: Complete signal handler
	default:
		break;
	} /* end switch */
} /* end of sig_handler */

/*
 * Name: print_usage
 * Zweck: Hilfeausgabe für Startoptionen des Servers
 * In-Parameter: const char *progname (Name des Programms)
 * Out-Parameter: -
 * Globale Variablen: -
 * Rückgabewert: -
 */
static void print_usage(const char *progname) {
	fprintf(stderr, "Usage: %s options\n", progname);

	fprintf(stderr, "-d dir\n%s\n\n",
			"Definiert die Wurzel des Teilbaums, innerhalb dessen das Programm eine Ressource entsprechend der angefragten URI lädt.");

	fprintf(stderr, "-f file\n%s\n\n",
			"Protokolliert sämtliche Transaktionen in die Datei file. Wird als Dateiname ''–' verwendet oder die Option nicht angegeben, so ist für die Ausgabe die Standardausgabe (stdout) zu verwenden.");

	fprintf(stderr, "-p port\n%s\n\n",
			"Definiert den lokalen Port, auf dem der Server ankommende HTTP Verbindungen entgegen nimmt.");

	fprintf(stderr, "-h\n%s\n\n",
			"Gibt eine Übersicht der gültigen Programmparameter sowie Gruppe, Namen und Kurs der Programmautoren. Diese Meldung soll zusammen mit einer Fehlermeldung auch ausgegeben werden, wenn keine oder falsche Programmparameter an- gegeben wurden.");

} /* end of print_usage */

/*
 * Name: get_options
 * Zweck: Einlesen und Setzen der Programmoptionen
 * In-Parameter: int argc
 char *argv[] (Startparameter)
 * Out-Parameter: prog_options_t *opt (Struktur mit Programmoptionen)
 * Globale Variablen: -
 * Rückgabewert: int Statuscode (0, bei Fehler negativ)
 */
static int get_options(int argc, char *argv[], prog_options_t *opt) {
	int c;
	int err;
	int success = 1;
	char *p;
	struct addrinfo hints;

	p = strrchr(argv[0], '/');
	if (p) {
		p++;
	} else {
		p = argv[0];
	} /* end if */

	opt->progname = (char *) malloc(strlen(p) + 1);
	if (opt->progname != NULL) {
		strcpy(opt->progname, p);
	} else {
		err_print("cannot allocate memory");
		return EXIT_FAILURE;
	} /* end if */

	opt->log_filename = NULL;
	opt->root_dir = NULL;
	opt->server_addr = NULL;
	opt->log_fd = NULL;
	opt->verbose = 0;
	opt->timeout = 120;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_UNSPEC; /* Allows IPv4 or IPv6 */
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;

	while (success) {
		int option_index = 0;
		static struct option long_options[] = { { "file", required_argument, 0,
				0 }, { "port", required_argument, 0, 0 }, { "dir",
				required_argument, 0, 0 }, { "verbose", no_argument, 0, 0 }, {
				"debug", no_argument, 0, 0 }, { NULL, 0, 0, 0 } };

		c = getopt_long(argc, argv, "f:p:d:vh", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'f':
			// 'optarg' contains file name
			opt->log_filename = (char *) malloc(strlen(optarg) + 1);
			if (opt->log_filename != NULL) {
				strcpy(opt->log_filename, optarg);
			} else {
				err_print("cannot allocate memory");
				return EXIT_FAILURE;
			} /* end if */
			break;
		case 'p':
			// 'optarg' contains port number
			if ((err = getaddrinfo(NULL, optarg, &hints, &opt->server_addr))
					!= 0) {
				fprintf(stderr, "Cannot resolve service '%s': %s\n", optarg,
						gai_strerror(err));
				return EXIT_FAILURE;
			} /* end if */
			opt->server_port = get_port_from_name((char *) optarg);
			break;
		case 'd':
			// 'optarg contains root directory */
			opt->root_dir = (char *) malloc(strlen(optarg) + 1);
			if (opt->root_dir != NULL) {
				strcpy(opt->root_dir, optarg);
			} else {
				err_print("cannot allocate memory");
				return EXIT_FAILURE;
			} /* end if */
			break;
		case 'v':
			opt->verbose = 1;
			break;
		default:
			success = 0;
			break;
		} /* end switch */
	} /* end while */

	// check presence of required program parameters
	success = success && opt->server_addr && opt->root_dir;

	// additional parameters are silently ignored, otherwise check for
	// ((optind < argc) && success)

	return success;
} /* end of get_options */

/*
 * Name: open_logfile
 * Zweck: Öffnen der Logdatei
 * In-Parameter: prog_options_t *opt (Struktur mit Programmoptionen)
 * Out-Parameter: -
 * Globale Variablen: -
 * Rückgabewert: -
 */
static void open_logfile(prog_options_t *opt) {
	set_verbosity_level(opt->verbose);

	// open logfile or redirect to stdout
	if (opt->log_filename != NULL && strcmp(opt->log_filename, "-") != 0) {
		opt->log_fd = fopen(opt->log_filename, "w");
		if (opt->log_fd == NULL) {
			perror("ERROR: Cannot open logfile");
			exit(EXIT_FAILURE);
		} /* end if */
	} else {
		printf("Note: logging is redirected to stdout.\n");
		opt->log_fd = stdout;
	} /* end if */

	set_log_file(opt->log_fd);
} /* end of open_logfile */

/*
 * Name: check_root_dir
 * Zweck: Prüfen, ob Wurzelverzeichnis vorhanden und lesbar
 * In-Parameter: prog_options_t *opt (Struktur mit Programmoptionen)
 * Out-Parameter: -
 * Globale Variablen: -
 * Rückgabewert: -
 */
static void check_root_dir(prog_options_t *opt) {
	struct stat stat_buf;

	// check whether root directory is accessible
	if (stat(opt->root_dir, &stat_buf) < 0) {
		/* root dir cannot be found */
		perror("ERROR: Cannot access root dir");
		exit(EXIT_FAILURE);
	} else if (!IS_ROOT_DIR(stat_buf.st_mode)) {
		err_print("Root dir is not readable or not a directory");
		exit(EXIT_FAILURE);
	} /* end if */
} /* end of check_root_dir */

/*
 * Name: install_signal_handlers
 * Zweck: Installiert die Signalhandler für die Signale, auf die der Server reagiert
 * In-Parameter: -
 * Out-Parameter: -
 * Globale Variablen: -
 * Rückgabewert: -
 */
static void install_signal_handlers(void) {
	struct sigaction sa;

	// init signal handler(s)
	// TODO: add other signals
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = sig_handler;
	if (sigaction(SIGINT, &sa, NULL) < 0) {
		perror("sigaction(SIGINT)");
		exit(EXIT_FAILURE);
	} /* end if */

	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	if (sigaction(SIGPIPE, &sa, 0) == -1) {
		perror("sigaction(SIGPIPE)");
		exit(1);
	}

} /* end of install_signal_handlers */

/*
 * Name: main
 * Zweck: Einstiegspunkt des Programms
 * In-Parameter: int argc
 char *argv[] (Startparameter, Übergabe aus Konsole)
 * Out-Parameter: -
 * Globale Variablen: -
 * Rückgabe\Exit-wert: int Statuscode (EXIT_SUCCESS=ok, EXIT_FAILURE=Fehler)
 */
int main(int argc, char *argv[]) {
	int retcode = EXIT_SUCCESS;
	int ret = 0;
	prog_options_t my_opt;
	int sd;

	// read program options
	if (get_options(argc, argv, &my_opt) == 0) {
		print_usage(my_opt.progname);
		exit(EXIT_FAILURE);
	} /* end if */

	// set the time zone (TZ) to GMT in order to
	// ignore any other local time zone that would
	// interfere with correct time string parsing
	setenv("TZ", "GMT", 1);
	tzset();

	// do some checks and initialisations...
	open_logfile(&my_opt);
	check_root_dir(&my_opt);
	install_signal_handlers();
	init_logging_semaphore();

	// start the server and handle clients...
	printf("[%d] Starting server '%s on port %d'...\n", getpid(),
			my_opt.progname, my_opt.server_port);
	server_running = true;
	sd = server_start(my_opt.server_port);
	if (sd > 0) {
		ret = 1;
		while (server_running && ret == 1) {
			ret = server_accept_clients(sd, my_opt.root_dir);

			int status = 0;
			int pid = 1;
			while (pid > 0) {
				pid = waitpid(-1, &status, WNOHANG);
				if (pid > 0) {
					print_debug("Child[%d] exited with %d\n", pid,
							WEXITSTATUS(status));
				}
			}

		}
	}
	if (ret == -1) {
		retcode = EXIT_FAILURE;
	}

	print_debug("Good Bye[%d]...\n", retcode);

	//Free options
	free(my_opt.log_filename);
	my_opt.log_filename = NULL;
	free(my_opt.root_dir);
	my_opt.root_dir = NULL;
	free(my_opt.progname);
	my_opt.progname = NULL;
	freeaddrinfo(my_opt.server_addr);
	my_opt.server_addr = NULL;

	if (my_opt.log_fd != NULL) {
		fclose(my_opt.log_fd);
		my_opt.log_fd = NULL;
	}
	free_logging_semaphore();

	exit(retcode);
} /* end of main */

