#ifndef _FBUFFER_H_
#define _FBUFFER_H_

#ifndef _XOPEN_SOURCE 
	#define _XOPEN_SOURCE       /* See feature_test_macros(7) */
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>

#include "config.h"
#include "die.h"

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

/* Start of function definitions */

void bufInit (FileBuffer *b)
{
	b->rw = NULL;
	b->rownum = 0;
}

/* Add a row to the file buffer */
void rowAddLast (FileBuffer *b, char *s, int len)
{
	/* Extend the block of memory containing the lines */
	Row *newr = realloc(b->rw, (b->rownum + 1) * sizeof(Row));
	if (newr == NULL) die("realloc in rowAddLast", 0);
	b->rw = newr;

	/* Allocate memory for the line and copy it
	 * at the current row number */
	b->rw[b->rownum].chars = malloc(len  + 1);
	if (b->rw[b->rownum].chars == NULL) die("malloc in rowAddLast chars", 0);
	memcpy(b->rw[b->rownum].chars, s, len);
	b->rw[b->rownum].chars[len] = '\0';
	b->rw[b->rownum].size = len;
	updateRender(&b->rw[b->rownum]);
	b->rownum++;
}

void rowAddChar (Row *rw, char c, int pos)
{
	/* Check if char is valid */
	if (!c || (iscntrl(c) && c != '\t')) return;
	
	/* extend the string */
	rw->size++;
	char *tmp = realloc(rw->chars, rw->size + 1);
	if (tmp == NULL) die("realloc in rowAddchar", 0);
	rw->chars = tmp;

	/* make space for the new char */
	memcpy(&rw->chars[pos + 1], &rw->chars[pos], (rw->size + 1) - (pos + 1));

	/* add the new char */
	rw->chars[pos] = c;

	updateRender(rw);
}

int rowDeleteChar (Row *rw, int select, int pos) // WIP
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

void rowAddRow (FileBuffer *b, int pos, int cur) // WIP; TO DOCUMENT
{
	/* 	MOVE THE ROWS AWAY */
	/* add another line to the bottom containing the previous
	 * (last) line, effectively making new space */
	rowAddLast(b, b->rw[b->rownum - 1].chars, b->rw[b->rownum - 1].size);

	/* copy all other lines until the specified position to the next one */
	for (int last = b->rownum - 1; last > pos; last--)
		rowCpy(&b->rw[last], &b->rw[last - 1]);

	/* SPLIT THE ROW AT POS AND STORE IT */
	/* Get the row length at position after the cursor */
	int len = b->rw[pos].size - cur;
	/* create a dummy row as the new row souce */
	Row nex = EROW;
	/* allocate a buffer */
	char *s = malloc(len + 1);
	if (s == NULL) die("malloc in rowAddRow s", 0);
	/* copy the contents of the pos row after the cursor into the buffer */
	memcpy(s, &b->rw[pos].chars[cur], len);
	s[len] = '\0';
	/* update the dummy row */
	nex.chars = s;
	nex.size = strlen(s);
	
	/* MAKE THE SPLIT INTO TWO LINES */
	/* shrink the line at pos */
	char *p = realloc(b->rw[pos].chars, cur + 1);
	if (p == NULL) die("realloc in rowAddRow", 0);
	b->rw[pos].chars = p;
	/* and terminate it with null like a good boi */
	b->rw[pos].chars[cur] = '\0';
	/* update info and render */
	b->rw[pos].size = cur;
	updateRender(&b->rw[pos]);

	/* copy the dummy to the new line and free */
	rowCpy(&b->rw[pos + 1], &nex);
	rowFree(&nex);

}

void rowFree (Row *rw) // WIP
{
	/* Free both render and memory strings */
	free(rw->render);
	free(rw->chars);
	/* clear sizes */
	rw->size = 0;
	rw->r_size = 0;
	rw->utf = 0;
}

void rowCpy (Row *to, Row *from) // WIP
{
	/* Free the destination row (without destroying it) */
	rowFree(to);
	/* Allocate space for the new string */
	to->chars = (char*) malloc(strlen(from->chars) + 1);
	if (to->chars == NULL) die("malloc in rowCpy", 0);
	/* And update the size */
	to->size = from->size;
	/* Then copy the chars from the source row to the destination row */
	memcpy(to->chars, from->chars, to->size);
	to->chars[to->size] = '\0';
	/* Then update the render */
	updateRender(to);
}

void rowAppendString (Row *rw, char *s, int len)
{
	/* reallocate the row to accomodate for the added string */
	char *temp = realloc(rw->chars, rw->size + len + 1);
	if (temp == NULL) die("realloc in rowAppendString", 0);
	rw->chars = temp;

	memcpy(&rw->chars[rw->size], s, len);
	rw->size += len;
	rw->chars[rw->size] = '\0';
	
	updateRender(rw);
}

void rowDeleteRow (FileBuffer *b, int pos)
{
	if (b->rownum == 1) return;
	if (pos >= b->rownum) return;
	if (pos < 0) return;

	for (; pos < b->rownum - 1; pos++) {
		rowCpy(&b->rw[pos], &b->rw[pos + 1]); // rowcpy already frees the row
	}
	b->rownum--;
	rowFree(&b->rw[b->rownum]);
	Row *temp = realloc(b->rw, sizeof(Row) * b->rownum);
	if (temp == NULL) die("realloc in rowDeleteRow", 0);
	b->rw = temp;
}

void updateRender (Row *rw)
{
	/* count the special characters
	 * spaces, utf-8 continuation chars */
	static int tabs, i, off/*, utf_width, utf_chars*/;
	//static wchar_t wc_tmp;
	//static char *mb_p;

	tabs = 0;
	//utf_width = 0;
	//utf_chars = 0;

	for (i = 0; i < rw->size; i++) {
		if (rw->chars[i] == '\t') tabs++;

		/*else if (isStart(rw->chars[i])) {
				utf_chars++;
				wc_tmp = 0;
				mb_p = &rw->chars[i];
				//int utf_len = mblen(mb_p, rw->size - i);

				mbtowc(&wc_tmp, mb_p, rw->size - i);
				utf_width += wcwidth(wc_tmp);
				//utf_len += utf_chars;
		} */
	}
	rw->render = NULL;
	free(rw->render);

	/* Render is long as size with the added tab spaces - 1
	 * (we already count for the \t as a char) */
	rw->render = malloc(rw->size + tabs * (TABSIZE - 1) + 1);
	if (rw->render == NULL) die ("malloc in updateRender", 0);

	/* put all the characters (substituing all special chars)
	 * into the render buffer */
	for (i = 0, off = 0; i < rw->size; i++) {
		if (rw->chars[i] == '\t') {
			for (int j = 0; j < TABSIZE; j++){
				#ifdef RENDER_SHOW_TABS
					if (!j) rw->render[off++] = '|'; 
					else rw->render[off++] = ' ';
				#else
					rw->render[off++] = ' ';
				#endif
			}		
		} else {
			rw->render[off++] = rw->chars[i];
		}
	}
	rw->render[off] = '\0';
	rw->r_size = off;
	//rw->utf = utf_width - utf_chars;
}

#endif