#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>

#include "fbuffer.h"
#include "config.h"

/* defines */
#define CTRL(k) ((k) & 0x1f) // Control mask modifier
#define STAT_SIZE 128
#define _XOPEN_SOURCE_EXTENDED
#define _GNU_SOURCE
#define SBUF_SIZE 2048

// Search buffer
typedef struct sbuf {
	char c[SBUF_SIZE];
	int num;
} sbuf;

/* main data structure containing:
 *	-cursor position
 *	+real (matching memory)
 *	+offset (memory and screen offset)
 *	+render (drawn on the screen)
 *	-window size
 *	-statusbar message */
struct term {
	struct {
		int x;
		int y;
		int off_x;
		int off_y;
		int r_x;
		int r_y;
	} cur;

	struct {
		int x;
		int y;
	} dim;

	char statusbar[STAT_SIZE];
	int pad;
	char mode_b;
	sbuf search_buffer;
} t;

fbuffer rows;

const char *msg[] = {
					"Find: ",
					"Nigger"
					};

/* Prototypes */
/* draw operations */
static void drawBar (char *s);
static void drawScreen ();
static void drawLines (void);
static void curUpdateRender (void);
static void cursorMove(int a);
static int decimalSize (int n);
static inline void lnMove (int y, int x);

/* Terminal operations */
static void termInit (void);
static void termExit (void);
static void termDie (char *s);

/* file operations */
static void fileOpen (char *filename);
void fileSave (char *filename);

/* buffer operations */
static int editorFind (const char* needle, int* y, int* x);

/* garbage */
static void handleDel (int select);
/* testing */
static void updateInfo (void);
static int whatsThat (void);
static void insert (sbuf *buf, int c);
static inline void flush (sbuf *buf);
static void toString (sbuf *buf, char *s);

/* --------------------------------- main ------------------------------------ */
int main (int argc, char *argv[])
{
	/* Initialize the first row */
	bufInit(&rows);

	/* Try to open the file */
	if (argc < 2) {
		perror("File not found");
		exit(1);
	} else fileOpen(argv[1]);

	/* Initialize the terminal in raw mode,
	 * start curses and initialize the term struct */
	termInit();

	/* Set the statusbar left (static) message */
	snprintf(t.statusbar, STAT_SIZE, "%s %d lines %dx%d", argv[1], rows.rownum, t.dim.y, t.dim.x);

	/* Main event loop */
	int c;
	while (1) {
		updateInfo();
		/* Redraw the screen */
		drawScreen();

		/* Wait for an event (keypress) */
		switch (c = getch()) {
			case (CTRL('q')):
				termExit();
				break;

			case (KEY_LEFT):
			case (KEY_RIGHT):
			case (KEY_UP):
			case (KEY_DOWN):
				cursorMove(c);
				break;

			case (KEY_BACKSPACE):
				handleDel(0);
				break;

			case (KEY_DC):
				handleDel(1);
				break;

			case (KEY_ENTER):
			case (10):
			case ('\r'):
				if ((t.mode_b & 0x1) == 0x0) {
					rowAddRow(&rows, t.cur.y, t.cur.x);
					t.cur.y++;
					t.cur.x = 0;
				} else {
					char *query = malloc(t.search_buffer.num + 1);
					if (query == NULL) termDie("maloc in query allocation");
					toString(&t.search_buffer, query);
					editorFind(query, &t.cur.y, &t.cur.x);
					// Toggle mode
					t.mode_b ^= 0x1;
					flush (&t.search_buffer);
				}
				break;

			case (KEY_END):
				t.cur.y = rows.rownum - 1;
				break;

			case (KEY_HOME):
				t.cur.y = 0;
				break;

			case (CTRL('f')):
				/*if (!editorFind("for", &t.cur.y, &t.cur.x)) {
					t.cur.y = 0;
					editorFind("for", &t.cur.y, &t.cur.x);
				}*/

				// Toggle mode
				t.mode_b ^= 0x1;
				flush (&t.search_buffer);

				break;

			default:
				if ((t.mode_b & 0x1) == 0x0) {
					if (c == KEY_STAB) c = '\t';
					rowAddChar(&rows.rw[t.cur.y], c, t.cur.x);
					t.cur.x++;
				} else {
					insert(&t.search_buffer, c);
				}
				break;
		}
	}

	/* If by chance i find myself here be sure
	 * end curses mode and clenaup */
	termExit();
	return 0;
}

