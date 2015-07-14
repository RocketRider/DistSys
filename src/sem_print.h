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

#ifndef _SEM_PRINT_H
#define _SEM_PRINT_H

#include <semaphore.h>


extern void init_logging_semaphore(void);
extern void free_logging_semaphore(void);
extern void set_verbosity_level(unsigned short level);
extern int print_log(const char *format, ...);
extern int print_debug(const char *format, ...);
extern void print_http_header(const char *what, const char *response_str);
extern void set_log_file(FILE *fd);

#endif

