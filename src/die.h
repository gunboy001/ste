#ifndef _HELPER_DIE_H_
#define _HELPER_DIE_H_

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#ifdef FRONTEND_NCURSES
	#include <ncurses.h>
#endif

void die (const char *message, int err)
{
	#ifdef FRONTEND_NCURSES
		erase();
		refresh();
		endwin();
	#endif

	if (message == NULL)
		exit(0);

	if (err > 131)
		err = 1;

	if (err) {
		printf("\n%s:%s", message, strerror(err));
		exit(err);
	} else {
		perror(message);
		exit(errno);
	}
	exit(-1);
}

#endif