/* ----------------------------- end of main ---------------------------------- */

void termInit (void)
{
	/* Init locales */
	setlocale(LC_ALL, "");
	
	/* Init the screen and refresh */
	initscr();
	refresh();

	/* Enable raw mode, this makes the termianl ignore
	 * interrupt signals like CTRL+C and CTRL+Z
	 * allowing us to make our own bindings */
	raw();

	/* Allow use of function keys */
	keypad(stdscr, TRUE);

	/* Turn off echoing */
	noecho();

	/* Set the tab size */
	set_tabsize(TABSIZE);

	/* Start color mode */
	start_color();
	init_pair(2, COLOR_BLACK, COLOR_CYAN);
	init_pair(1, COLOR_RED, COLOR_BLACK);

	/* Set default color */
	//bkgd(COLOR_PAIR(1));

	/* Populate the main data structure */
	updateInfo();

	/* Initialize the data staructure */
	t.cur.x = t.cur.off_x = 0;
	t.cur.y = t.cur.off_y = 0;
}

/* Calculate the correct spacing for the line numbers
 * based on the size of the file */
int decimalSize (int n)
{
	static int l;
	for (l = 0; n > 0; l++) n /= 10;
	return l + 1;
}

void termExit (void)
{
	erase();
	refresh();
	endwin();
	exit(0);
}

void termDie (char *s)
{
	erase();
	refresh();
	endwin();
	perror(s);
	exit(1);
}

/* ----------------------------- term operations -------------------------------- */

void drawScreen ()
{
	/* Clear the screen */
	erase();
	/* Update Scroll */
	curUpdateRender();
	/* draw the lines */
	drawLines();
	/* draw the bar */
	drawBar(t.statusbar);
	/* move back to the cursor position */
	lnMove(t.cur.r_y, t.cur.r_x);
	/* refresh the screen */
	refresh();
}

/* Draw all the appropriate lines (following cursor) to the screen */
void drawLines (void)
{
	register short ln, i;
	register int start;

	for (i = 0, ln = 0; i < t.dim.y + t.cur.off_y; i++) {
		ln = i + t.cur.off_y;

		/* vi style tildes */
		if (ln >= rows.rownum) {
			mvaddch(i, 0, '~');
		} else {
			/* Draw the line number */
			attron(COLOR_PAIR(1));
			mvprintw(i, 0, "%d", ln + 1);
			mvaddch(i, t.pad - 1, ACS_VLINE);
			attroff(COLOR_PAIR(1));
			lnMove(i, 0);

			/* Draw the line matcing render memory */
			if (&rows.rw[ln] == NULL) termDie("drawlines NULL");
			if (rows.rw[ln].r_size > t.cur.off_x) {
				start = t.cur.off_x;
				while (isCont(rows.rw[ln].render[start])) start++; 
				addnstr(&rows.rw[ln].render[start], (t.dim.x + 1) + (rows.rw[ln].utf >> 2));
			}
		}
		lnMove(i + 1, 0);
	}
	lnMove(t.cur.y, t.cur.x);
}

/* Move avoiding the space allocated for line numbers */
inline void lnMove (int y, int x)
{
	move(y, x + t.pad);
}

/* Draw the status bar at the bottom of the screen */
void drawBar (char *s)
{
	/* Set maximum contrast for bar */
	attron(COLOR_PAIR(2));
	/* get the length of the statusbar text */
	int len = strlen(s);
	/* Print the message */
	mvprintw(t.dim.y, 0, s);
	/* Fill everything else with spaces */
	static int i;
	for (i = len; i <= t.dim.x + t.pad; i++)
		mvaddch(t.dim.y, i, ' ');

	static char m[40];
	sprintf(m, "x: %d y: %d Zoom: %c", t.cur.x, t.cur.y, whatsThat());
	mvaddstr(t.dim.y, t.dim.x + t.pad - strlen(m), m);

	/* Return to normal contrast mode */
	attroff(COLOR_PAIR(2));
}

/* -------------------------------- draw operations -------------------------------- */

