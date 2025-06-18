// libraries

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

//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

// definitions

#define CTRL_P(c) ((c) & 0x1f)

#define CLE_VER "0.0.1"
#define CLE_TAB_STOP 4
#define CLE_QUIT_TIMES 1

enum editorKey
{

    BACKSPACE = 127,
    _LEFT = 1000,
    _RIGHT,
    _UP,
    _DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

// data

struct erow
{
    int size;
    int r_size;
    char* chars;
    char* render;
};

struct editorConfig
{
    int cursorx, cursory;
    int renderx;
    int screenrows;
    int screencols;
    
    int col_offset;
    int row_offset;
    int numrows;

    char* filename;

    char statusmsg[80];
    time_t statusmsg_time;
    int dirty;

    struct erow* row;

    struct termios orig_termios;
};

struct editorConfig EConf;

// prototypes

void editorSetStatusMessage(const char* fmt, ...);
void editorRefreshScreen();
char* editorPrompt(char* prompt);

// terminal.

void die(const char* s)
{
    perror(s);
    exit(1);
}

void disableRawMode()
{	
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &EConf.orig_termios) == -1)
    { die("tcsetattr"); }
}

void enableRawMode() 
{   
    if(tcgetattr(STDIN_FILENO, &EConf.orig_termios) == -1)
    { die("tcgetattr"); }
    atexit(disableRawMode);

    struct termios raw = EConf.orig_termios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 10;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    { die("tcsetattr"); }
}

int editorReadKey()
{
    int nread;
    char c;

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if(nread == -1 && errno != EAGAIN) die("read");
    }
   
    if(c == '\x1b')
    {
        char seq[3];

	if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
	if(read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

	if(seq[0] == '[')
        {
	    if(seq[1] >= '0' && seq[1] <= '9')
	    {
                if(read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
		if(seq[2] == '~')
		{
		    switch(seq[1])
		    {
                        case '1': return HOME_KEY;
			case '3': return DEL_KEY;
			case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
		}
	    }

            else
	    {
	        switch(seq[1])
	        {
                    case 'A': return _UP;
                    case 'B': return _DOWN;
                    case 'C': return _RIGHT;
                    case 'D': return _LEFT;
		    case 'H': return HOME_KEY;
		    case 'F': return END_KEY;
                }
            }
        }
        
        else if (seq[0] == '0')
	{
	    switch (seq[1])
	    {
	        case 'H': return HOME_KEY;
		case 'F': return END_KEY;
            }
	}

	return '\x1b';
    }
    else
    {
        return c;
    }
}

int getCursorPosition(int* rows, int* cols)
{
    char buf[32];
    unsigned int i = 0;

    if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) - 1)
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

int GWINSZ(int* rows, int* cols)
{
    struct winsize winsz;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsz) == -1 || winsz.ws_col == 0) 
    {
	if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    }

    else
    {
        *cols = winsz.ws_col;
	*rows = winsz.ws_row;
	return 0;
    }
}

// row operations

int editorCursor_to_Render(struct erow* row, int cx)
{
    int renderx = 0;
    int idx;
    for(idx = 0; idx < cx; idx++)
    {
        if(row->chars[idx] == '\t')
	    renderx += (CLE_TAB_STOP - 1) - (renderx % CLE_TAB_STOP);
	renderx++;
    }
    return renderx;
}


int editorRender_to_Cursor(struct erow* row, int render_x)
{
    int cur_rx = 0;
    int cx;

    for(cx = 0; cx < row->size; cx++)
    {
        if(row->chars[cx] == '\t')
            cur_rx += (CLE_TAB_STOP - 1) - (cur_rx % CLE_TAB_STOP);
	cur_rx++;

	if(cur_rx > render_x) return cx;
    }

    return cx;
}

