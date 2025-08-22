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

void refreshScreen();
void clearScreen();
int readKey();
int cursorIsAt(int *x, int *y);
void setStatusMessage(const char* format_str, ...);
void expandBuffer(char* buffer, size_t buffer_size);
						
struct search;

char *setPrompt(char *prompt_buffer, void (*callback)(char *, int));

/* DEFINITIONS */
#define CTRL_P(c) ((c) & 0x1f)
#define CLE_VER "0.1.0"
#define CLE_TAB_SIZE 4
#define TIMES_TO_QUIT_UNSAVED 1

#define SUCCESS 1
#define FAILURE 0

/* ERROR HANDLING */

#define updBufQueue(ubuf, s, len) \
	do { if(queue(ubuf, s, len) != (len)) died_of("Updating buffer error."); } while(0)

void died_of(const char *cause)
{
	perror(cause);
	exit(1);
}

/* DATA */

struct erow
{
	int size;
	int render_size;

	char *text;
	char *render_text;

	unsigned char* highlight;
	int last_inString;
	int last_inMLComment;
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
 	_DELETE
};

enum highlight_color
{
	HL_DIGIT = 31,
	HL_STRING,
	HL_DATATYPE,
	HL_KEYWORD,
	HL_MATCH,
	HL_COMMENT,
	HL_DEFAULT
};

/* TERMINAL SETUP */

struct editorConfig
{
	int cursorx, cursory;	
	int render_cursorx;
	int x_offset, y_offset;

	int screencols, screenrows;
	int numrows;

	struct erow *row;

	int isModified;

	char *filename;
	
	char status_message[80];		
	time_t status_message_time;

	struct termios cooked;

	struct syntax *SxConf;
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

/* SYNTAX HIGHLIGHTING */

#define HL_NUMBER (1<<0)
#define HL_STRING (1<<1)

struct syntax
{
	char *filetype;
	char **file_exts;

	char *MLComment_start;
	char *MLComment_end;
	char *SLComment;

	char *stringDelimiters;

	char **datatypes;
	char **keywords;