/* Open a file and put it into a buffer line by line */
void fileOpen (char *filename)
{
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) termDie("Cannot open file");

	/* Init the linebuffer */
	char *line = NULL;
	/* Set linecap to 0 so getline will atumatically allocate
	 * memory for the line buffer*/
	size_t linecap = 0;
	ssize_t linelen;

	/* getline returns -1 if no new line is present */
	while ((linelen = getline(&line, &linecap, fp)) != -1) {
		while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
			linelen--;
		rowAddLast(&rows, line, linelen);
	}
	/* free the line buffer */
	free(line);
	/* close the file */
	fclose(fp);
}

/*------------------------------------- file operations ----------------------------------*/

/* take care of the cursor movement */
void cursorMove (int a)
{
	switch (a) {
		case (KEY_LEFT):
			if (t.cur.x <= 0) {
				if (t.cur.y) {
					t.cur.y--;
					t.cur.x = rows.rw[t.cur.y].size;
				}
			} else if (isCont(rows.rw[t.cur.y].chars[t.cur.x - 1])) {
				do {
					t.cur.x--;
				} while(!isStart(rows.rw[t.cur.y].chars[t.cur.x]));
			} else
				t.cur.x--;
			break;

		case (KEY_RIGHT):
				if (t.cur.x >= rows.rw[t.cur.y].size) {
					if (t.cur.y < rows.rownum - 1) {
						t.cur.y++;
						t.cur.x = 0;
					}
				} else if (isStart(rows.rw[t.cur.y].chars[t.cur.x])) {
					do {
						t.cur.x++;
					} while(isCont(rows.rw[t.cur.y].chars[t.cur.x]));
				} else
					t.cur.x++;
			break;

		case (KEY_UP):
			if (t.cur.y) {
				t.cur.y--;
				if (t.cur.x > rows.rw[t.cur.y].size)
					t.cur.x = rows.rw[t.cur.y].size;
			}
			break;

		case (KEY_DOWN):
			if (t.cur.y < rows.rownum - 1) {
				t.cur.y++;
				if (t.cur.x > rows.rw[t.cur.y].size)
					t.cur.x = rows.rw[t.cur.y].size;
			}
			break;
	}
}

void curUpdateRender ()
{
	/*
	      Whole file (memory)
	 _____________________________
	|(0, 0)                       |
	|                             |
	|            off_y            |
	|      +--------------+       |
	|      |              |       |
	|      |              |       |
	|      |              | off_x |
	| off_x|              |   +   |
	|      |              | dim_x |
	|      |              |       |
	|      |              |       |
	|      |              |       |
	|      +--------------+       |
	|        off_y + dim.y        |
	|                             |
	|                             |
	|                             |
	|_____________________________| rows.rownum

	The inner sqaure represents the render area
	and it is delimited by:
		left: x -> t.cur.off_x
		right: x -> t.cur.off_x + (t.dim.x - 1)
		upper: y -> t.cur.off_y
		lower: y -> t.cur.off_y + (t.dim.y - 1)
	The rows are drawn top to botom, starting
	from the left most char of the render area
	until row end or when the characters hit
	the right bound.
	*/

	/* Adjust x and y if they are out of bounds */
	if (t.cur.y < 0 || t.cur.y >= rows.rownum) t.cur.y = 0;
	if (t.cur.x < 0) t.cur.x = 0;

	if (t.cur.y >= t.cur.off_y && t.cur.y < t.cur.off_y + t.dim.y) {
		t.cur.r_y = t.cur.y - t.cur.off_y;

	} else if (t.cur.y >= t.cur.off_y + t.dim.y) {
		if (t.cur.y == t.cur.off_y + t.dim.y) t.cur.off_y++;
		else t.cur.off_y += t.cur.y - (t.cur.off_y + t.dim.y) + 1;
		t.cur.r_y = t.dim.y - 1;

	} else if (t.cur.y < t.cur.off_y) {
		t.cur.off_y -= t.cur.off_y - t.cur.y;
		t.cur.r_y = 0;
	}

	static int i, c;
	for (c = i = 0, t.cur.r_x = 0; i < t.cur.x; i++) {
		c = rows.rw[t.cur.y].chars[i];

		/* continue (skip increment) if you encounter a continuation char */
		if (isCont(c)) continue;
		else if (isStart(c)) t.cur.r_x++;

		if (c == '\t') t.cur.r_x += (TABSIZE - 1);

		t.cur.r_x++;
	}

	if (t.cur.r_x >= t.cur.off_x && t.cur.r_x < t.cur.off_x + t.dim.x) {
		t.cur.r_x -= t.cur.off_x;

	} else if (t.cur.r_x < t.cur.off_x) {
		t.cur.off_x -= t.cur.off_x - t.cur.r_x;
		t.cur.r_x = 0;
	
	} else if (t.cur.r_x >= t.cur.off_x + t.dim.x) {
		t.cur.off_x += t.cur.r_x - (t.cur.off_x + t.dim.x);
		t.cur.r_x = t.dim.x;
	}
}

