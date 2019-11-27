#ifndef _FBUFFER_H_
#define _FBUFFER_H_

/* Row structure, defines actual and
 * render chars, actual and render size
 * and difference between render and
 * real size of the row 
 * Utf-8 continuation chars */
typedef struct row {
	int size;
	int r_size;
	int utf;
	char *chars;
	char *render;
} row;

/* Empty row initializer */
#define EROW {0, 0, 0, NULL, NULL} 

/* Rows structure (or file buffer)
 * defines rows and teh number of rows */
typedef struct fbuffer{
	row *rw;
	int rownum;
} fbuffer;

void bufInit (fbuffer *b);

void rowAddChar (row *rw, char c, int pos);
int rowDeleteChar (row *rw, int select, int pos);
void rowCpy (row *to, row *from);
void rowAddRow (fbuffer *b, int pos, int cur);
void rowFree (row *rw);
void rowAppendString (row *rw, char *s, int len);
void rowDeleteRow (fbuffer *b, int pos);
void rowAddLast (fbuffer *b, char *s, int len);

void updateRender (row *rw);

int isUtf (int c);
int isCont (int c);
int isStart (int c);

#endif