void editorUpdateRow(struct erow* row)
{
    int tabs = 0;
    int j;

    for(j = 0; j < row->size; j++)
    {
        if(row->chars[j] == '\t') tabs++;
    }

    free(row->render);
    row->render = malloc(row->size + tabs*(CLE_TAB_STOP - 1) + 1);

    int idx = 0;
    for(j = 0; j < row->size; j++)
    {
        if(row->chars[j] == '\t')
        {
	    row->render[idx++] = ' ';
	    while(idx % CLE_TAB_STOP != 0) row->render[idx++] = ' ';
	}
	else
	{
        row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->r_size = idx;
}

void editorInsertRow(int at, char* s, size_t len)
{
    if(at < 0 || at > EConf.numrows) return;

    EConf.row = realloc(EConf.row, sizeof(struct erow) * (EConf.numrows + 1));
    memmove(&EConf.row[at + 1], &EConf.row[at], sizeof(struct erow) * (EConf.numrows - at));

    EConf.row[at].size = len;
    EConf.row[at].chars = malloc(len + 1);
    
    memcpy(EConf.row[at].chars, s, len);
    
    EConf.row[at].chars[len] = '\0';

    EConf.row[at].r_size = 0;
    EConf.row[at].render = NULL;

    editorUpdateRow(&EConf.row[at]);

    EConf.numrows++;
    EConf.dirty++;
}

void editorFreeRow(struct erow* row)
{
    free(row->render);
    free(row->chars);
}

void editorDelRow(int at)
{
    if(at < 0 || at >= EConf.numrows) return;
    
    editorFreeRow(&EConf.row[at]);
    memmove(&EConf.row[at], &EConf.row[at + 1], 
            sizeof(struct erow) * (EConf.numrows - at - 1));

    EConf.numrows--;
    EConf.dirty++;
}

void editorRowInsertChar(struct erow* row, int at, int c)
{
    if(at < 0 || at > row->size) at = row->size;

    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);

    row->size++;
    row->chars[at] = c;

    editorUpdateRow(row);
    EConf.dirty++;
}

void editorRowAppendString(struct erow* row, char* s, size_t len)
{
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);

    row->size += len;
    row->chars[row->size] = '\0';
    
    editorUpdateRow(row);
    
    EConf.dirty++;
}

void editorRowDelChar(struct erow* row, int at)
{
    if(at < 0 || at > row->size) return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(row);
    EConf.dirty++;
}

// editor operations

void editorInsertChar(int c)
{
    if(EConf.cursory == EConf.numrows) 
    {
        editorInsertRow(EConf.numrows, "", 0);
    }

    editorRowInsertChar(&EConf.row[EConf.cursory], EConf.cursorx, c);
    EConf.cursorx++;
}

void editorInsertNewline()
{
    if(EConf.cursorx == 0) 
    {
        editorInsertRow(EConf.cursory, "", 0); 
    }
    else
    {
        struct erow* row = &EConf.row[EConf.cursory];
	editorInsertRow(EConf.cursory + 1, 
			&row->chars[EConf.cursorx], 
			row->size - EConf.cursorx);
	row = &EConf.row[EConf.cursory];
	row->size = EConf.cursorx;
	row->chars[row->size] = '\0';
	editorUpdateRow(row);
    }
    EConf.cursory++;
    EConf.cursorx = 0;
}

void editorDelChar()
{
    if(EConf.cursory == EConf.numrows) return;
    
    struct erow* row = &EConf.row[EConf.cursory];
    if(EConf.cursorx > 0)
    {
        editorRowDelChar(row, EConf.cursorx - 1);
	EConf.cursorx--;
    } 
    else
    {
        EConf.cursorx = EConf.row[EConf.cursory - 1].size;
	editorRowAppendString(&EConf.row[EConf.cursory - 1], row->chars, row->size);
	editorDelRow(EConf.cursory);
	EConf.cursory--;
    }
}

// file i/o

void editorOpen(char* filename)
{
    free(EConf.filename);
    EConf.filename = strdup(filename);

    FILE* fp = fopen(filename, "r");
    if (!fp) die("fopen");

    char* line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    while ((linelen = getline(&line, &linecap, fp)) != -1)
    {
        while (linelen > 0 && (line[linelen - 1] == '\n' ||
			       line[linelen - 1] == '\r')) 
	    linelen--;
        editorInsertRow(EConf.numrows, line, linelen);
    }
    free(line);
    fclose(fp);
    
    EConf.dirty = 0;
}

