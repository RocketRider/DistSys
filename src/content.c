/*===================================================================
 * DHBW Ravensburg - Campus Friedrichshafen
 *
 * Vorlesung Verteilte Systeme
 *
 * Authoren:  Ralf Reutemann (person in charge)
 * 			  Michael Möbius
 * 			  Maximilian Schmitz
 *
 * Modul: Ermitteln des MIME-TYPES von Dateien
 *
 *===================================================================*/
#include <string.h>
#include "content.h"
#include <stdio.h>


static http_content_type_entry_t http_content_type_list[] = {
    { ".html",    "text/html"          },
    { ".css",     "text/css"           },
    { ".gif",     "image/gif"          },
    { ".jpg",     "image/jpeg"         },
    { ".pdf",     "application/pdf"    },
    { ".tar",     "application/x-tar"  },
    { ".xml",     "application/xml"    },
    { NULL,       "text/plain"         }
};


/*
 * Name: get_http_content_type
 * Zweck: Ermitteln des Http Content Type anhand der Dateiendung
 * In-Parameter: char *filename (Dateiname)
 * Out-Parameter: -
 * Globale Variablen: -
 * Rückgabewert: http_content_type_t (Content Type der Datei)
 */
http_content_type_t get_http_content_type(const char *filename) {
	int i;

	//To find last dot in the filename to make sure filenames like test.gif.html work as well!
	char* pch = strchr(filename, '.');
	while (pch != NULL) {
		filename = pch;
		pch = strchr(filename + 1, '.');
	}

	i = 0;
	while (http_content_type_list[i].ext != NULL) {
		if (strcasecmp(filename, http_content_type_list[i].ext) == 0) { //To make sure .HTML does work as well
			break;
		} /* end if */
		i++;
	} /* end while */

	return (http_content_type_t) i;
} /* end of get_http_content_type */

/*
 * Name: get_http_content_type_str
 * Zweck: Umwandeln des http_content_type_t in eine Zeichenkette
 * In-Parameter: http_content_type_t type (Http Content Type)
 * Out-Parameter: -
 * Globale Variablen: -
 * Rückgabewert: char* (Http Content Type als c_string)
 */
char *
get_http_content_type_str(const http_content_type_t type) {
	return http_content_type_list[type].name;
} /* end of get_http_content_type_str */