/*---------------------------------- scroll ------------------------------------*/

/* See whats under the cursor (memory) */
int whatsThat (void) {
	int c = rows.rw[t.cur.y].chars[t.cur.x];
	switch (c) {
		case ('\t'):
			return '^';
			break;
		case (' '):
			return '~';
			break;
		case ('\0'):
			return '.';
			break;
		default:
			return c;
			break;
	}
	return 0;
}


void handleDel (int select)
{	
	if (!select) {
		if (t.cur.x <= 0 && t.cur.y > 0) {
			t.cur.x = rows.rw[t.cur.y - 1].size;
			rowAppendString(&rows.rw[t.cur.y - 1], rows.rw[t.cur.y].chars, rows.rw[t.cur.y].size);
			rowDeleteRow(&rows, t.cur.y);
			t.cur.y--;
		} else {
			if (isUtf(rows.rw[t.cur.y].chars[t.cur.x - 1])) {
				do {
					if (rowDeleteChar(&rows.rw[t.cur.y], 0, t.cur.x))
						t.cur.x--;
				} while (!isStart(rows.rw[t.cur.y].chars[t.cur.x - 1]));
			}
			if (rowDeleteChar(&rows.rw[t.cur.y], 0, t.cur.x))
				t.cur.x--;
		}
	} else {
		if (t.cur.x >= rows.rw[t.cur.y].size) {
			rowAppendString(&rows.rw[t.cur.y], rows.rw[t.cur.y + 1].chars, rows.rw[t.cur.y + 1].size);
			rowDeleteRow(&rows, t.cur.y + 1);
		} else {
			if (isStart(rows.rw[t.cur.y].chars[t.cur.x])) {
				do {
					rowDeleteChar(&rows.rw[t.cur.y], 1, t.cur.x);
				} while (isCont(rows.rw[t.cur.y].chars[t.cur.x]));
			} else rowDeleteChar(&rows.rw[t.cur.y], 1, t.cur.x);
		}
	}
}

void updateInfo (void)
{
	getmaxyx(stdscr, t.dim.y, t.dim.x);
	t.dim.y -= 1;
	t.pad = decimalSize(rows.rownum - 1);
	t.dim.x -= t.pad + 1;
}

/* Check for utf-8 char type */
int isUtf (int c) {
	return (c >= 0x80 || c < 0 ? 1 : 0);
}

int isCont (int c) {
	return ((c &= 0xC0) == 0x80 ? 1 : 0);
}

int isStart (int c) {
	return (isUtf(c) && !isCont(c) ? 1 : 0);
}

int editorFind (const char* needle, int *y, int *x)
{
	/* Create a pointer to store the location of the match */
	char *res = NULL;
	static int i, c;

	/* Search trough all the buffer */
	for (i = *y + 1; i < rows.rownum; i++) {
			res = strstr(rows.rw[i].chars, needle);
			if (res != NULL) break;
	}
	if (res == NULL) return 0;

	/* If something wa found convert from pointer to yx coodinates */
	*y = c = i;
	for (i = 0; i <= rows.rw[c].size; i++)
		if (&rows.rw[c].chars[i] == res) break;
	*x = i;
	return 1;
}

void insert (sbuf *buf, int c)
{
	if (buf->num < SBUF_SIZE - 1)
		buf->c[buf->num++] = c;
}

void flush (sbuf *buf)
{
	buf->num = 0;
}

void toString (sbuf *buf, char *s)
{
	for (int i = 0; i < buf->num; i++)
		s[i] = buf->c[i];
	s[buf->num] = '\0';
}
/*--------------------------------- testing ------------------------------------*/