char* editorRowsToString(int* buff_len)
{
    int total_len = 0;
    int j;
    
    for(j = 0; j < EConf.numrows; j++)
        total_len += EConf.row[j].size + 1;
    *buff_len = total_len;

    char* buffer = malloc(total_len);
    char* p = buffer;
    
    for(j = 0; j < EConf.numrows; j++)
    {
        memcpy(p, EConf.row[j].chars, EConf.row[j].size);
        p += EConf.row[j].size;
	*p = '\n';
	p++;
    }

    return buffer;
}

void editorSave()
{
    if(EConf.filename == NULL)
    {
        EConf.filename = editorPrompt("Save as: %s [ESC to cancel]");
        if(EConf.filename == NULL)
        {
	    editorSetStatusMessage("Save aborted");
	    return;
	}
    }

    int len;
    char* buffer = editorRowsToString(&len);

    int fd = open(EConf.filename, O_RDWR | O_CREAT, 0644);
    if(fd != -1)
    {
        if(ftruncate(fd, len) != -1)
        {
	    if(write(fd, buffer, len) == len)
            {
	        close(fd);
		free(buffer);
		EConf.dirty = 0;
	        editorSetStatusMessage("Saved. %dB total.", len);
	    	return;
	    }
	}
        close(fd);
    }
    free(buffer);
    editorSetStatusMessage("Saving error: %s", strerror(errno));
}

// search

void editorSearch()
{
    char* query = editorPrompt("Search: %s [ESC to cancel]");
    if(query == NULL) return;

    int i;
    for(i = 0; i < EConf.numrows; i++)
    {
        struct erow* row = &EConf.row[i];
        char* match = strstr(row->render, query);
        if(match)
        {
            EConf.cursory = i;
            EConf.cursorx = editorRender_to_Cursor(row, match - row->render);
            EConf.row_offset = EConf.numrows;
            break;
        }
    }

    free(query);
}

// appending buffer
 
struct abuf
{
    char* b;
    int len;
};
 
#define ABUF_INIT {NULL, 0}
 
