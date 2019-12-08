#ifndef _HELPER_DIE_H_
#define _HELPER_DIE_H_

#include <stdlib.h>
#include <stdio.h>

#ifdef FRONTEND_NCURSES
	#include <ncurses.h>
#endif

typedef enum {
	AOK,
	GENERIC_ERR,
	BAD_FILE,
	MALLOC_ERR,
	REALLOC_ERR,
	BAD_PNTR
} DeathStatus;

void die (const char *message, DeathStatus sn)
{
	#ifdef FRONTEND_NCURSES
		erase();
		refresh();
		endwin();
	#endif

	if (sn)	perror(message);
	exit(sn);
}

#endif