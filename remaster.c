/* LIBRARIES */

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>

/* PREDECLARATIONS AND DOCUMENTATIONS | P&D */

void clearScreen();
/* Clears the screen with 2 terminal instructions:
*	<esc>[2J - clear the entire screen;
*	<esc>[H - return the cursor to the upper left corner of the screen;
*/

int readKey();
/* Gets an input from keyboard and reads it into STDIN_FILENO for further processing.
*	Escape sequences are processed based on a 0-character: [ or O. 1-character: letter or digit.
*/

int cursorIsAt(int* x, int* y);
/* Sends the cursor into the bottom-right corner of the screen and then parses its current row and column.
* 	Current row and column are equal to the size of the screen, so that's how it's set.
*/

/* DEFINITIONS */

#define CTRL_P(c) ((c) & 0x1f)
#define CLE_VER "0.1.0"
#define CLE_TAB_SIZE 4

/* ERROR HANDLING */

#define updBufQueue(ubuf, s, len) \
	do { if(queue(ubuf, s, len) != (len)) died_of("Updating buffer error."); } while(0)

void died_of(const char* cause)
{
	perror(cause);
	exit(1);
}

/* DATA */

struct erow
{
	int size;
	int render_size;

	char* text;
	char* render_text;
};

enum arrows
{
	_UP = 1000,
	_DOWN,
	_LEFT,
	_RIGHT
};

enum miscMovement
{
	PAGE_UP = 1005,
	PAGE_DOWN,
	_HOME,
	_END
};

enum charManipulation
{
	_BACKSPACE = 127,
	_DEL
};

/* TERMINAL SETUP */

struct editorConfig
{
	int cursorx, cursory;	
	int render_cursorx;
	int x_offset, y_offset;

	int screencols, screenrows;
	int numrows;

	struct erow* row;

	struct termios cooked;
};

struct editorConfig EConf;

void cookTheTerminal()
{
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &EConf.cooked) == -1)
		died_of("Attribute setting error.");
}

void makeRaw()
{
	if(tcgetattr(STDIN_FILENO, &EConf.cooked) == -1)
		died_of("Attribute getting error.");
	atexit(cookTheTerminal);

	struct termios raw = EConf.cooked;
	raw.c_iflag &= ~(IXON | ICRNL | BRKINT | ISTRIP | INPCK);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 10;

	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
		died_of("Attribute setting error.");
}

int GWINSZ(int* rows, int* cols)
{
	struct winsize ws;

	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) 
	{
		if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
		return cursorIsAt(rows, cols);
	}
	else
	{
		*rows = ws.ws_row;
		*cols = ws.ws_col;
		return 0;
	}
}
/* ROW PROCESSING */

int CLE_cursorToRender(struct erow* row, int cursorx)
{
	int render_cursorx = 0;
	for(int i = 0; i < cursorx; i++)
	{
		if(row->text[i] == '\t') render_cursorx += (CLE_TAB_SIZE - 1) - (render_cursorx & CLE_TAB_SIZE);
		
		render_cursorx++;
	}
	return render_cursorx;
}
	
void CLE_updateRow(struct erow* row)
{
	int tab_count = 0;
	for(int i = 0; i < row->size; i++)
		if(row->text[i] == '\t') tab_count++;

	free(row->render_text);
	row->render_text = malloc(row->size + tab_count * (CLE_TAB_SIZE - 1) + 1);

	int j;
	int idx = 0;
	for(j = 0; j < row->size; j++)
	{
		if(row->text[j] == '\t')
		{
			do { row->render_text[idx++] = ' '; } while(idx % CLE_TAB_SIZE != 0);
		}		
		else row->render_text[idx++] = row->text[j];
	}

    row->render_text[idx] = '\0';
    row->render_size = idx;
}

void CLE_appendRow(char* s, size_t len)
{
	EConf.row = realloc(EConf.row, sizeof(struct erow) * (EConf.numrows + 1));
	
	int to_process = EConf.numrows;	// last line to be processed.

	EConf.row[to_process].size = len;
	EConf.row[to_process].text = malloc(len + 1);
	
	memcpy(EConf.row[to_process].text, s, len);
	EConf.row[to_process].text[len] = '\0';

	EConf.row[to_process].render_size = 0;
	EConf.row[to_process].render_text = NULL;

	CLE_updateRow(&EConf.row[to_process]);

	EConf.numrows++;
}

/* FILE OPERATIONS */

void cle_launch(char* filename)
{
	FILE* fp = fopen(filename, "r");
	if(!fp) died_of("File opening error.");
	
	char* line = NULL;
	size_t len = 0;
	ssize_t linelen;

	while((linelen = getline(&line, &len, fp)) != -1)
	{
		while(linelen > 0 && (line[linelen - 1] == '\n' 
						      || line[linelen - 1] == '\r'))
		linelen--;
	
		CLE_appendRow(line, linelen);			
	}
	free(line);
	fclose(fp);
}