void abAppend(struct abuf* ab, const char* s, int len)
{
    char* new = realloc(ab->b, ab->len + len);

    if(new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len; 
}

void abFree(struct abuf* ab)
{
    free(ab->b);
}

// output

void editorScroll()
{
    EConf.renderx = 0;
    if(EConf.cursory < EConf.numrows)
    {
    EConf.renderx = editorCursor_to_Render(&EConf.row[EConf.cursory], EConf.cursorx);
    }

    if (EConf.cursory < EConf.row_offset)
    {
        EConf.row_offset = EConf.cursory;
    }

    if (EConf.cursory >= EConf.row_offset + EConf.screenrows)
    {
        EConf.row_offset = EConf.cursory - EConf.screenrows + 1;
    }

    if (EConf.renderx < EConf.col_offset) 
    {
        EConf.col_offset = EConf.renderx;
    }

    if (EConf.renderx >= EConf.col_offset + EConf.screencols)
    {
        EConf.col_offset = EConf.renderx - EConf.screencols + 1;
    }
}

void editorDrawRows(struct abuf* ab)
{
    int y;
    for(y = 0; y < EConf.screenrows; y++) 
    { 
	int file_row = y + EConf.row_offset;
        if(file_row >= EConf.numrows)
        {		
            if(EConf.numrows == 0 && y == EConf.screenrows / 3)
	    {
                char welcome[80];
	        int welcomelen = snprintf(welcome, sizeof(welcome),
                    "CLEditor -- version %s", CLE_VER);
	        if(welcomelen > EConf.screenrows) welcomelen = EConf.screenrows;
                int padding = (EConf.screenrows - welcomelen) / 2;
	        if(padding)
	        {
                    abAppend(ab, "~", 1);
		    padding--;
	        }
	        while(padding--) abAppend(ab, " ", 1);
	        abAppend(ab, welcome, welcomelen);
            }

	    else 
	    {
                abAppend(ab, "~", 1);	
	    }
        }
	    
        else
	{
            int len = EConf.row[file_row].r_size - EConf.col_offset;
	    if (len < 0) len = 0;
	    if (len > EConf.screencols) len = EConf.screencols;
	    abAppend(ab, &EConf.row[file_row].render[EConf.col_offset], len);
	}

	abAppend(ab, "\x1b[K", 3);
        abAppend(ab, "\r\n", 2); 	
    }
}

void editorDrawStatusBar(struct abuf* ab)
{
    abAppend(ab, "\x1b[7m", 4);

    char status[80], right_status[80];
    
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
		       EConf.filename? EConf.filename : "[No Name]",
		       EConf.numrows,
		       EConf.dirty? "[MODIFIED]" : "");
    
    int right_len = snprintf(right_status, sizeof(right_status), "%d/%d",
		             EConf.cursory + 1, EConf.numrows);
    
    if(len > EConf.screencols) len = EConf.screencols;
    abAppend(ab, status, len);
    
    while (len < EConf.screencols) 
    {
        if(EConf.screencols - len == right_len)
	{
            abAppend(ab, right_status, right_len);
	    break;
	}
        else
        {
            abAppend(ab, " ", 1);
	    len++;
        }
    }
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab)
{
    abAppend(ab, "\x1b[K", 3);
    int msg_len = strlen(EConf.statusmsg);
    if(msg_len > EConf.screencols) msg_len = EConf.screencols;
    if(msg_len && time(NULL) - EConf.statusmsg_time < 5)
        abAppend(ab, EConf.statusmsg, msg_len);
}

void editorRefreshScreen()
{
    editorScroll();

    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "\x1b[%d;%dH", 
			      	     (EConf.cursory - EConf.row_offset) + 1, 
				     (EConf.renderx - EConf.col_offset) + 1);

    abAppend(&ab, buffer, strlen(buffer));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void editorSetStatusMessage(const char* format_str, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, format_str);
    vsnprintf(EConf.statusmsg, sizeof(EConf.statusmsg), format_str, arg_ptr);
    va_end(arg_ptr);
    EConf.statusmsg_time = time(NULL);
}


// input

char* editorPrompt(char* prompt)
{
    size_t buff_size = 128;
    char* buff = malloc(buff_size);

    size_t buff_len = 0;
    buff[0] = '\0';

    while(1)
    {
        editorSetStatusMessage(prompt, buff);
	editorRefreshScreen();

	int c = editorReadKey();
	if(c == DEL_KEY || c == CTRL_P('h') || c == BACKSPACE)
	{
	    if(buff_len != 0) buff[buff_len--] = '\0';
	}
	else if(c == '\x1b')
	{
	    editorSetStatusMessage("");
	    free(buff);
	    return NULL;
	}
	else if(c == '\r' && buff_len != 0)
	{
	    editorSetStatusMessage("");
	    return buff;
	}
        else if(!iscntrl(c) && c < 128)
        {
            if(buff_len == buff_size - 1)
            {
	        buff_size *= 2;
	        buff = realloc(buff, buff_size);
	    }
            buff[buff_len++] = c;
            buff[buff_len] = '\0';
	}
    }
}