	int HL_flags;
};

char *C_mainExts[] = { "*.c", "*.h", NULL };

char C_stringDelimiters[] = { '"', '\'', '\0' };

char *C_mainDatatypes[] = 
{ 
	"int", "char", "float", "double", "long", "short", "const", "volatile", "restrict", "static",
	"_Bool", "_Complex", "_Imaginary", "inline", "register", 
	"signed", "unsigned", "void", "struct", "enum", "union", "typedef", NULL 
};         

char *C_mainKeywords[] = 
{ 
	"if", "else", "for", "do", "while", "break", "#define", "#include",
	"goto", "sizeof", "extern", "auto", "default",
	"_Alignas", "_Alignof", "_Atomic", "_Generic", "_Noreturn", "_Static_assert", "_Thread_local",
	"continue", "switch", "case", "return", NULL 
};

struct syntax highlight_database[] = 
{
	{
		"c",
		C_mainExts,
	
		"/*",
		"*/",
		"//",

		C_stringDelimiters,
	
		C_mainDatatypes,
		C_mainKeywords,

		HL_NUMBER | HL_STRING
	},
};

#define HL_ENTRIES (sizeof(highlight_database) / sizeof(highlight_database[0]));

int syntax_defineString(struct erow *row, int endOfRow, int inString, int *i)
{
	int isEscapedQuote = 0;
	isEscapedQuote = (!endOfRow && inString && row->render_text[*i] == '\\' && (row->render_text[*i + 1] == '\'' || row->render_text[*i + 1] == '"'));
	if(isEscapedQuote)
	{
		memset(&row->highlight[*i++], HL_STRING, 2);
		return inString;
	} 

	int foundQuote = 0;
	char *quote = strchr(EConf.SxConf->stringDelimiters, row->render_text[*i]);
	if(quote) foundQuote = abs(inString - *quote);
/*
*	1. if quote is found and inString == 0, we get ASCII value of a found quote. 
*	2. if inString == *quote, we get 0. 
*	3. if we find the other quote, we get 5 (" is 34 and ' is 39 so we get the difference). Covered by default case just carrying on the current state.  
*/
	switch(foundQuote)
	{
		case 0:
			return 0;

		case '\'':
		case '"':
			return *quote;

		default: return inString;
	}  
}

#define YES 1
#define NO 2

int syntax_defineMLComment(struct erow *row, int endOfRow, int inMLComment, int *i)
{
	if(endOfRow) return inMLComment;	

	int MLComment_open = 1;
	int MLComment_closed = 1; 
/*
*	Both are set to 1 to not confuse with strncmp() status report, 
*	since strncmp() returns 0 if strings are equal. (I hate it)
*/
	if(!inMLComment) MLComment_open = strncmp(EConf.SxConf->MLComment_start, &row->render_text[*i], strlen(EConf.SxConf->MLComment_start));
	else 			MLComment_closed = strncmp(EConf.SxConf->MLComment_end, &row->render_text[*i], strlen(EConf.SxConf->MLComment_end));

	if(!endOfRow && !MLComment_open) 
	{
		memset(&row->highlight[*i], HL_COMMENT, strlen(EConf.SxConf->MLComment_start) - 1); 
		*i += strlen(EConf.SxConf->MLComment_start) - 1;
		return YES;
	}
	else if(!endOfRow && !MLComment_closed)
	{
		memset(&row->highlight[*i], HL_COMMENT, strlen(EConf.SxConf->MLComment_end));
		*i += strlen(EConf.SxConf->MLComment_end) - 1;
		return NO;
	}
/*	What I did:
*	1. If opening or closing is found, I paint it and shift the counter index straight to the insides of the comment.
*	2. Else I carry on current state unchanged.
*/
	else return inMLComment;
}

int syntax_defineSLComment(struct erow *row, int endOfRow, int inSLComment, int *i)
{
	if(endOfRow) return inSLComment;
	
	int isSLComment = 1;
/*
*	inSLComment set to 1 with the same purpose - to not confuse outside state and success of strncmp().  
*/
	isSLComment = strncmp(&row->render_text[*i], EConf.SxConf->SLComment, strlen(EConf.SxConf->SLComment));

	if(!isSLComment)
	{
		memset(&row->highlight[*i], HL_COMMENT, strlen(EConf.SxConf->SLComment) - 1);
		*i += strlen(EConf.SxConf->SLComment) - 1;
		return YES;
	}
/*
*	If entry point is found - paint it, move cursor past it, and carry on the YES state. Else just carry on the current state.
*/
	else return inSLComment;
}

int syntax_defineSeparator(int c)
{
	return (isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[]{}:;", c) != NULL);	
}

int syntax_defineDigit(struct erow *row, int i)
{
	int currIsDigit = (row->render_text[i] >= '0' && row->render_text[i] <= '9');
	int prevIsDigitOrSeparator = (i > 0 && syntax_defineSeparator(row->render_text[i - 1]));
	int nextIsDigitOrSeparator = (syntax_defineSeparator(row->render_text[i + 1]));

	if(prevIsDigitOrSeparator && currIsDigit && nextIsDigitOrSeparator) return YES;
	else if(i == 0 && currIsDigit && nextIsDigitOrSeparator) return YES;
	else return NO;
}

int syntax_defineKeyword(struct erow *row, int i)
{
	char **keywords = EConf.SxConf->keywords;

	for(int j = 0; keywords[j]; j++)
	{
		int keyword_len = strlen(keywords[j]);
		if(!strncmp(&row->render_text[i], keywords[j], strlen(keywords[j])) && syntax_defineSeparator(row->render_text[i + keyword_len]))
		{
			int prevIsSeparator = ((i == 0) || (i > 0 && syntax_defineSeparator(row->render_text[i - 1])));
			if(prevIsSeparator) return keyword_len;
		}
	}
/*	What is done here:
*	1. If keyword is found, return its length so we know how many characters have to be painted and to move i past the keyword.
*	2. Else return NO state.
*/
	return NO;
}

int syntax_defineDatatype(struct erow *row, int i)
{
	char **datatypes = EConf.SxConf->datatypes;

    for(int j = 0; datatypes[j]; j++)
    {
        int datatype_len = strlen(datatypes[j]);
        if(!strncmp(&row->render_text[i], datatypes[j], strlen(datatypes[j])) && syntax_defineSeparator(row->render_text[i + datatype_len]))
        {
            int prevIsSeparator = ((i == 0) || (i > 0 && syntax_defineSeparator(row->render_text[i - 1])));
            if(prevIsSeparator) return datatype_len;
        }
    }
/*  What is done here:
*   This function is a straight up copy of a keyword one. Just the word "keyword" is changed to "datatype".
*/
    return NO;
}

void syntax_updateIndexes(struct erow *row)
{
	unsigned char *t_highlight = realloc(row->highlight, row->render_size);
	if(!t_highlight)
	{
		if(row->highlight) free(row->highlight);
		row->highlight = NULL;
		return;
	}

	int endOfRow;
	int inSLComment = 0;
	int isDigit, isKeyword, isDatatype;
	int inString = row->last_inString;
	int inMLComment = row->last_inMLComment;
	
	for(int i = 0; i < row->render_size; i++)
	{
		endOfRow = (i == row->render_size - 1);
		inString = syntax_defineString(row, endOfRow, inString, &i);
		inMLComment = syntax_defineMLComment(row, endOfRow, inMLComment, &i);
		isDigit = syntax_defineDigit(row, i);
		isKeyword = syntax_defineKeyword(row, i);
		if(endOfRow)
		{
			row->last_inString = inString;
			row->last_inMLComment = inMLComment;
		}
		
		switch(inString)
		{
			case 0: break;

			case '\'':
			case '"': 
				row->highlight[i] = HL_STRING;
				break; 
		}

		switch(inMLComment)
		{
			case NO: break;
			
			case YES: 
				row->highlight[i] = HL_COMMENT;
				break;
		}

		switch(inSLComment)
		{
			case YES:
				row->highlight[i] = HL_COMMENT;
				break;

			case NO:
				break;
		}

		switch(isDigit)
		{
			case YES:
				row->highlight[i] = HL_DIGIT;
				break;

			case NO: break;
		}

		switch(isKeyword)
		{
			case NO: break;
		
			default:
				for(int j = i; j < i + isKeyword; j++) row->highlight[j] = HL_KEYWORD;
				i += isKeyword - 1;
				break;
		}

		switch(isDatatype)
		{
			case NO: break;

			default:
				for(int j = i; j < i + isDatatype; j++) row->highlight[j] = HL_DATATYPE;
				i += isDatatype - 1;
				break;
		}
	}
}

/* ROW PROCESSING */

int CLE_cursorToRender(struct erow* row, int cursorx)
{
	int render_cursorx = 0;
	for(int i = 0; i < cursorx; i++)
	{
		if(row->text[i] == '\t') render_cursorx += CLE_TAB_SIZE - (render_cursorx % CLE_TAB_SIZE);
		else render_cursorx++;
	}
	return render_cursorx;
}

int CLE_renderToCursor(struct erow* row, int renderx)
{
	int cursorx;
	int t_renderx = 0;
	
	for(cursorx = 0; cursorx < row->size; cursorx++)
	{
		if(row->text[cursorx] == '\t') t_renderx += CLE_TAB_SIZE - (t_renderx % CLE_TAB_SIZE);
		else t_renderx++;

		if(t_renderx > renderx) return cursorx;
	}
	return cursorx;
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

void CLE_appendRow (int to_process, char* s, size_t len)
{
	if(to_process < 0 || to_process > EConf.numrows) return;

	EConf.row = realloc(EConf.row, sizeof(struct erow) * (EConf.numrows + 1));	
	memmove(&EConf.row[to_process + 1], &EConf.row[to_process], sizeof(struct erow) * (EConf.numrows - to_process));

	EConf.row[to_process].size = len;
	EConf.row[to_process].text = malloc(len + 1);
	
	memcpy(EConf.row[to_process].text, s, len);
	EConf.row[to_process].text[len] = '\0';
	EConf.row[to_process].render_size = 0;
	EConf.row[to_process].render_text = NULL;
	EConf.row[to_process].highlight = NULL;
	if(to_process > 0)
	{
		EConf.row[to_process].last_inString = EConf.row[to_process - 1].last_inString;
		EConf.row[to_process].last_inMLComment = EConf.row[to_process - 1].last_inMLComment;
	}
	else
	{
		EConf.row[to_process].last_inString = 0;
		EConf.row[to_process].last_inMLComment = 0;
	}
	
	CLE_updateRow(&EConf.row[to_process]);

	EConf.isModified++;
	EConf.numrows++;
}

void CLE_freeRow(struct erow* row)
{
	free(row->render_text);
	free(row->text);
	free(row->highlight);
}

void CLE_delRow(int at)
{
	if(at < 0 || at >+ EConf.numrows) return;

	CLE_freeRow(&EConf.row[at]);
	memmove(&EConf.row[at], &EConf.row[at + 1], sizeof(struct erow) * (EConf.numrows - at - 1));
	EConf.numrows--;
	EConf.isModified++;
}

void CLE_insertCharToRow(struct erow* row, int at, int c)
{
	if(at < 0 || at > row->size) at = row->size;
	row->text = realloc(row->text, row->size + 2);

	memmove(&row->text[at + 1], &row->text[at], row->size - at + 1);
	row->size++;
	row->text[at] = c;
	CLE_updateRow(row);
	EConf.isModified++;
}

void CLE_uniteRows(struct erow* row, char* s, size_t len)
{
	row->text = realloc(row->text, row->size + len + 1);

	memcpy(&row->text[row->size], s, len);

	row->size += len;
	row->text[row->size] = '\0';

	CLE_updateRow(row);

	EConf.isModified++;
}

void CLE_delCharFromRow(struct erow* row, int at)
{
	if(at < 0 || at > row->size) at = row->size;
	
	memmove(&row->text[at - 1], &row->text[at], row->size - at);
	row->size--;
	CLE_updateRow(row);
	EConf.isModified++;
}

/* ENTIRE EDITOR OPERATIONS */

void insertChar(int c)
{
	if(EConf.cursory == EConf.numrows) 
	{
		CLE_appendRow(EConf.numrows, "", 0);	
	}	

	CLE_insertCharToRow(&EConf.row[EConf.cursory], EConf.cursorx, c);
	EConf.cursorx++;
}

void insertNewRow()
{
	if(EConf.cursorx == 0)
	{
		CLE_appendRow(EConf.cursory, "", 0);
	}
	else
	{
		struct erow* row = &EConf.row[EConf.cursory];
		CLE_appendRow(EConf.cursory + 1, &row->text[EConf.cursorx], row->size - EConf.cursorx);
		row = &EConf.row[EConf.cursory];
		row->size = EConf.cursorx;
		row->text[row->size] = '\0';
		CLE_updateRow(row);
	}
	EConf.cursory++;
	EConf.cursorx = 0;
}

void delChar()
{
	if(EConf.cursory == EConf.numrows) return;
	if(EConf.cursorx == 0 && EConf.cursory == 0) return;
	
	struct erow* row = &EConf.row[EConf.cursory];
	if(EConf.cursorx > 0)
	{
		CLE_delCharFromRow(row, EConf.cursorx);
		EConf.cursorx--;
	}
	else if(EConf.cursorx == 0)
	{
		EConf.cursorx = EConf.row[EConf.cursory - 1].size;
		CLE_uniteRows(&EConf.row[EConf.cursory - 1], row->text, row->size);
		CLE_delRow(EConf.cursory);
		EConf.cursory--;
	}
}

/* FILE OPERATIONS */

int cle_fileTruncate(int file_len, char* buffer)
{
	int cle = open(EConf.filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if(cle == -1)
	{
		free(buffer);
		return 0;
	}
	if(write(cle, buffer, file_len) == file_len)
	{
		close(cle);
		free(buffer);
		setStatusMessage("Success. %dB saved.", file_len);
		EConf.isModified = 0;
		return 1;
	}
	close(cle);
	free(buffer);
	setStatusMessage("Fail. I/O error: %s", strerror(errno));
	return 0;
}

char* cle_fileToSingleString(int* buffer_len)
{
	int file_len = 0;
	for(int j = 0; j < EConf.numrows; j++) file_len += EConf.row[j].size + 1;
	*buffer_len = file_len;

	char* buffer = malloc(file_len);
	char* ptr = buffer;

	for(int j = 0; j < EConf.numrows; j++)
	{
		memcpy(ptr, EConf.row[j].text, EConf.row[j].size);
		ptr += EConf.row[j].size;
		*ptr = '\n';
		ptr++;
	}
	return buffer;
}

void cle_setFilename(char* filename)
{
	free(EConf.filename);
	EConf.filename = strdup(filename);
	if(!EConf.filename) return;
}

void cle_launch(char* filename)
{
	cle_setFilename(filename);

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
		CLE_appendRow(EConf.numrows, line, linelen);			
	}
	free(line);
	fclose(fp);
	EConf.isModified = 0;
}

void cle_saveFile()
{
	if(EConf.filename == NULL)
	{
		EConf.filename = setPrompt("Save as: %s [ESC to cancel]", NULL);
		if(EConf.filename == NULL)
		{
			setStatusMessage("Saving aborted.");
			return;
		}
	}
	
	int file_len;
	char* file_buffer = cle_fileToSingleString(&file_len);
	
	if(!cle_fileTruncate(file_len, file_buffer))
	{
		setStatusMessage("File truncation failed miserably.");
	}
}

/* SEARCH */

struct search
{
	int initial_size;
	int actual_size;
	int current_capacity;

	int* match_x;
	int* match_y;

	int current_match;
	int cursor_offset;
};

struct search *search_init()
{
	struct search *SConf = malloc(sizeof *SConf);
	if(!SConf) return NULL;

	SConf->initial_size = 64;
	SConf->actual_size = 0;
	SConf->current_capacity = SConf->initial_size;

	SConf->match_x = malloc(SConf->initial_size * sizeof(int));
	if(!SConf->match_x)
	{
		free(SConf);
		return NULL;
	}
	SConf->match_y = malloc(SConf->initial_size * sizeof(int));
	if(!SConf->match_y)
	{
		free(SConf->match_x);
		free(SConf);
		return NULL;
	}

	SConf->current_match = 0;
	SConf->cursor_offset = 0;
	return SConf;
}

void search_emptyContents(struct search *SConf)
{
	if(!SConf) return;	

	if(SConf->match_x) 
	{
		free(SConf->match_x);
		SConf->match_x = NULL;
	}
	if(SConf->match_y) 
	{
		free(SConf->match_y);
		SConf->match_y = NULL;
	}

	SConf->actual_size = 0;
	SConf->current_capacity = 0;
	SConf->current_match = 0;
	SConf->cursor_offset = 0;
}

void search_resetContents(struct search *SConf)
{
	SConf->current_capacity = SConf->initial_size;
	SConf->actual_size = 0;
	SConf->current_match = 0;
	SConf->cursor_offset = 0;

	int* temp_x = realloc(SConf->match_x, SConf->initial_size * sizeof(int));
	if(!temp_x)
	{
		search_emptyContents(SConf);
		return;
	}
	else SConf->match_x = temp_x;

	int* temp_y = realloc(SConf->match_y, SConf->initial_size * sizeof(int));
	if(!temp_y)
	{
		search_emptyContents(SConf);
		return;
	}
	else SConf->match_y = temp_y;	

	memset(SConf->match_x, 0, SConf->initial_size * sizeof(int));
	memset(SConf->match_y, 0, SConf->initial_size * sizeof(int));
}

void search_expandArrays(struct search *SConf)
{
	SConf->current_capacity *= 2;

	int* temp_x = realloc(SConf->match_x, SConf->current_capacity * sizeof(int));
	if(!temp_x)
	{
		search_emptyContents(SConf);
		return;
	}
	else SConf->match_x = temp_x;

	int* temp_y = realloc(SConf->match_y, SConf->current_capacity * sizeof(int));
	if(!temp_y)
	{
		search_emptyContents(SConf);
		return;
	}
	else SConf->match_y = temp_y;
}

void search_moveCursor(struct search *SConf, int match_number)
{
	EConf.cursory = SConf->match_y[match_number];
	EConf.y_offset = (EConf.cursory - SConf->cursor_offset > 0)? EConf.cursory - SConf->cursor_offset : 0;
	EConf.cursorx = SConf->match_x[match_number];	
}

void search_findMatches(struct search* SConf, char* query)
{
	for(int i = 0; i < EConf.numrows; i++)
	{
		struct erow* row = &EConf.row[i];
		char *match = strstr(row->render_text, query);
		if(match)
		{
			SConf->match_x[SConf->actual_size] = CLE_renderToCursor(row, match - row->render_text);
			SConf->match_y[SConf->actual_size] = i;
			SConf->actual_size++;
		}
		if(SConf->actual_size >= SConf->current_capacity) search_expandArrays(SConf);
	}
	if(SConf->actual_size > 0) search_moveCursor(SConf, 0);
}

void search_traverseMatches(struct search *SConf, int c)
{
	if(SConf->actual_size == 0) return;

	switch(c)
	{
		case _UP:
		case _LEFT:
			SConf->current_match = (SConf->current_match - 1 < 0)? SConf->actual_size - 1 : SConf->current_match - 1;
			break;

		case _DOWN:
		case _RIGHT:
			SConf->current_match = (SConf->current_match + 1 > SConf->actual_size - 1)? 0 : SConf->current_match + 1;
			break;
	}
	search_moveCursor(SConf, SConf->current_match);
}

void search_control(char* query, int c)
{
	static struct search *SConf = NULL;

	if(!SConf) SConf = search_init();

	if(!SConf) return;	

	int navigation = (c == _UP || c == _DOWN || c == _LEFT || c == _RIGHT);
	int cancellation = (c == '\x1b' || c == '\r' || c == CTRL_P('f'));
	int termination = (!SConf->match_x || !SConf->match_y);

	if(termination || cancellation)
	{
		if(SConf->match_x || SConf->match_y) search_emptyContents(SConf);
		free(SConf);
		SConf = NULL;
		return;
	}
	else if(navigation)
	{
		if(SConf->match_x && SConf->match_y && SConf->actual_size > 0) search_traverseMatches(SConf, c);
	}
	else if(query)
	{
		search_resetContents(SConf);
		SConf->cursor_offset = EConf.cursory - EConf.y_offset;	
		search_findMatches(SConf, query);
		SConf->cursor_offset = EConf.cursory - EConf.y_offset;
	}
}

void cle_search()
{
	char* query = setPrompt("Search for: %s [ESC to cancel]", search_control);
	if(query) free(query);
}

/* BUFFER FOR WRITE QUEUE */

struct ubuf
{
    char *buffer;
    int len;
};

#define UBUF_INIT {NULL, 0}

int queue(struct ubuf* ubuf, const char* s, int len)
{
    char* new = realloc(ubuf->buffer, ubuf->len + len);

    if(new == NULL) return 0;
    memcpy(&new[ubuf->len], s, len);
    ubuf->buffer = new;
    ubuf->len += len;

    return len;
}

void updBufFree(struct ubuf* ubuf)
{
    free(ubuf->buffer);
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
void expandBuffer(char* buffer, size_t buffer_size)
{
	buffer_size *= 2;
	char *t_buffer = realloc(buffer, buffer_size); //Double the size and reallocate new amount of memory to the same block.
	if(!t_buffer)
	{
		free(buffer);
		buffer = NULL;
	}
	else buffer = t_buffer;
}

char* setPrompt(char* prompt, void (*callback)(char *, int))
{
	size_t buffer_size = 128;
	char* prompt_buffer = malloc(buffer_size);

	size_t prompt_len = 0;
	prompt_buffer[0] = '\0';
	int noPrompt, isValidChar, deleteChar;

	while(1)
	{
		setStatusMessage(prompt, prompt_buffer);
		refreshScreen();

		int c = readKey();
		deleteChar = (c == _DELETE || c == _BACKSPACE);
		noPrompt = (prompt_len == 0 && c == '\r');
		isValidChar = (!iscntrl(c) && c < 128);
		if(deleteChar)
		{
			if(prompt_len != 0) prompt_buffer[--prompt_len] = '\0';
		}
		else if(c == '\x1b')
		{
			setStatusMessage("");
			if(callback) callback(prompt_buffer, c);
			free(prompt_buffer);
			return NULL;
		}
		else if(noPrompt)
		{
			setStatusMessage("");
			if(callback) callback(prompt_buffer, c);
			return prompt_buffer;
		} 
		else if(isValidChar)
		{
			if(prompt_len == buffer_size - 1) expandBuffer(prompt_buffer, buffer_size);
			prompt_buffer[prompt_len++] = c;
			prompt_buffer[prompt_len] = '\0';
		}
		else if(c == '\r')
		{
			setStatusMessage("");
			if(callback) callback(prompt_buffer, c);
			return prompt_buffer;
		}
		if(callback) callback(prompt_buffer, c);
	}
}

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
		case '3': return _DELETE;

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

int moveLeft(struct erow* prev_row)
{
	if(EConf.cursorx > 0)
	{
		EConf.cursorx--;
		return 1;
	}
	else if(prev_row && EConf.cursorx == 0)
	{
		EConf.cursory--;
		EConf.cursorx = prev_row->size;
		return 1;
	}
	return 0;
}

int moveRight(struct erow* curr_row, struct erow* next_row)
{
	if(curr_row && EConf.cursorx == curr_row->size && next_row)
	{
		EConf.cursory++;
		EConf.cursorx = 0;
		return 1;
	}
	else if(curr_row && EConf.cursorx < curr_row->size)
	{
		EConf.cursorx++;
		return 1;
	}
	return 0;
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
	if(EConf.cursory < EConf.numrows - 1)
	{
		EConf.cursory++;
	}
}

void movePage(int movement)
{
	int cursor_offset = EConf.cursory - EConf.y_offset;
	for(int i = 0; i < EConf.screenrows - 1; i++) movement == PAGE_UP? moveUp() : moveDown();

	EConf.y_offset = (EConf.cursory < EConf.numrows - EConf.screenrows)? EConf.cursory - cursor_offset : EConf.numrows - EConf.screenrows;
	if(EConf.y_offset < 0) EConf.y_offset = 0;
}

void moveToEdge(int movement, struct erow* curr_row)
{
	if(curr_row) EConf.cursorx = (movement == _HOME)? 0 : curr_row->size;
}

void moveCursor(int movement)
{
	static int saved_x = 0;
	struct erow* prev_row = (EConf.cursory <= 0)? NULL : &EConf.row[EConf.cursory - 1];
	struct erow* curr_row = (EConf.cursory > EConf.numrows - 1 || EConf.cursory < 0)? NULL : &EConf.row[EConf.cursory];
	struct erow* next_row = (EConf.cursory >= EConf.numrows - 1 || EConf.cursory < 0)? NULL : &EConf.row[EConf.cursory + 1];		

	switch(movement)
	{
		case _UP:
			moveUp();
			if(prev_row && saved_x > prev_row->size) EConf.cursorx = prev_row->size;
			else if(prev_row && saved_x <= prev_row->size) EConf.cursorx = saved_x;
			break;

		case _DOWN:
			moveDown();
			if(next_row && saved_x > next_row->size) EConf.cursorx = next_row->size;
			else if(next_row && saved_x <= next_row->size) EConf.cursorx = saved_x;
			break;

		case _LEFT:
			if(moveLeft(prev_row)) saved_x = EConf.cursorx;
			break;

		case _RIGHT:
			if(moveRight(curr_row, next_row)) saved_x = EConf.cursorx;
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
}	

void processKey()
{
	static int quit_attempts = TIMES_TO_QUIT_UNSAVED;
	int c = readKey();

	struct erow* curr_row = (EConf.cursory > EConf.numrows - 1 || EConf.cursory < 0)? 
							NULL : &EConf.row[EConf.cursory];
	struct erow *next_row = (EConf.cursory >= EConf.numrows - 1 || EConf.cursory < 0)? 
							NULL : &EConf.row[EConf.cursory + 1];	

	int startOfFile = (EConf.cursorx == 0 && EConf.cursory == 0);
	
	switch(c)
	{
		case CTRL_P('q'):
			if(EConf.isModified && quit_attempts > 0) 
			{
				setStatusMessage("CTRL+Q %d more time(s) to discard unsaved changes.", quit_attempts);
				quit_attempts--;
				return;
			}
			clearScreen();
			free(EConf.filename);
			exit(0);
			break;

		case CTRL_P('s'):
			cle_saveFile(); // TODO: ESC key delay — caused by escape sequence timeout. Fix when editor is feature-complete.
			break;

		case CTRL_P('f'):
			cle_search();
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

		case '\r':
		case '\n':
			insertNewRow();
			break;

		case _BACKSPACE:
		case _DELETE:
			if(c == _DELETE && moveRight(curr_row, next_row)) delChar();
			else if(c == _BACKSPACE && !startOfFile) delChar();
			break;

		case '\x1b':
			break;

		default:
			insertChar(c);
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
		 updBufQueue(ubuf, "\r\n", 2);
	}
}

void drawStatusBar(struct ubuf* ubuf)
{
	updBufQueue(ubuf, "\x1b[7m", 4);

	char status[80];
	int status_len = snprintf(status, sizeof(status), "%.20s - %d lines %s", 
														EConf.filename? EConf.filename : "[No name]", 
														EConf.numrows,
														EConf.isModified? "[MODIFIED]" : "");

	char right_status[80];
	int right_status_len = snprintf(right_status, sizeof(right_status), "%s | %d/%d", EConf.SxConf? EConf.SxConf->filetype : "no filetype", EConf.cursory + 1, EConf.numrows);

	updBufQueue(ubuf, status, status_len);
	while(status_len < EConf.screencols)
	{
		if(EConf.screencols - status_len == right_status_len)
		{
			updBufQueue(ubuf, right_status, right_status_len);
			break;
		}
		else
		{
			updBufQueue(ubuf, " ", 1);
			status_len++;
		}	
	}
	updBufQueue(ubuf, "\x1b[m", 3);
	updBufQueue(ubuf, "\r\n", 2);
}

void drawStatusMessageBar(struct ubuf* ubuf)
{
	updBufQueue(ubuf, "\x1b[K", 3);
	int message_len = strlen(EConf.status_message);
	if(message_len > EConf.screencols) message_len = EConf.screencols;
	if(message_len && time(NULL) - EConf.status_message_time < 5) updBufQueue(ubuf, EConf.status_message, message_len);
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
	char buffer[32];
	int buffer_len = snprintf(buffer, sizeof(buffer), "\x1b[%d;%dH", (EConf.cursory - EConf.y_offset) + 1, (EConf.render_cursorx - EConf.x_offset) + 1);
	updBufQueue(ubuf, buffer, buffer_len);
}

void refreshScreen()
{
	scrollRows();

	struct ubuf ubuf = UBUF_INIT;

	updBufQueue(&ubuf, "\x1b[?25l", 6);
	updBufQueue(&ubuf, "\x1b[H", 3);

	drawTextRows(&ubuf);
	drawStatusBar(&ubuf);
	drawStatusMessageBar(&ubuf);

	traceCursor(&ubuf);	

	updBufQueue(&ubuf, "\x1b[?25h", 6);

	write(STDOUT_FILENO, ubuf.buffer, ubuf.len);
	updBufFree(&ubuf);
}

void setStatusMessage(const char* format_str, ...)
{
	va_list arg_list;
	va_start(arg_list, format_str);
	vsnprintf(EConf.status_message, sizeof(EConf.status_message), format_str, arg_list);
	va_end(arg_list);
	EConf.status_message_time = time(NULL);
}

/* INITIALIZATION */

void editorInit()
{
	EConf.cursorx = 0;	EConf.x_offset = 0;
	EConf.cursory = 0;	EConf.y_offset = 0;

	EConf.render_cursorx = 0;

	EConf.numrows = 0;

	EConf.row = NULL;
	EConf.filename = NULL;

	EConf.status_message[0] = '\0';
	EConf.status_message_time = 0;
	
	EConf.isModified = 0;
	EConf.SxConf = NULL;

	if(GWINSZ(&EConf.screenrows, &EConf.screencols) == -1) died_of("GWINSZ!");
	EConf.screenrows -= 2; // Getting one spare line for filename and other info bar and one more line for status message bar
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
	
	setStatusMessage("CTRL + { Q to quit, S to save, F to search }");
	
	while(1)
	{
		refreshScreen();
		processKey();
	}	

	return 0;
}