/* BUFFER FOR WRITE QUEUE */

struct ubuf
{
    char* buf;
    int len;
};

#define UBUF_INIT {NULL, 0}

int queue(struct ubuf* ubuf, const char* s, int len)
{
    char* new = realloc(ubuf->buf, ubuf->len + len);

    if(new == NULL) return 0;
    memcpy(&new[ubuf->len], s, len);
    ubuf->buf = new;
    ubuf->len += len;

    return len;
}

void updBufFree(struct ubuf* ubuf)
{
    free(ubuf->buf);
}

/* STATUS REPORTS */

int cursorIsAt(int* rows, int* cols)
{
	char buf[32];
	unsigned int i = 0;

	if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

	while(i < sizeof(buf) - 1)
	{
		if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
		if(buf[i] == 'R') break;
		
		i++;		
	}	
	buf[i] = '\0';

	if(buf[0] != '\x1b' || buf[1] != '[') return -1;
	if(sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

	return 0;
}

/* INPUT PROCESSING */

int arrowMove(int c)
{
	switch(c)
	{
		case 'A': return _UP;
		case 'B': return _DOWN;
		case 'C': return _RIGHT;
		case 'D': return _LEFT;
		default: return '\x1b';
	}
} 

int otherMove(int key)
{
	switch(key)
	{
		case '3': return _DEL;

		case 'H':
		case '1':
		case '7': return _HOME; // 1 - xterm; 7 - VT100;

		case 'F':
		case '4':
		case '8': return _END;

		case '5': return PAGE_UP;
		case '6': return PAGE_DOWN;
		default: return '\x1b';
	}
}

enum seq_type
{
	CSI = 1337,
	SS3
};

char* readSequence()
{
	char* seq = NULL;
	int isFinalByte = 0;
	int seq_len = 0;

	for(int i = 0; !isFinalByte; i++)
	{
		char* s = realloc(seq, seq_len + 2);
		if(!s)
		{
			free(s);
			return NULL;
		}

		if(read(STDIN_FILENO, &s[i], 1) != 1) return NULL;
		isFinalByte = (s[i] == '~' || (s[i] >= 'A' && s[i] <= 'Z'));
		seq = s;
		seq_len++;
	}
	seq[seq_len] = '\0';
	
	return seq;
}

int defineSequence(char* seq)
{
	int seq_len = strlen(seq);
	
	int seq_type = 0;
	if(seq[0] == '[') seq_type = CSI;
	else if(seq[0] == 'O') seq_type = SS3;

	char final_byte = seq[seq_len - 1];
	int result = '\x1b';
	if(seq_type == CSI && (final_byte >= 'A' && final_byte <= 'D')) result = arrowMove(seq[1]);
	else if(seq_type == CSI && final_byte == '~') result = otherMove(seq[1]);
	else if(seq_type == SS3 || (final_byte >= 'A' && final_byte <= 'Z')) result = otherMove(seq[1]);

	return result;
}

int readKey()
{
	int reading;
	char c;

	while((reading = read(STDIN_FILENO, &c, 1)) != 1)
	{
		if(reading == -1 && errno != EAGAIN) died_of("Reading error.");
	}

	if(c == '\x1b')
	{
		char* seq = readSequence();
		if(!seq)
		{
			free(seq);
			return '\x1b';
		}
		return defineSequence(seq);
	}
	return c;	
}

void moveLeft(struct erow* prev_row)
{
	if(EConf.cursorx > 0)
	{
		EConf.cursorx--;
	}
	else if(EConf.cursorx == 0 && prev_row)
	{
		EConf.cursory--;
		EConf.cursorx = prev_row->size;
	}
}

void moveRight(struct erow* curr_row, struct erow* next_row)
{
	if(EConf.cursorx == curr_row->size && next_row)
	{
		EConf.cursory++;
		EConf.cursorx = 0;
	}
	else
	{
		EConf.cursorx++;
	}
}

void moveUp()
{
	if(EConf.cursory > 0)
	{
		EConf.cursory--;
	}
}

void moveDown()
{
	if(EConf.cursory < EConf.numrows)
	{
		EConf.cursory++;
	}
}

void movePage(int movement)
{
	for(int i = 0; i <= EConf.screenrows; i++)
		movement == PAGE_UP? moveUp() : moveDown();
}

void moveToEdge(int movement, struct erow* curr_row)
{
	EConf.cursorx = (movement == _HOME)? 0 : curr_row->size;
}

void moveCursor(int movement)
{
	static int saved_x = 0;

	struct erow* prev_row = (EConf.cursory <= 0)? NULL : &EConf.row[EConf.cursory - 1];
	struct erow* curr_row = (EConf.cursory >= EConf.numrows)? NULL : &EConf.row[EConf.cursory];
	struct erow* next_row = (EConf.cursory == EConf.numrows)? NULL : &EConf.row[EConf.cursory + 1];		

	switch(movement)
	{
		case _UP:
			moveUp(prev_row);
			break;

		case _DOWN:
			moveDown(next_row);
			break;

		case _LEFT:
			moveLeft(prev_row);
			saved_x = EConf.cursorx;
			break;

		case _RIGHT:
			moveRight(curr_row, next_row);
			saved_x = EConf.cursorx;
			break;			

		case PAGE_UP:
		case PAGE_DOWN:
			movePage(movement);			
			break;

		case _HOME:
		case _END:
			moveToEdge(movement, curr_row);
			break;
	}
	curr_row = (EConf.cursory >= EConf.numrows)? NULL : &EConf.row[EConf.cursory];
	if(saved_x > curr_row->size) EConf.cursorx = curr_row->size;
	else if(curr_row->size > saved_x) EConf.cursorx = saved_x;
}

void processKey()
{
	int c = readKey();

	switch(c)
	{
		case CTRL_P('q'):
			clearScreen();
			exit(0);
			break;
	
		case _UP:
		case _DOWN:
		case _LEFT:
		case _RIGHT:
		case PAGE_UP:
		case PAGE_DOWN:
		case _HOME:
		case _END:
			moveCursor(c);
			break;
	}
}

/* OUTPUT PROCESSING */

void drawTextRows(struct ubuf* ubuf)
{
	int y;
	
	for(y = 0; y < EConf.screenrows; y++)
	{
		int filerow = y + EConf.y_offset;
		if(filerow >= EConf.numrows)
		{
			updBufQueue(ubuf, "~", 1);
		}
		else
		{
			int len = EConf.row[filerow].render_size - EConf.x_offset;
			if(len < 0) len = 0;
			updBufQueue(ubuf, &EConf.row[filerow].render_text[EConf.x_offset], len);
		}

		updBufQueue(ubuf, "\x1b[K", 3);
		if(y < EConf.screenrows - 1)
		{
			updBufQueue(ubuf, "\r\n", 2);
		}
	}
}

void scrollRows()
{
	EConf.render_cursorx = 0;
	if(EConf.cursory < EConf.numrows)
	{
		EConf.render_cursorx = CLE_cursorToRender(&EConf.row[EConf.cursory], EConf.cursorx);	
	}

	if(EConf.cursory < EConf.y_offset)
	{
		EConf.y_offset = EConf.cursory;
	}
	if(EConf.cursory >= EConf.y_offset + EConf.screenrows)
	{
		EConf.y_offset = EConf.cursory - EConf.screenrows + 1;
	}
	if(EConf.render_cursorx < EConf.x_offset)
	{
		EConf.x_offset = EConf.render_cursorx;
	}
	if(EConf.render_cursorx >= EConf.screencols + EConf.x_offset)
	{
		EConf.x_offset = EConf.render_cursorx - EConf.screencols + 1;
	}
}

void clearScreen()
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
}

