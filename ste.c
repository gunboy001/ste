#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>

/* defines */
#define CTRL(k) ((k) & 0x1f) // Control mask modifier
#define TABSIZE 4 // Tab size as used in render
#define STAT_SIZE 128
#define _XOPEN_SOURCE_EXTENDED 1
#define EROW {0, NULL, 0, 0, NULL}

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
} t;

/* Row structure, defines actual and
 * render chars, actual and render size
 * and difference between render and
 * real size of the row 
 * Utf-8 continuation chars */
typedef struct row {
	int size;
	char *chars;
	int r_size;
	int utf;
	char *render;
} row;

/* Rows structure (or file buffer)
 * defines rows and teh number of rows */
struct {
	row *rw;
	int rownum;
} rows;

/* Prototypes */
/* draw operations */
static void drawBar (char *s);
static void drawScreen ();
static void drawLines (void);
static void updateRender (row *rw);
static void curUpdateRender (void);
static void cursorMove(int a);
static int decimalSize (int n);
static inline void lnMove (int y, int x);

/* Row operations */
static inline void rowInit (void);
static void rowAddChar (row *rw, char c, int pos);
static int rowDeleteChar (row *rw, int select, int pos);
static void rowCpy (row *to, row *from);
static void rowAddRow (int pos);
static void rowFree (row *rw);
static void rowAppendString (row *rw, char *s, int len);
static void rowDeleteRow (int pos);

/* Terminal operations */
static void termInit (void);
static void termExit (void);
static void termDie (char *s);

/* file operations */
static void fileOpen (char *filename);
void fileSave (char *filename);

/* buffer operations */
static void rowAddLast (char *s, int len);

/* garbage */
static void handleDel (int select);
/* testing */
static void updateInfo (void);
static int whatsThat (void);
static inline int isUtf (int c);
static inline int isCont (int c);
static inline int isStart (int c);

/* --------------------------------- main ------------------------------------ */
int main (int argc, char *argv[])
{
	/* Initialize the first row */
	rowInit();

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

	/* remember the initial row number */
	int irow = decimalSize(rows.rownum);

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
				rowAddRow(t.cur.y);
				t.cur.y++;
				t.cur.x = 0;
				break;
			case (KEY_END):
				t.cur.y = rows.rownum - 1;
				break;
			case (KEY_HOME):
				t.cur.y = 0;
				break;
			default:
				if (c == KEY_STAB) c = '\t';
				rowAddChar(&rows.rw[t.cur.y], c, t.cur.x);
				t.cur.x++;
				break;
		}
		if (decimalSize(rows.rownum) - irow) updateInfo();
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
		if (ln >= rows.rownum) break;
		
		/* Draw the line number */
		attron(COLOR_PAIR(1));
		mvprintw(i, 0, "%d", ln + 1);
		attroff(COLOR_PAIR(1));
		lnMove(i, 0);

		/* Draw the line matcing render memory */
		if (&rows.rw[ln] == NULL) termDie("drawlines NULL");
		if (rows.rw[ln].r_size > t.cur.off_x) {
			start = t.cur.off_x;
			while (isCont(rows.rw[ln].render[start])) start++; 
			addnstr(&rows.rw[ln].render[start], (t.dim.x + 1) + (rows.rw[ln].utf >> 2));
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



void updateRender (row *rw)
{
	/* count the special characters
	 * spaces, utf-8 continuation chars */
	static int tabs, i, off, cc, ut;
	for (i = 0, tabs = 0, cc = 0, ut = 0; i < rw->size; i++) {
		if (rw->chars[i] == '\t') tabs++;
		else if (isCont(rw->chars[i])) cc++;
		else if (isUtf(rw->chars[i])) ut++;
	}
	rw->render = NULL;
	free(rw->render);

	/* Render is long as size with the added tab spaces - 1
	 * (we already count for the \t as a char) */
	rw->render = malloc(rw->size + tabs * (TABSIZE - 1) + 1);
	if (rw->render == NULL) termDie ("malloc in updateRender");

	/* put all the characters (substituing all special chars)
	 * into the render buffer */
	for (i = 0, off = 0; i < rw->size; i++) {
		if (rw->chars[i] == '\t') {
			for (int j = 0; j < TABSIZE; j++){
				//if (!j) rw->render[off++] = '|'; 
				//else rw->render[off++] = ' ';
				rw->render[off++] = ' ';
			}		
		} else {
			rw->render[off++] = rw->chars[i];
		}
	}
	rw->render[off] = '\0';
	rw->r_size = off - cc;
	rw->utf = cc + ut;
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
		rowAddLast(line, linelen);
	}
	/* free the line buffer */
	free(line);
	/* close the file */
	fclose(fp);
}

/*------------------------------------- file operations ----------------------------------*/

/* Add a row to the file buffer */
void rowAddLast (char *s, int len)
{
	/* Extend the block of memory containing the lines */
	row *newr = realloc(rows.rw, (rows.rownum + 1) * sizeof(row));
	if (newr == NULL) termDie("realloc in rowAddLast");
	else rows.rw = newr;

	/* Allocate memory for the line and copy it
	 * at the current row number */
	rows.rw[rows.rownum].chars = malloc(len  + 1);
	if (rows.rw[rows.rownum].chars == NULL) termDie("malloc in rowAddLast chars");
	memcpy(rows.rw[rows.rownum].chars, s, len);
	rows.rw[rows.rownum].chars[len] = '\0';
	rows.rw[rows.rownum].size = len;
	updateRender(&rows.rw[rows.rownum]);
	rows.rownum++;
}

void rowInit (void)
{
	rows.rw = NULL;
	rows.rownum = 0;
}

