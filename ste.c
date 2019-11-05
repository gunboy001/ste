#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* defines */
#define CTRL(k) ((k) & 0x1f) // Control mask modifier
#define TABSIZE 4 // Tab size as used in render
#define STAT_SIZE 128

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
		//int d_x;
		//int xx;
		//int yy;
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
 * real size of the row */
typedef struct row {
	int size;
	char *chars;
	int r_size;
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
//static int curRealToRender (row *rw, int c_x);

/* Row operations */
static inline void rowInit (void);
static void rowAddChar (row *rw, char c, int pos);
static void rowDeleteChar (row *rw, int select, int pos);
static void rowCpy (row *to, row *from);
static void rowAddRow (int pos);
static void rowFree (row *rw);
static void rowAddString (row *rw, char *s, int len, int pos);
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
				//rowAddRow(t.cur.yy);
				
				//t.cur.off_x = 0;
				rowAddRow(t.cur.y);
				t.cur.y++;
				t.cur.x = 0;
				break;
			case (KEY_END):
				t.cur.y = rows.rownum - 1;
				//t.cur.off_y = 0;
				break;
			case (KEY_HOME):
				t.cur.y = 0;
				//t.cur.off_y = 0;
				break;
			default:
				if (c == KEY_STAB) c = '\t';
				rowAddChar(&rows.rw[t.cur.y], c, t.cur.x);
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
	static int ln, i;
	/* move to the beginning of the screen */

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
			addnstr(&rows.rw[ln].render[t.cur.off_x], t.dim.x + 1);
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
	/* count the special characters (only tabs for now) */
	static int tabs = 0, i, off;
	for (i = 0, tabs = 0; i < rw->size; i++) {
		if (rw->chars[i] == '\t') tabs++;
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
	rw->r_size = off;
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
	row *newr = reallocarray(rows.rw, rows.rownum + 1, sizeof(row));
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

void rowAddChar (row *rw, char c, int pos) // WIP
{
	// Error checking (allow tab)
	if (!c || (iscntrl(c) && c != '\t')) return;
	
	int i = 0;
	char *s = rw->chars;

	// reallocate mem and inc size
	rw->chars = malloc(rw->size + 2);
	if (rw->chars == NULL) termDie("malloc in rowAddchar");
	rw->size++;

	// copy bf cursor
	for (i = 0; i < pos; i++) {
		rw->chars[i] = s[i];
	}

	// add at cursor
	rw->chars[pos++] = c;

	//copy after cursor 
	for (i = pos; i < rw->size + 1; i++) {
			rw->chars[i] = s[i - 1];
	}
	
	free(s);

	updateRender(rw);
	t.cur.x++;
}

void rowDeleteChar (row *rw, int select, int pos) // WIP
{
	char *s = rw->chars;
	//Do not delete NULL char
	if (s[pos - 1] == '\0' && pos) return;
	if (!pos && !select) return;
	if (s[pos] == '\0' && select) return;

	rw->chars = malloc(rw->size);
	if (rw->chars == NULL) termDie("malloc in rowDeleteChar");
	rw->size--;

	// Backspace
	if (!select) {
		for (int i = 0; i < pos - 1; i++)
			rw->chars[i] = s[i];
	
		for (int i = pos; i < rw->size + 1; i++)
			rw->chars[i - 1] = s[i];

		t.cur.x--;
	// Delete			
	} else {
		if(pos) {
			for (int i = 0; i < pos; i++)
				rw->chars[i] = s[i];
		}
	
		for (int i = pos; i < rw->size + 1; i++)
			rw->chars[i] = s[i + 1];
	}
	
	free(s);
	updateRender(rw);
}

void rowAddRow (int pos) // WIP; TO DOCUMENT
{
	char *s = NULL;
	// Move away other lines
	//copy old last line to new space
	rowAddLast(rows.rw[rows.rownum - 1].chars, rows.rw[rows.rownum - 1].size);

	for (int last = rows.rownum - 1; last > pos; last--) {
		rowCpy(&rows.rw[last], &rows.rw[last - 1]);
	}

	//copy previous row
	int l = rows.rw[pos].size - t.cur.x;
	s = malloc(l + 1);
	if (s == NULL) termDie("malloc in rowAddRow s");
	memcpy(s, &rows.rw[pos].chars[t.cur.x], l);
	s[l] = '\0';
	// Delete prev row until cursor
	char *p = malloc(t.cur.x + 1);
	if (p == NULL) termDie("malloc in rowAddRow p");
	memcpy(p, rows.rw[pos].chars, t.cur.x);
	p[t.cur.x] = '\0';
	rowFree(&rows.rw[pos]);
	rows.rw[pos].chars = malloc(t.cur.x + 1);
	if (rows.rw[pos].chars == NULL) termDie("malloc in rowAddRow chars until cursor");
	memcpy(rows.rw[pos].chars, p, t.cur.x + 1);
	free(p);
	rows.rw[pos].size = t.cur.x;
	updateRender(&rows.rw[pos]);

	if (pos != rows.rownum - 1) {
		rowFree(&rows.rw[pos + 1]);
		rows.rw[pos + 1].chars = malloc(strlen(s) + 1);
		if (rows.rw[pos + 1].chars == NULL) termDie("malloc in rowAddRow chars next row");
		memcpy(rows.rw[pos + 1].chars, s, strlen(s) + 1);
		rows.rw[pos + 1].size = strlen(s);
		updateRender(&rows.rw[pos + 1]);
	} else rowAddLast(s, l);

	free(s);
}

void rowFree (row *rw) // WIP
{
	free(rw->render);
	free(rw->chars);
	rw->size = 0;
	rw->r_size = 0;
}

void rowCpy (row *to, row *from) // WIP
{ 
	rowFree(to);
	to->chars = (char*) malloc(strlen(from->chars) + 1);
	if (to->chars == NULL) termDie("malloc in rowCpy");
	to->size = from->size;
	memcpy(to->chars, from->chars, to->size);
	to->chars[to->size] = '\0';
	updateRender(to);
}

void rowAddString (row *rw, char *s, int len, int pos)
{
	char *temp = realloc(rw->chars, rw->size + len + 1);
	if (temp == NULL) termDie("realloc in rowAddString");
	rw->chars = temp;
	if (pos == -1 || pos == rw->size) {
		memcpy(&rw->chars[rw->size], s, len);
		rw->size += len;
		rw->chars[rw->size] = '\0';
	} else {
		memcpy(&rw->chars[rw->size], &rw->chars[rw->size - len], len);
		memcpy(&rw->chars[rw->size - len], s, len);
		rw->size += len;
		rw->chars[rw->size] = '\0';
	}
	updateRender(rw);
}

void rowDeleteRow (int pos)
{
	for (; pos < rows.rownum - 1; pos++) {
		rowCpy(&rows.rw[pos], &rows.rw[pos + 1]); // rowcpy already frees the row
	}
	rows.rownum--;
	rowFree(&rows.rw[rows.rownum]);
	row *temp = realloc(rows.rw, sizeof(row) * rows.rownum);
	if (temp == NULL) termDie("malloc in rowDeleteRow");
	rows.rw = temp;
}

/* ----------------------------- row operations --------------------------- */

/* take care of the cursor movement */
/*void cursorMove (int a)
{
	switch (a) {
		case (KEY_LEFT):
			//if (t.cur.x <= 0 && !t.cur.off_x) {
			if (t.cur.x <= 0) {
				if (t.cur.yy) {
					t.cur.y--;
					t.cur.yy--;
					t.cur.x = rows.rw[t.cur.yy].size;
				}
			} else t.cur.x--;
			break;
			
		case (KEY_RIGHT):
			if (t.cur.x >= rows.rw[t.cur.yy].size) {
				if (t.cur.yy < rows.rownum - 1) {
					t.cur.y++;
					t.cur.yy++;
					//if (t.cur.off_x) t.cur.off_x = 0;
					//t.cur.x = rows.rw[t.cur.yy].size;
					t.cur.x = 0;
				}
			} else t.cur.x++;
			break;
			
		case (KEY_UP):
			if (t.cur.yy > 0) {
				if (t.cur.y) {
					t.cur.y--;
					t.cur.yy--;
				if (t.cur.x > rows.rw[t.cur.yy].size) {
					//if (t.cur.off_x) t.cur.off_x = 0;
					t.cur.x = rows.rw[t.cur.yy].size;
				}
			}
			break;

		case (KEY_DOWN):
			if (t.cur.yy < rows.rownum - 1) {
				t.cur.y++;
				t.cur.yy++;
				if (t.cur.x > rows.rw[t.cur.yy].size) {
					//if (t.cur.off_x) t.cur.off_x = 0;
					t.cur.x = rows.rw[t.cur.yy].size;
				}
			}
			break;
		}
	}
}*/


/*void curUpdateRender (void)
{
	//Set y offset 
	if (t.cur.y >= t.dim.y) {
		if (t.cur.y == t.dim.y) t.cur.off_y++;
		else t.cur.off_y += t.cur.y - t.dim.y;
		
		t.cur.y = t.dim.y - 1;
		
	} else if (t.cur.y <= 0 && t.cur.off_y > 0) {
		t.cur.off_y--;
		t.cur.y = 0;
	}

	//Old curRealToRender()
	t.cur.r_x = curRealToRender(&rows.rw[t.cur.yy], t.cur.x);


	if (t.cur.r_x >= t.dim.x) {
		t.cur.off_x += (t.cur.r_x > t.dim.x ) ? (t.cur.r_x - t.dim.x) : 1;
		t.cur.r_x = t.dim.x - 1;
	} else if (t.cur.r_x <= 0) {
		t.cur.off_x -= (t.cur.off_x > 0) ? 0 : t.cur.off_x;
		t.cur.r_x = 0;
	}
	//convert the cursor from real to render
	 // and update other cursor info
	t.cur.yy = t.cur.y + t.cur.off_y;
	//t.cur.xx = t.cur.x + t.cur.off_x;
	//t.cur.d_x = t.cur.r_x - t.cur.x;
} */

/* convert the cursor matchoing the memory to the drawn one */
/*
int curRealToRender (row *rw, int c_x)
{
	static int r_x = 0, i;
	for (i = 0, r_x = 0; i < c_x; i++) {
		if (rw->chars[i] == '\t') r_x += (TABSIZE - 1) - (r_x % TABSIZE);
		r_x++;
	}
	return r_x;
}*/

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
			rowAddString(&rows.rw[t.cur.y - 1], rows.rw[t.cur.y].chars, rows.rw[t.cur.y].size, -1);
			rowDeleteRow(t.cur.y);
			t.cur.y--;
		} else {
			rowDeleteChar(&rows.rw[t.cur.y], 0, t.cur.x);
		}
	} else {
		if (t.cur.x >= rows.rw[t.cur.y].size) {
			rowAddString(&rows.rw[t.cur.y], rows.rw[t.cur.y + 1].chars, rows.rw[t.cur.y + 1].size, -1);
			rowDeleteRow(t.cur.y + 1);
		} else {
			rowDeleteChar(&rows.rw[t.cur.y], 1, t.cur.x);
		}
	}
}

