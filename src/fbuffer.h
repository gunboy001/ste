#ifndef _FBUFFER_H_
#define _FBUFFER_H_

/* Row structure, defines actual and
 * render chars, actual and render size
 * and difference between render and
 * real size of the row 
 * Utf-8 continuation chars */
typedef struct Row {
	int size;
	int r_size;
	int utf;
	char *chars;
	char *render;
} Row;

/* Empty row initializer */
#define EROW {0, 0, 0, NULL, NULL} 

/* Rows structure (or file buffer)
 * defines rows and teh number of rows */
typedef struct FileBuffer{
	Row *rw;
	int rownum;
} FileBuffer;

void bufInit (FileBuffer *b);

void rowAddChar (Row *rw, char c, int pos);
int rowDeleteChar (Row *rw, int select, int pos);
void rowCpy (Row *to, Row *from);
void rowAddRow (FileBuffer *b, int pos, int cur);
void rowFree (Row *rw);
void rowAppendString (Row *rw, char *s, int len);
void rowDeleteRow (FileBuffer *b, int pos);
void rowAddLast (FileBuffer *b, char *s, int len);

void updateRender (Row *rw);

int isUtf (int c);
int isCont (int c);
int isStart (int c);

#endif