void editorMoveCursor(int key)
{
    struct erow* row = (EConf.cursory <= EConf.numrows)? 
	               &EConf.row[EConf.cursory] : NULL;

    switch(key)
    {
        case _LEFT:
            if(EConf.cursorx > 0) { EConf.cursorx--; }
	    else if(EConf.cursory > 0) 
	    { 
                EConf.cursory--;
		row = (EConf.cursory <= EConf.numrows)?
                      &EConf.row[EConf.cursory] : NULL;
		EConf.cursorx = row->size;
	    }
	break;
	
	case _DOWN:
	    if(EConf.cursory < EConf.numrows) { EConf.cursory++; }
	    if(EConf.cursorx == row->size)
	    {
                row = (EConf.cursory <= EConf.numrows)?
                      &EConf.row[EConf.cursory] : NULL;
		EConf.cursorx = row->size;
	    } 
	break;

	case _UP:
	    if(EConf.cursory > 0) { EConf.cursory--; }
	    if(EConf.cursorx == row->size)
	    {
                row = (EConf.cursory <= EConf.numrows)?
                      &EConf.row[EConf.cursory] : NULL;
                EConf.cursorx = row->size;
            }
	break;

	case _RIGHT:
	    if(EConf.cursorx < row->size) { EConf.cursorx++; }
	    else if(EConf.cursory < EConf.numrows) 
	    { 
                EConf.cursory++;
	        EConf.cursorx = 0;
	    }
	break;
    }

    row = (EConf.cursory <= EConf.numrows)? 
	  &EConf.row[EConf.cursory] : NULL;
    if(EConf.cursorx > row->size) EConf.cursorx = row->size;
}

void editorProcessKeypress()
{
    static int quit_times = CLE_QUIT_TIMES;

    int c = editorReadKey();

    switch(c)
    {
        case '\r':
            editorInsertNewline();
	    break;
	   
        case CTRL_P('q'):
	    if(EConf.dirty && quit_times > 0)
	    {
                editorSetStatusMessage("Unsaved changes. CTRL-Q %d more time(s) to quit.", quit_times);
		quit_times--;
		return;
	    }
            write(STDOUT_FILENO, "\x1b[2J", 4);
	    write(STDOUT_FILENO, "\x1b[H", 3);
	    exit(0);
	    break;

        case CTRL_P('s'):
	    editorSave();
            break;

        case CTRL_P('f'):
	    editorSearch();
	    break;

	case HOME_KEY:
	    EConf.cursorx = 0;
	    break;

	case END_KEY:
            if(EConf.cursory < EConf.numrows)
                EConf.cursorx = EConf.row[EConf.cursory].size;
            break;

        case BACKSPACE:
	case CTRL_P('h'):
	case DEL_KEY:
	    if(c == DEL_KEY) editorMoveCursor(_RIGHT);
	    editorDelChar();
	    break;

        case _UP:
	case _DOWN:
	case _LEFT:
	case _RIGHT:
	    editorMoveCursor(c);
	    break;  

        case PAGE_UP:
        case PAGE_DOWN:
	    if(c == PAGE_UP)
	    {
                EConf.cursory = EConf.row_offset;
	    }   
	    else if(c == PAGE_DOWN)
            {
	        EConf.cursory = EConf.row_offset + EConf.screenrows - 1;
	        if(EConf.cursory > EConf.numrows) EConf.cursory = EConf.numrows;
            }
	    int times = EConf.screenrows;
	    while(times--) 
                editorMoveCursor(c == PAGE_UP? _UP : _DOWN);
            break;  

        case CTRL_P('l'):
	case '\x1b':
	    break;

        default:
            editorInsertChar(c);
            break;	    
    }

    quit_times = CLE_QUIT_TIMES;
}

// initialization

void initEditor()
{
    EConf.cursorx = 0;
    EConf.cursory = 0;
    EConf.renderx = 0;

    EConf.numrows = 0;
    EConf.col_offset = 0;
    EConf.row_offset = 0;

    EConf.row = NULL;
    EConf.filename = NULL;

    EConf.statusmsg[0] = '\0';
    EConf.statusmsg_time = 0;
    EConf.dirty = 0;

    if(GWINSZ(&EConf.screenrows, &EConf.screencols) == -1) die("GWINSZ!!!");
    EConf.screenrows -= 2;
}

int main(int argc, char* argv[]) 
{
    enableRawMode();
    initEditor();
    if(argc >= 2)
    {  
        editorOpen(argv[1]);
    }

    editorSetStatusMessage("HELP: Ctrl-|Q| = quit |S| = save |F| = find");

    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    
    return 0;
}
