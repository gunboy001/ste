#ifndef _EDITOR_CONFIG_H_
#define _EDITOR_CONFIG_H_

#define FRONTEND_NCURSES

#define TABSIZE 4 // Tab size as used in render
#define MAX_LINE 1024 // maximum line length on screen
#define PGK_DELTA 15 // Step of jump
//#define RENDER_SHOW_TABS

#define KEY_JUMP_UP       KEY_PPAGE
#define KEY_JUMP_DOWN     KEY_NPAGE
#define KEY_ROW_BEG       KEY_HOME
#define KEY_ROW_END       KEY_END
#define KEY_MOVE_LEFT     KEY_LEFT
#define KEY_MOVE_RIGHT    KEY_RIGHT
#define KEY_MOVE_UP       KEY_UP
#define KEY_MOVE_DOWN     KEY_DOWN
#define KEY_FILE_FIND     CTRL('f')

/* Modify ONLY if you know what you are doing */
#define FILENAME_MAX_LENGTH 128
#define STAT_SIZE 256
/*--------------------------------------------*/

#endif