/*--------------------------------- garbage ------------------------------------*/

void updateInfo (void)
{
	getmaxyx(stdscr, t.dim.y, t.dim.x);
	t.dim.y -= 1;
	t.pad = decimalSize(rows.rownum - 1);
	t.dim.x -= t.pad + 1;
}

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
			} else
				t.cur.x--;
			break;
			
		case (KEY_RIGHT):
				if (t.cur.x >= rows.rw[t.cur.y].size) {
					if (t.cur.y < rows.rownum - 1) {
						t.cur.y++;
						t.cur.x = 0;
					}
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
	// y
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
	static int i;
	for (i = 0, t.cur.r_x = 0; i < t.cur.x; i++) {
		if (rows.rw[t.cur.y].chars[i] == '\t') t.cur.r_x += (TABSIZE - 1) - (t.cur.r_x % TABSIZE);
		t.cur.r_x++;
	}

	if (t.cur.r_x >= t.cur.off_x && t.cur.r_x < t.dim.x) {
		//ok
	} else if (t.cur.r_x < t.cur.off_x) {
		t.cur.off_x -= t.cur.off_x - t.cur.r_x;
		t.cur.r_x = 0;
	
	} else if (t.cur.r_x >= t.cur.off_x + t.dim.x) {
		t.cur.off_x += t.cur.r_x - t.cur.off_x - t.dim.x;
		t.cur.r_x = t.dim.x - 1;
	}
}
/*--------------------------------- testing ------------------------------------*/