void rowAddChar (row *rw, char c, int pos)
{
	/* Check if char is valid */
	if (!c || (iscntrl(c) && c != '\t')) return;
	
	/* extend the string */
	rw->size++;
	char *t = realloc(rw->chars, rw->size + 1);
	if (t == NULL) termDie("realloc in rowAddchar");
	rw->chars = t;

	/* make space for the new char */
	memcpy(&rw->chars[pos + 1], &rw->chars[pos], (rw->size + 1) - (pos + 1));

	/* add the new char */
	rw->chars[pos] = c;

	updateRender(rw);
}

int rowDeleteChar (row *rw, int select, int pos) // WIP
{
	/* Check if the character is valid */
	if (rw->chars[pos - 1] == '\0' && pos) return 0;
	if (!pos && !select) return 0;
	if (rw->chars[pos] == '\0' && select) return 0;
	//change pos based on the command
	if (!select) pos--;

	memcpy(&rw->chars[pos], &rw->chars[pos + 1], rw->size - pos);

	char *s = realloc(rw->chars, rw->size);
	if (s != NULL) rw->chars = s;
	rw->size--;

	updateRender(rw);
	return 1;
}

void rowAddRow (int pos) // WIP; TO DOCUMENT
{
	/* 	MOVE THE ROWS AWAY */
	/* add another line to the bottom containing the previous
	 * (last) line, effectively making new space */
	rowAddLast(rows.rw[rows.rownum - 1].chars, rows.rw[rows.rownum - 1].size);

	/* copy all other lines until the specified position to the next one */
	for (int last = rows.rownum - 1; last > pos; last--)
		rowCpy(&rows.rw[last], &rows.rw[last - 1]);

	/* SPLIT THE ROW AT POS AND STORE IT */
	/* Get the row length at position after the cursor */
	int len = rows.rw[pos].size - t.cur.x;
	/* create a dummy row as the new row souce */
	row nex = EROW;
	/* allocate a buffer */
	char *s = malloc(len + 1);
	if (s == NULL) termDie("malloc in rowAddRow s");
	/* copy the contents of the pos row after the cursor into the buffer */
	memcpy(s, &rows.rw[pos].chars[t.cur.x], len);
	s[len] = '\0';
	/* update the dummy row */
	nex.chars = s;
	nex.size = strlen(s);
	
	/* MAKE THE SPLIT INTO TWO LINES */
	/* shrink the line at pos */
	char *p = realloc(rows.rw[pos].chars, t.cur.x + 1);
	if (p == NULL) termDie("realloc in rowAddRow");
	rows.rw[pos].chars = p;
	/* and terminate it with null like a good boi */
	rows.rw[pos].chars[t.cur.x] = '\0';
	/* update info and render */
	rows.rw[pos].size = t.cur.x;
	updateRender(&rows.rw[pos]);

	/* copy the dummy to the new line and free */
	rowCpy(&rows.rw[pos + 1], &nex);
	rowFree(&nex);

}

void rowFree (row *rw) // WIP
{
	/* Free both render and memory strings */
	free(rw->render);
	free(rw->chars);
	/* clear sizes */
	rw->size = 0;
	rw->r_size = 0;
	rw->utf = 0;
}

void rowCpy (row *to, row *from) // WIP
{
	/* Free the destination row (without destroying it) */
	rowFree(to);
	/* Allocate space for the new string */
	to->chars = (char*) malloc(strlen(from->chars) + 1);
	if (to->chars == NULL) termDie("malloc in rowCpy");
	/* And update the size */
	to->size = from->size;
	/* Then copy the chars from the source row to the destination row */
	memcpy(to->chars, from->chars, to->size);
	to->chars[to->size] = '\0';
	/* Then update the render */
	updateRender(to);
}

void rowAppendString (row *rw, char *s, int len)
{
	/* reallocate the row to accomodate for the added string */
	char *temp = realloc(rw->chars, rw->size + len + 1);
	if (temp == NULL) termDie("realloc in rowAppendString");
	rw->chars = temp;

	memcpy(&rw->chars[rw->size], s, len);
	rw->size += len;
	rw->chars[rw->size] = '\0';
	
	updateRender(rw);
}

void rowDeleteRow (int pos)
{
	if (rows.rownum == 1) return;
	if (pos >= rows.rownum) return;
	if (pos < 0) return;

	for (; pos < rows.rownum - 1; pos++) {
		rowCpy(&rows.rw[pos], &rows.rw[pos + 1]); // rowcpy already frees the row
	}
	rows.rownum--;
	rowFree(&rows.rw[rows.rownum]);
	row *temp = realloc(rows.rw, sizeof(row) * rows.rownum);
	if (temp == NULL) termDie("realloc in rowDeleteRow");
	rows.rw = temp;
}

/* ----------------------------- row operations --------------------------- */

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

	if (t.cur.y >= t.cur.off_y && t.cur.y < t.cur.off_y + t.dim.y) {
		t.cur.r_y = t.cur.y - t.cur.off_y;

	} else if (t.cur.y >= t.cur.off_y + t.dim.y) {
		if (t.cur.y == t.cur.off_y + t.dim.y) t.cur.off_y++;
		else t.cur.off_y += t.cur.y - (t.cur.off_y + t.dim.y);
		t.cur.r_y = t.dim.y - 1;
	
	} else if (t.cur.y < t.cur.off_y) {
		t.cur.off_y -= t.cur.off_y - t.cur.y;
		t.cur.r_y = 0;
	}

	// x
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
		t.cur.off_x += t.cur.r_x - t.cur.off_x - t.dim.x;
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
			rowDeleteRow(t.cur.y);
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
			rowDeleteRow(t.cur.y + 1);
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
/*--------------------------------- testing ------------------------------------*/