void traceCursor(struct ubuf* ubuf)
{
	char buf[32];
	int buf_len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (EConf.cursory - EConf.y_offset) + 1, (EConf.render_cursorx - EConf.x_offset) + 1);
	updBufQueue(ubuf, buf, buf_len);
}

void refreshScreen()
{
	scrollRows();

	struct ubuf ubuf = UBUF_INIT;

	updBufQueue(&ubuf, "\x1b[?25l", 6);
	updBufQueue(&ubuf, "\x1b[H", 3);

	drawTextRows(&ubuf);

	traceCursor(&ubuf);	

	updBufQueue(&ubuf, "\x1b[?25h", 6);

	write(STDOUT_FILENO, ubuf.buf, ubuf.len);
	updBufFree(&ubuf);
}

/* INITIALIZATION */

void editorInit()
{
	EConf.cursorx = 0;	EConf.x_offset = 0;
	EConf.cursory = 0;	EConf.y_offset = 0;

	EConf.render_cursorx = 0;

	EConf.numrows = 0;

	EConf.row = NULL;

	if(GWINSZ(&EConf.screenrows, &EConf.screencols) == -1) died_of("GWINSZ!");
}

int main(int argc, char* argv[])
{
	makeRaw();
	editorInit();
	clearScreen();
	if(argc >= 2)
	{
		cle_launch(argv[1]);
	}

	while(1)
	{
		refreshScreen();
		processKey();
	}	

	return 0;
}
