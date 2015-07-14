/*===================================================================
 * DHBW Ravensburg - Campus Friedrichshafen
 *
 * Vorlesung Verteilte Systeme
 *
 * Authoren:  Ralf Reutemann (person in charge)
 * 			  Michael Möbius
 * 			  Maximilian Schmitz
 *
 *===================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>


#define SEM_NAME                  "/tinysem"
#define err_print(s)              fprintf(stderr, "ERROR: %s, %s:%d\n", (s), __FILE__, __LINE__)
#define MAXS 1024

static sem_t *log_sem = NULL;
static unsigned short verbosity_level = 0;
static FILE *log_fd = NULL;


/*
 * Name: init_loging_semaphore
 * Zweck: Initialisieren der Logging Semaphore
 * In-Parameter: -
 * Out-Parameter: -
 * Globale Variablen: sem_t* log_sem
 * Rückgabewert: -
 */
void
init_logging_semaphore(void)
{
    if ((log_sem = sem_open(SEM_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED) {
        err_print("cannot create named semaphore");
        exit(EXIT_FAILURE); //WARNING / TODO Does not free used memory!
    } /* end if */
} /* end of init_logging_semaphore */


/*
 * Name: free_loging_semaphore
 * Zweck: Freigeben der Logging Semaphore
 * In-Parameter: -
 * Out-Parameter: -
 * Globale Variablen: sem_t* log_sem
 * Rückgabewert: -
 */
void
free_logging_semaphore(void)
{
	if (log_sem != NULL)
	{
		sem_close(log_sem);
	}
} /* end of free_logging_semaphore */


/*
 * Name: set_verbosity_level
 * Zweck: Setzen der Detailstufe der Ausgabe
 * In-Parameter: unsigned short level (Detailstufe der Ausgabe)
 * Out-Parameter: -
 * Globale Variablen: unsigned short verbosity_level
 * Rückgabewert: -
 */
void
set_verbosity_level(unsigned short level)
{
    verbosity_level = level;
} /* end of set_verbosity_level */


/*
 * Name: set_log_file
 * Zweck: Setzen der Log-Datei
 * In-Parameter: FILE *fd (Filedeskriptor der Logdatei)
 * Out-Parameter: -
 * Globale Variablen: FILE *log_fd
 * Rückgabewert: -
 */
void set_log_file(FILE *fd)
{
	log_fd = fd;
}



/*
 * Name: print_log
 * Zweck: Ausgeben eines Log-Textes
 * In-Parameter: const char *format, ... (Parameter wie printf)
 * Out-Parameter: -
 * Globale Variablen: -
 * Rückgabewert: int (Rückgabewert aus fprintf)
 */
int
print_log(const char *format, ...)
{
    int status;
    va_list args;
    int pos;
    char buf[MAXS];

    if (log_sem != NULL && sem_wait(log_sem) < 0) {
        err_print("semaphore wait");
    } /* end if */


    if (log_fd == NULL)
    {
		printf("[%d] ", getpid());
		va_start(args, format);
		status = vprintf(format, args);
		va_end(args);
    }
    else
    {
        pos = snprintf(buf, sizeof(buf), "[%d] ", getpid());

        va_start(args, format);
        vsnprintf(buf+pos, sizeof(buf)-pos, format, args);
        va_end(args);
        status = fprintf(log_fd, "%s", buf);
    }


    if (log_sem != NULL && sem_post(log_sem) < 0) {
        err_print("semaphore post");
    } /* end if */


    return status;
} /* end of print_log */


/*
 * Name: print_debug
 * Zweck: Ausgeben eines Debug-Textes
 * In-Parameter: const char *format, ... (Parameter wie printf)
 * Out-Parameter: -
 * Globale Variablen: -
 * Rückgabewert: int (Rückgabewert aus fprintf)
 */
int
print_debug(const char *format, ...)
{
    int status;
    int pos;
    va_list args;
    char buf[MAXS];

    if (verbosity_level < 1) {//Edit: verbose is only set to 1!
        return 0;
    } /* end if */

    if (log_sem != NULL && sem_wait(log_sem) < 0) {
        err_print("semaphore wait");
    } /* end if */

    pos = snprintf(buf, sizeof(buf), "[%d] ", getpid());

    va_start(args, format);
    vsnprintf(buf+pos, sizeof(buf)-pos, format, args);
    va_end(args);
    status = write(STDOUT_FILENO, buf, strlen(buf)); /* write is async-signal-safe */

    if (log_sem != NULL && sem_post(log_sem) < 0) {
        err_print("semaphore post");
    } /* end if */

    return status;
} /* end of print_debug */


/* TODO */

/*
 * Name: print_http_header
 * Zweck: Ausgeben des Status für einen Http Request
 * In-Parameter: const char *what
		 const char *response_str
 * Out-Parameter: -
 * Globale Variablen: -
 * Rückgabewert: -
 */
void
print_http_header(const char *what, const char *response_str)
{
    size_t len;

    if (verbosity_level == 0) {
        return;
    } /* end if */

    len = strlen(response_str);

    if (log_sem != NULL && sem_wait(log_sem) < 0) {
        err_print("semaphore wait");
    } /* end if */

    printf("[%d] %s HEADER (%zd Bytes):\n%s", getpid(), what, len, response_str);


    if (log_sem != NULL && sem_post(log_sem) < 0) {
        err_print("semaphore post");
    } /* end if */

} /* end of print_http_header */

