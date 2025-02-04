/*
*   
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*** Include section ***/

// We add them above our includes, because the header
// files we are including use the macros to decide what
// features to expose. These macros remove some compilation
// warnings. See
// https://www.gnu.org/software/libc/manual/html_node/Feature-Test-Macros.html
// for more info.
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <limits.h>

/*** Define section ***/

// This mimics the Ctrl + whatever behavior, setting the
// 3 upper bits of the character pressed to 0.
#define CTRL_KEY(k) ((k) & 0x1f)
// Empty buffer
#define ABUF_INIT {NULL, 0}
// Version code
#define MEL_VERSION "0.2.0"
// Length of a tab stop
#define MEL_TAB_STOP 4
// Times to press Ctrl-Q before exiting
#define MEL_QUIT_TIMES 2
// Highlight flags
#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)
// Status print indicators
#define NO_STATUS false
#define DEFAULT_COLUMN_MARKER 0
#define STATUS_YES true
// Max Undo/Redo Operations
// Set to -1 for unlimited Undo
// Set to 0 to disable Undo
#define ACTIONS_LIST_MAX_SIZE 80


struct a_buf {
    char* buf;
    int len;
}; 

void editorScroll();
void editorDrawRows(struct a_buf* ab);
void editorDrawStatusBar(struct a_buf* ab);
void editorDrawMessageBar(struct a_buf* ab);

/*** Data section ***/

typedef struct editor_row {
    int idx; // Row own index within the file.
    int size; // Size of the content (excluding NULL term)
    int render_size; // Size of the rendered content
    char* chars; // Row content
    char* render; // Row content "rendered" for screen (for TABs).
    unsigned char* highlight; // This will tell you if a character is part of a string, comment, number...
    int hl_open_comment; // True if the line is part of a ML comment.
} editor_row;

struct editor_syntax {
    // file_type field is the name of the filetype that will be displayed
    // to the user in the status bar.
    char* file_type;
    // file_match is an array of strings, where each string contains a
    // pattern to match a filename against. If the filename matches,
    // then the file will be recognized as having that filetype.
    char** file_match;
    // This will be a NULL-terminated array of strings, each string containing
    // a keyword. To differentiate between the two types of keywords,
    // we’ll terminate the second type of keywords with a pipe (|)
    // character (also known as a vertical bar).
    char** keywords;
    // We let each language specify its own single-line comment pattern.
    char* singleline_comment_start;
    // flags is a bit field that will contain flags for whether to
    // highlight numbers and whether to highlight strings for that
    // filetype.
    char* multiline_comment_start;
    char* multiline_comment_end;
    int flags;
};

/*** Action Definitions ***/
enum ActionType {
    CutLine,
    PasteLine,
    FlipUp,
    FlipDown,
    NewLine,
    InsertChar,
    DelChar,
};
typedef enum ActionType ActionType;

typedef struct Action Action;
typedef struct AListNode AListNode;
typedef struct ActionList ActionList;

struct Action {
    ActionType t;
    int cpos_x;
    int cpos_y;
    bool cursor_on_tilde;
    char* string;
};

struct AListNode {
    Action* action;
    AListNode* next;
    AListNode* prev;
};

struct ActionList {
    AListNode* head;
    AListNode* tail;
    AListNode* current;
    int size;
};

struct editor_config {
    int cursor_x;
    int cursor_y;
    int render_x;
    int row_offset;      // Offset of row displayed.
    int col_offset;      // Offset of col displayed.
	int line_number_offset;  // Add this new field for line numbering offset
	int column_marker;      // Position of the column marker (0 = disabled)
    int screen_rows;     // Number of rows that we can show
    int screen_cols;     // Number of cols that we can show
    int num_rows;        // Number of rows
    editor_row* row;
    int dirty;          // To know if a file has been modified since opening.
    unsigned show_line_numbers : 1;  // 1 = show, 0 = hide
	unsigned create_backup : 1;      // New: 1 = create backup, 0 = don't create backup
    char* file_name;
    char status_msg[80];
    time_t status_msg_time;
    char* copied_char_buffer;
    struct editor_syntax* syntax;
    struct termios orig_termios;
    ActionList* actions;
} ec;




enum editor_key {
    BACKSPACE = 0x7f, // 127
    ARROW_LEFT = 0x3e8, // 1000, large value out of the range of a char.
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY,
    END_KEY,
    DEL_KEY
};

enum editor_highlight {
    HL_NORMAL = 0,
    HL_SL_COMMENT,
    HL_ML_COMMENT,
    HL_KEYWORD_1,
    HL_KEYWORD_2,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH
};


/*** Filetypes ***/

char* C_HL_extensions[] = {".c", ".h", ".cpp", ".hpp", ".cc", NULL}; // Array must be terminated with NULL.
char* JAVA_HL_extensions[] = {".java", NULL};
char* PYTHON_HL_extensions[] = {".py",".pyw",".py3",".pyc",".pyo", NULL};
char* BASH_HL_extensions[] = {".sh", NULL};
char* JS_HL_extensions[] = {".js", ".jsx", NULL};
char* PHP_HL_extensions[] = {".php",".phtml", NULL};
char* JSON_HL_extensions[] = {".json", ".jsonp", NULL};
char* XML_HL_extensions[] = {".xml", NULL};
char* SQL_HL_extensions[] = {".sql", NULL};
char* RUBY_HL_extensions[] = {".rb", NULL};
char* GO_HL_extensions[] = {".go", NULL};
char* MSHELL_HL_extensions[] = {".ms", NULL};


char* C_HL_keywords[] = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "struct", "union", "typedef", "static", "enum", "case", "#include",
    "volatile", "register", "sizeof", "typedef", "union", "goto", "const", "auto",
    "#define", "#if", "#endif", "#error", "#ifdef", "#ifndef", "#undef",
    "asm" /* in stdbool.h  */ , "bool" , "true" , "fasle" , "inline" ,
    
    // C++
    "class" , "namespace" , "using" , "catch" , "delete" , "explicit" ,
    "export" , "friend" , "mutable" , "new" , "public" , "protected" ,
    "private" , "operator" , "this" , "template" , "virtual" , "throw" ,
    "try" , "typeid" ,

    "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
    "void|", "bool|", NULL
};

char* JAVA_HL_keywords[] = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "in", "public", "private", "protected", "static", "final", "abstract",
    "enum", "class", "case", "try", "catch", "do", "extends", "implements",
    "finally", "import", "instanceof", "interface", "new", "package", "super",
    "native", "strictfp",
    "synchronized", "this", "throw", "throws", "transient", "volatile",

    "byte|", "char|", "double|", "float|", "int|", "long|", "short|",
    "boolean|", NULL
};

char* PYTHON_HL_keywords[] = {
    "and", "as", "assert", "break", "class", "continue", "def", "del", "elif",
    "else", "except", "exec", "finally", "for", "from", "global", "if", "import",
    "in", "is", "lambda", "not", "or", "pass", "print", "raise", "return", "try",
    "while", "with", "yield",

    "buffer|", "bytearray|", "complex|", "False|", "float|", "frozenset|", "int|",
    "list|", "long|", "None|", "set|", "str|", "tuple|", "True|", "type|",
    "unicode|", "xrange|", NULL
};

char* BASH_HL_keywords[] = {
    "case", "do", "done", "elif", "else", "esac", "fi", "for", "function", "if",
    "in", "select", "then", "time", "until", "while", "alias", "bg", "bind", "break",
    "builtin", "cd", "command", "continue", "declare", "dirs", "disown", "echo",
    "enable", "eval", "exec", "exit", "export", "fc", "fg", "getopts", "hash", "help",
    "history", "jobs", "kill", "let", "local", "logout", "popd", "pushd", "pwd", "read",
    "readonly", "return", "set", "shift", "suspend", "test", "times", "trap", "type",
    "typeset", "ulimit", "umask", "unalias", "unset", "wait", "printf", NULL
};

char* JS_HL_keywords[] = {
    "break", "case", "catch", "class", "const", "continue", "debugger", "default",
    "delete", "do", "else", "enum", "export", "extends", "finally", "for", "function",
    "if", "implements", "import", "in", "instanceof", "interface", "let", "new",
    "package", "private", "protected", "public", "return", "static", "super", "switch",
    "this", "throw", "try", "typeof", "var", "void", "while", "with", "yield", "true",
    "false", "null", "NaN", "global", "window", "prototype", "constructor", "document",
    "isNaN", "arguments", "undefined",

    "Infinity|", "Array|", "Object|", "Number|", "String|", "Boolean|", "Function|",
    "ArrayBuffer|", "DataView|", "Float32Array|", "Float64Array|", "Int8Array|",
    "Int16Array|", "Int32Array|", "Uint8Array|", "Uint8ClampedArray|", "Uint32Array|",
    "Date|", "Error|", "Map|", "RegExp|", "Symbol|", "WeakMap|", "WeakSet|", "Set|", NULL
};

char* PHP_HL_keywords[] = {
    "__halt_compiler", "break", "clone", "die", "empty", "endswitch", "final", "global",
    "include_once", "list", "private", "return", "try", "xor", "abstract", "callable",
    "const", "do", "enddeclare", "endwhile", "finally", "goto", "instanceof", "namespace",
    "protected", "static", "unset", "yield", "and", "case", "continue", "echo", "endfor",
    "eval", "for", "if", "insteadof", "new", "public", "switch", "use", "array", "catch",
    "declare", "else", "endforeach", "exit", "foreach", "implements", "interface", "or",
    "require", "throw", "var", "as", "class", "default", "elseif", "endif", "extends",
    "function", "include", "isset", "print", "require_once", "trait", "while", NULL
};

char* JSON_HL_keywords[] = {
    NULL
};

char* XML_HL_keywords[] = {
    NULL
};

char* SQL_HL_keywords[] = {
    "SELECT", "FROM", "DROP", "CREATE", "TABLE", "DEFAULT", "FOREIGN", "UPDATE", "LOCK",
    "INSERT", "INTO", "VALUES", "LOCK", "UNLOCK", "WHERE", "DINSTINCT", "BETWEEN", "NOT",
    "NULL", "TO", "ON", "ORDER", "GROUP", "IF", "BY", "HAVING", "USING", "UNION", "UNIQUE",
    "AUTO_INCREMENT", "LIKE", "WITH", "INNER", "OUTER", "JOIN", "COLUMN", "DATABASE", "EXISTS",
    "NATURAL", "LIMIT", "UNSIGNED", "MAX", "MIN", "PRECISION", "ALTER", "DELETE", "CASCADE",
    "PRIMARY", "KEY", "CONSTRAINT", "ENGINE", "CHARSET", "REFERENCES", "WRITE",

    "BIT|", "TINYINT|", "BOOL|", "BOOLEAN|", "SMALLINT|", "MEDIUMINT|", "INT|", "INTEGER|",
    "BIGINT|", "DOUBLE|", "DECIMAL|", "DEC|" "FLOAT|", "DATE|", "DATETIME|", "TIMESTAMP|",
    "TIME|", "YEAR|", "CHAR|", "VARCHAR|", "TEXT|", "ENUM|", "SET|", "BLOB|", "VARBINARY|",
    "TINYBLOB|", "TINYTEXT|", "MEDIUMBLOB|", "MEDIUMTEXT|", "LONGTEXT|",

    "select", "from", "drop", "create", "table", "default", "foreign", "update", "lock",
    "insert", "into", "values", "lock", "unlock", "where", "dinstinct", "between", "not",
    "null", "to", "on", "order", "group", "if", "by", "having", "using", "union", "unique",
    "auto_increment", "like", "with", "inner", "outer", "join", "column", "database", "exists",
    "natural", "limit", "unsigned", "max", "min", "precision", "alter", "delete", "cascade",
    "primary", "key", "constraint", "engine", "charset", "references", "write",

    "bit|", "tinyint|", "bool|", "boolean|", "smallint|", "mediumint|", "int|", "integer|",
    "bigint|", "double|", "decimal|", "dec|" "float|", "date|", "datetime|", "timestamp|",
    "time|", "year|", "char|", "varchar|", "text|", "enum|", "set|", "blob|", "varbinary|",
    "tinyblob|", "tinytext|", "mediumblob|", "mediumtext|", "longtext|", NULL
};

char* RUBY_HL_keywords[] = {
    "__ENCODING__", "__LINE__", "__FILE__", "BEGIN", "END", "alias", "and", "begin", "break",
    "case", "class", "def", "defined?", "do", "else", "elsif", "end", "ensure", "for", "if",
    "in", "module", "next", "not", "or", "redo", "rescue", "retry", "return", "self", "super",
    "then", "undef", "unless", "until", "when", "while", "yield", NULL
};

char* GO_HL_keywords[] = {
	"break", "case", "chan", "const", "continue", "default", "defer", "else", "fallthrough", "for",
	"func", "go", "goto", "if", "import", "interface", "map", "package", "range", "return", "select",
	"struct", "switch", "type", "var"
};

char* MSHELL_HL_keywords[] = {
    "case", "do", "done", "elif", "else", "esac", "fi", "for", "function", "if",
    "in", "select", "then", "time", "until", "while", "alias", "bg", "bind", "break",
    "builtin", "cd", "command", "continue", "declare", "dirs", "disown", "echo",
    "enable", "eval", "exec", "exit", "export", "fc", "fg", "getopts", "hash", "help",
    "history", "jobs", "kill", "let", "local", "logout", "popd", "pushd", "pwd", "read",
    "readonly", "return", "set", "shift", "suspend", "test", "times", "trap", "type",
    "typeset", "ulimit", "umask", "unalias", "unset", "wait", "printf", NULL
};



struct editor_syntax HL_DB[] = {
    {
        "c",
        C_HL_extensions,
        C_HL_keywords,
        "//",
        "/*",
        "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
    {
        "java",
        JAVA_HL_extensions,
        JAVA_HL_keywords,
        "//",
        "/*",
        "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
    {
        "python",
        PYTHON_HL_extensions,
        PYTHON_HL_keywords,
        "#",
        "'''",
        "'''",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
    {
        "bash",
        BASH_HL_extensions,
        BASH_HL_keywords,
        "#",
        NULL,
        NULL,
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
    {
        "js",
        JS_HL_extensions,
        JS_HL_keywords,
        "//",
        "/*",
        "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
    {
        "php",
        PHP_HL_extensions,
        PHP_HL_keywords,
        "//",
        "/*",
        "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
    {
        "json",
        JSON_HL_extensions,
        JSON_HL_keywords,
        NULL,
        NULL,
        NULL,
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
    {
        "xml",
        XML_HL_extensions,
        XML_HL_keywords,
        NULL,
        NULL,
        NULL,
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
    {
        "sql",
        SQL_HL_extensions,
        SQL_HL_keywords,
        "--",
        "/*",
        "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
    {
        "ruby",
        RUBY_HL_extensions,
        RUBY_HL_keywords,
        "#",
        "=begin",
        "=end",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
	{
		"go",
		GO_HL_extensions,
		GO_HL_keywords,
		"//",
        "/*",
        "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
	},
	{
		"mshell",
        MSHELL_HL_extensions,
        MSHELL_HL_keywords,
        "#",
        NULL,
        NULL,
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    }
};

// Size of the "Hightlight Database" (HL_DB).
#define HL_DB_ENTRIES (sizeof(HL_DB) / sizeof(HL_DB[0]))

/*** Declarations section ***/

void editorClearScreen();

void editorRefreshScreen();

void editorSetStatusMessage(const char* msg, ...);

void consoleBufferOpen();

void abufFree();

void abufAppend();

char *editorPrompt(char* prompt, void (*callback)(char*, int));

void editorRowAppendString(editor_row* row, char* s, size_t len);

void editorInsertNewline();

void editorDisplayHelpPage();

void editorReplace();

// Add this to the declarations section where other function prototypes are declared
void editorInsertRow(int at, const char* s, size_t len);

/*** Terminal section ***/

void die(const char* s) {
    editorClearScreen();
    // perror looks for global errno variable and then prints
    // a descriptive error mesage for it.
    perror(s);
    printf("\r\n");
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &ec.orig_termios) == -1)
        die("Failed to disable raw mode");
}

void enableRawMode() {
    // Save original terminal state into orig_termios.
    if (tcgetattr(STDIN_FILENO, &ec.orig_termios) == -1)
        die("Failed to get current terminal state");
    // At exit, restore the original state.
    atexit(disableRawMode);

    // Modify the original state to enter in raw mode.
    struct termios raw = ec.orig_termios;
    // This disables Ctrl-M, Ctrl-S and Ctrl-Q commands.
    // (BRKINT, INPCK and ISTRIP are not estrictly mandatory,
    // but it is recommended to turn them off in case any
    // system needs it).
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    // Turning off all output processing (\r\n).
    raw.c_oflag &= ~(OPOST);
    // Setting character size to 8 bits per byte (it should be
    // like that on most systems, but whatever).
    raw.c_cflag |= (CS8);
    // Using NOT operator on ECHO | ICANON | IEXTEN | ISIG and
    // then bitwise-AND them with flags field in order to
    // force c_lflag 4th bit to become 0. This disables
    // chars being printed (ECHO) and let us turn off
    // canonical mode in order to read input byte-by-byte
    // instead of line-by-line (ICANON), ISIG disables
    // Ctrl-C command and IEXTEN the Ctrl-V one.
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    // read() function now returns as soon as there is any
    // input to be read.
    raw.c_cc[VMIN] = 0;
    // Forcing read() function to return every 1/10 of a
    // second if there is nothing to read.
    raw.c_cc[VTIME] = 1;

    consoleBufferOpen();

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("Failed to set raw mode");
}

int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        // Ignoring EAGAIN to make it work on Cygwin.
        if (nread == -1 && errno != EAGAIN)
            die("Error reading input");
    }

    // Check escape sequences, if first byte
    // is an escape character then...
    if (c == '\x1b') {
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1 ||
            read(STDIN_FILENO, &seq[1], 1) != 1)
            return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1)
                    return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        // Home and End keys may be sent in many ways depending on the OS
                        // \x1b[1~, \x1b[7~, \x1b[4~, \x1b[8~
                        case '1':
                        case '7':
                            return HOME_KEY;
                        case '4':
                        case '8':
                            return END_KEY;
                        // Del key is sent as \x1b[3~
                        case '3':
                            return DEL_KEY;
                        // Page Up and Page Down send '\x1b', '[', '5' or '6' and '~'.
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                    }
                }
            } else {
                switch (seq[1]) {
                    // Arrow keys send multiple bytes starting with '\x1b', '[''
                    // and followed by an 'A', 'B', 'C' or 'D' depending on which
                    // arrow is pressed.
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    // Home key can also be sent as \x1b[H
                    case 'H': return HOME_KEY;
                    // End key can also be sent as \x1b[F
                    case 'F': return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                // Yes, Home key can ALSO be sent as \x1bOH
                case 'H': return HOME_KEY;
                // And... End key as \x1bOF
                case 'F': return END_KEY;
            }
        }
        return '\x1b';
    } else {
        return c;
    }
}

int checkFilePermissions(const char* filename) {
    // First check if file exists
    if (access(filename, F_OK) == 0) {
        // File exists, check if we can write to it
        return access(filename, W_OK);
    }
    
    // File doesn't exist, check if we can write to the directory
    char dirname[PATH_MAX];
    strncpy(dirname, filename, PATH_MAX);
    char* last_slash = strrchr(dirname, '/');
    if (last_slash) {
        *last_slash = '\0';
    } else {
        strcpy(dirname, ".");
    }
    
    return access(dirname, W_OK);
}

int getWindowSize(int* screen_rows, int* screen_cols) {
    struct winsize ws;

    // Getting window size thanks to ioctl into the given
    // winsize struct.
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    } else {
        *screen_cols = ws.ws_col;
        *screen_rows = ws.ws_row;
        return 0;
    }
}

int createBackupFile(const char* filename) {
    if (!filename) return 0;
    
    // Create backup filename
    char backup_name[PATH_MAX];
    snprintf(backup_name, sizeof(backup_name), "%s.bak", filename);
    
    // Try to copy the file
    FILE* source = fopen(filename, "rb");
    if (!source) return 0;  // Source file doesn't exist or can't be opened
    
    FILE* backup = fopen(backup_name, "wb");
    if (!backup) {
        fclose(source);
        return 0;
    }
    
    int success = 1;
    char buf[4096];
    size_t bytes;
    
    // Copy the file
    while ((bytes = fread(buf, 1, sizeof(buf), source)) > 0) {
        if (fwrite(buf, 1, bytes, backup) != bytes) {
            success = 0;
            break;
        }
    }
    
    fclose(source);
    fclose(backup);
    
    return success;
}

void editorUpdateWindowSize() {
    if (getWindowSize(&ec.screen_rows, &ec.screen_cols) == -1)
        die("Failed to get window size");
    ec.screen_rows -= 2; // Room for the status bar.
}


// Add this function to handle reading from stdin
void editorOpenFromStdin() {
    char* line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    
    // Read line by line from stdin
    while ((linelen = getline(&line, &linecap, stdin)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
            linelen--;
        editorInsertRow(ec.num_rows, line, linelen);
    }
    
    free(line);
    ec.dirty = 0;  // Consider stdin content as "saved"
    ec.file_name = NULL;  // No filename for stdin content
}


void editorHandleSigwinch() {
    editorUpdateWindowSize();
    if (ec.cursor_y > ec.screen_rows)
        ec.cursor_y = ec.screen_rows - 1;
    if (ec.cursor_x > ec.screen_cols)
        ec.cursor_x = ec.screen_cols - 1;
    editorRefreshScreen();
}

void editorRefreshScreen() {
    editorScroll();

    struct a_buf ab = ABUF_INIT;

    abufAppend(&ab, "\x1b[?25l", 6);  // Hide cursor
    abufAppend(&ab, "\x1b[H", 3);     // Reset cursor position

    editorDrawRows(&ab);
    
    // Status bar (inverted colors)
    abufAppend(&ab, "\x1b[7m", 4);
    
    // Calculate file info (left side)
    char left_status[80];
    int left_len = snprintf(left_status, sizeof(left_status), " %.20s - %d lines %s",
        ec.file_name ? ec.file_name : "[No Name]", 
        ec.num_rows,
        ec.dirty ? "(modified)" : "");
    if (left_len > ec.screen_cols) left_len = ec.screen_cols;

    // Calculate cursor info (right side)
    char right_status[80];
    int right_len = snprintf(right_status, sizeof(right_status), "Line %d/%d Col %d ",
        ec.cursor_y + 1, ec.num_rows, ec.cursor_x + 1);

    // Write left status
    abufAppend(&ab, left_status, left_len);

    // Fill middle with spaces
    int padding = ec.screen_cols - left_len - right_len;
    while (padding-- > 0) {
        abufAppend(&ab, " ", 1);
    }

    // Write right status
    abufAppend(&ab, right_status, right_len);
    
    // Reset formatting
    abufAppend(&ab, "\x1b[m", 3);
    abufAppend(&ab, "\r\n", 2);

    // Message bar
    abufAppend(&ab, "\x1b[K", 3);  // Clear the message bar line
    int msglen = strlen(ec.status_msg);
    if (msglen > ec.screen_cols) msglen = ec.screen_cols;
    if (msglen && time(NULL) - ec.status_msg_time < 5) {
        abufAppend(&ab, ec.status_msg, msglen);
    }

    // Position cursor
    char buf[32];
    int cursor_screen_x = (ec.render_x - ec.col_offset);
    if (ec.show_line_numbers) {
        cursor_screen_x += 8;
    }
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", 
             (ec.cursor_y - ec.row_offset) + 1,
             cursor_screen_x + 1);
    abufAppend(&ab, buf, strlen(buf));

    abufAppend(&ab, "\x1b[?25h", 6);  // Show cursor

    write(STDOUT_FILENO, ab.buf, ab.len);
    abufFree(&ab);
}


void editorHandleSigcont() {
    disableRawMode();
    consoleBufferOpen();
    enableRawMode();
    editorRefreshScreen();
}

void consoleBufferOpen() {
    // Switch to another terminal buffer in order to be able to restore state at exit
    // by calling consoleBufferClose().
    if (write(STDOUT_FILENO, "\x1b[?47h", 6) == -1)
        die("Error changing terminal buffer");
}

void consoleBufferClose() {
    // Restore console to the state mel opened.
    if (write(STDOUT_FILENO, "\x1b[?9l", 5) == -1 ||
        write(STDOUT_FILENO, "\x1b[?47l", 6) == -1)
        die("Error restoring buffer state");

    /*struct a_buf ab = {.buf = NULL, .len = 0};
    char* buf = NULL;
    if (asprintf(&buf, "\x1b[%d;%dH\r\n", ec.screen_rows + 1, 1) == -1)
        die("Error restoring buffer state");
    abufAppend(&ab, buf, strlen(buf));
    free(buf);

    if (write(STDOUT_FILENO, ab.buf, ab.len) == -1)
        die("Error restoring buffer state");
    abufFree(&ab);*/

    editorClearScreen();
}

/*** Syntax highlighting ***/

int isSeparator(int c) {
    // strchr() looks to see if any one of the characters in the first string
    // appear in the second string. If so, it returns a pointer to the
    // character in the second string that matched. Otherwise, it
    // returns NULL.
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[]:;", c) != NULL;
}

int isAlsoNumber(int c) {
    return c == '.' || c == 'x' || c == 'a' || c == 'b' || c == 'c' || c == 'd' || c == 'e' || c == 'f';
}

void editorUpdateSyntax(editor_row* row) {
    if (!row || row->render_size <= 0) {
        if (row && row->highlight) {
            free(row->highlight);
            row->highlight = NULL;
        }
        return;
    }

    row->highlight = realloc(row->highlight, row->render_size > 0 ? row->render_size : 1);
    if (!row->highlight) {
        exit(EXIT_FAILURE);
    }
    memset(row->highlight, HL_NORMAL, row->render_size);

    if (!ec.syntax) return;

    char** keywords = ec.syntax->keywords;
    char* scs = ec.syntax->singleline_comment_start;
    char* mcs = ec.syntax->multiline_comment_start;
    char* mce = ec.syntax->multiline_comment_end;

    int scs_len = scs ? strlen(scs) : 0;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;

    int prev_sep = 1;
    int in_string = 0;
    int in_comment = (row->idx > 0 && ec.row[row->idx - 1].hl_open_comment);

    int i = 0;
    while (i < row->render_size) {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->highlight[i - 1] : HL_NORMAL;

        if (scs_len && !in_string && !in_comment) {
            if (!strncmp(&row->render[i], scs, scs_len)) {
                memset(&row->highlight[i], HL_SL_COMMENT, row->render_size - i);
                break;
            }
        }

        if (mcs_len && mce_len && !in_string) {
            if (in_comment) {
                row->highlight[i] = HL_ML_COMMENT;
                if (!strncmp(&row->render[i], mce, mce_len)) {
                    memset(&row->highlight[i], HL_ML_COMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                } else {
                    i++;
                    continue;
                }
            } else if (!strncmp(&row->render[i], mcs, mcs_len)) {
                memset(&row->highlight[i], HL_ML_COMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }

        if (ec.syntax->flags & HL_HIGHLIGHT_STRINGS) {
            if (in_string) {
                row->highlight[i] = HL_STRING;
                if (c == '\\' && i + 1 < row->render_size) {
                    row->highlight[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }
                if (c == in_string) in_string = 0;
                i++;
                prev_sep = 1;
                continue;
            } else {
                if (c == '"' || c == '\'') {
                    in_string = c;
                    row->highlight[i] = HL_STRING;
                    i++;
                    continue;
                }
            }
        }

        if (ec.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
            if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
                (c == '.' && prev_hl == HL_NUMBER)) {
                row->highlight[i] = HL_NUMBER;
                i++;
                prev_sep = 0;
                continue;
            }
        }

        if (prev_sep) {
            for (int j = 0; keywords[j]; j++) {
                int klen = strlen(keywords[j]);
                int kw_flag = keywords[j][klen - 1] == '|';
                if (kw_flag) klen--;

                if (!strncmp(&row->render[i], keywords[j], klen) &&
                    isSeparator(row->render[i + klen])) {
                    memset(&row->highlight[i], kw_flag ? HL_KEYWORD_2 : HL_KEYWORD_1, klen);
                    i += klen;
                    prev_sep = 0;
                    break;
                }
            }
        }

        prev_sep = isSeparator(c);
        i++;
    }

    int changed = (row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment;

    if (changed && row->idx + 1 < ec.num_rows) {
        editorUpdateSyntax(&ec.row[row->idx + 1]);
    }
}



int editorSyntaxToColor(int highlight) {
    // We return ANSI codes for colors.
    // See https://en.wikipedia.org/wiki/ANSI_escape_code#Colors
    // for a list of them.
    switch (highlight) {
        case HL_SL_COMMENT:
        case HL_ML_COMMENT: return 36;
        case HL_KEYWORD_1: return 31;
        case HL_KEYWORD_2: return 32;
        case HL_STRING: return 33;
        case HL_NUMBER: return 35;
        case HL_MATCH: return 34;
        default: return 37;
    }
}

void editorApplySyntaxHighlight() {
    if (ec.syntax == NULL)
        return;

    int file_row;
    for (file_row = 0; file_row < ec.num_rows; file_row++) {
        editorUpdateSyntax(&ec.row[file_row]);
    }
}

void editorSelectSyntaxHighlight() {
    ec.syntax = NULL; // Reset syntax

    if (!ec.file_name) return;

    char* ext = strrchr(ec.file_name, '.'); // Extract file extension
    if (!ext) return;

    // Iterate through all known syntax definitions
    for (unsigned int i = 0; i < HL_DB_ENTRIES; i++) {
        struct editor_syntax* s = &HL_DB[i];

        // Match file extension with syntax database
        for (int j = 0; s->file_match[j]; j++) {
            int is_ext = (s->file_match[j][0] == '.');
            if ((is_ext && ext && !strcmp(ext, s->file_match[j])) ||
                (!is_ext && strstr(ec.file_name, s->file_match[j]))) {
                ec.syntax = s;

                // Apply syntax highlighting to all rows
                for (int row = 0; row < ec.num_rows; row++) {
                    editorUpdateSyntax(&ec.row[row]);
                }

                return; // Exit after setting the syntax
            }
        }
    }
}





/*** Row operations ***/

int editorRowCursorXToRenderX(editor_row* row, int cursor_x) {
    int render_x = 0;
    for (int j = 0; j < cursor_x && j < row->size; j++) {
        if (row->chars[j] == '\t')
            render_x += (MEL_TAB_STOP - 1) - (render_x % MEL_TAB_STOP);
        render_x++;
    }
    return render_x;
}


int editorRowRenderXToCursorX(editor_row* row, int render_x) {
    int cur_render_x = 0;
    int cursor_x;
    for (cursor_x = 0; cursor_x < row -> size; cursor_x++) {
        if (row -> chars[cursor_x] == '\t')
            cur_render_x += (MEL_TAB_STOP - 1) - (cur_render_x % MEL_TAB_STOP);
        cur_render_x++;

        if (cur_render_x > render_x)
            return cursor_x;
    }
    return cursor_x;
}

void editorUpdateRow(editor_row* row) {
    if (!row || !row->chars) return;

    // Подсчёт табуляций
    int tabs = 0;
    for (int j = 0; j < row->size; j++) {
        if (row->chars[j] == '\t') tabs++;
    }

    // Освобождение старого render буфера
    free(row->render);

    // Выделение памяти для нового render буфера
    size_t render_size = row->size + tabs * (MEL_TAB_STOP - 1) + 1;
    row->render = malloc(render_size);
    if (!row->render) {
        row->render_size = 0;
        return;
    }

    // Рендеринг содержимого
    int idx = 0;
    for (int j = 0; j < row->size; j++) {
        if (row->chars[j] == '\t') {
            row->render[idx++] = ' ';
            while (idx % MEL_TAB_STOP != 0 && idx < render_size - 1) {
                row->render[idx++] = ' ';
            }
        } else if (idx < render_size - 1) {
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->render_size = idx;

    // Обновление подсветки синтаксиса
    editorUpdateSyntax(row);
}


void editorInsertRow(int at, const char* s, size_t len) {
    // Проверка валидности позиции вставки
    if (at < 0 || at > ec.num_rows) return;

    // Выделение памяти для новой строки с проверкой
    editor_row* new_rows = realloc(ec.row, sizeof(editor_row) * (ec.num_rows + 1));
    if (!new_rows) {
        editorSetStatusMessage("Failed to allocate memory for new row");
        return;
    }
    ec.row = new_rows;

    // Сдвиг существующих строк
    memmove(&ec.row[at + 1], &ec.row[at], sizeof(editor_row) * (ec.num_rows - at));
    
    // Обновление индексов для сдвинутых строк
    for (int j = at + 1; j <= ec.num_rows; j++) {
        ec.row[j].idx = j;
    }

    // Инициализация новой строки
    ec.row[at].idx = at;
    ec.row[at].size = len;
    ec.row[at].chars = malloc(len + 1);
    if (!ec.row[at].chars) {
        editorSetStatusMessage("Failed to allocate memory for row content");
        return;
    }

    // Копирование содержимого
    if (s && len > 0) {
        memcpy(ec.row[at].chars, s, len);
    }
    ec.row[at].chars[len] = '\0';

    // Инициализация render буфера
    ec.row[at].render = NULL;
    ec.row[at].render_size = 0;
    ec.row[at].highlight = NULL;
    ec.row[at].hl_open_comment = 0;

    // Обновление строки с проверками
    editorUpdateRow(&ec.row[at]);
    if (!ec.row[at].render) {
        // Если не удалось создать render буфер, очищаем строку
        free(ec.row[at].chars);
        memmove(&ec.row[at], &ec.row[at + 1], sizeof(editor_row) * (ec.num_rows - at));
        return;
    }

    ec.num_rows++;
    ec.dirty++;
}





void editorFreeRow(editor_row* row) {
    free(row -> render);
    free(row -> chars);
    free(row -> highlight);
}

void editorDelRow(int at) {
    if (at < 0 || at >= ec.num_rows)
        return;
    editorFreeRow(&ec.row[at]);
    memmove(&ec.row[at], &ec.row[at + 1], sizeof(editor_row) * (ec.num_rows - at - 1));

    for (int j = at; j < ec.num_rows - 1; j++) {
        ec.row[j].idx--;
    }

    ec.num_rows--;
    ec.dirty++;
}

// -1 down, 1 up
void editorFlipRow(int dir) {
    editor_row c_row = ec.row[ec.cursor_y];
    ec.row[ec.cursor_y] = ec.row[ec.cursor_y - dir];
    ec.row[ec.cursor_y - dir] = c_row;

    ec.row[ec.cursor_y].idx += dir;
    ec.row[ec.cursor_y - dir].idx -= dir;

    int first = (dir == 1) ? ec.cursor_y - 1 : ec.cursor_y;
    editorUpdateSyntax(&ec.row[first]);
    editorUpdateSyntax(&ec.row[first] + 1);
    if (ec.num_rows - ec.cursor_y > 2)
      editorUpdateSyntax(&ec.row[first] + 2);

    ec.cursor_y -= dir;
    ec.dirty++;
}

void editorCopy(bool printStatus) {
    ec.copied_char_buffer = realloc(ec.copied_char_buffer, strlen(ec.row[ec.cursor_y].chars) + 1);
    strcpy(ec.copied_char_buffer, ec.row[ec.cursor_y].chars);
    if(printStatus) editorSetStatusMessage("Content copied");
}

void editorCut() {
    editorDelRow(ec.cursor_y);
    if (ec.num_rows - ec.cursor_y > 0)
        editorUpdateSyntax(&ec.row[ec.cursor_y]);
    if (ec.num_rows - ec.cursor_y > 1)
        editorUpdateSyntax(&ec.row[ec.cursor_y + 1]);
    ec.cursor_x = ec.cursor_y == ec.num_rows ? 0 : ec.row[ec.cursor_y].size;
    editorSetStatusMessage("Content cut");
}

void editorPaste() {
    if (ec.copied_char_buffer == NULL)
      return;

    if (ec.cursor_y == ec.num_rows)
      editorInsertRow(ec.cursor_y, ec.copied_char_buffer, strlen(ec.copied_char_buffer));
    else
      editorRowAppendString(&ec.row[ec.cursor_y], ec.copied_char_buffer, strlen(ec.copied_char_buffer));
    ec.cursor_x += strlen(ec.copied_char_buffer);
}

void editorRowInsertChar(editor_row* row, int at, int c) {
    if (at < 0 || at > row->size) {
        return;
    }

    row->chars = realloc(row->chars, row->size + 2);
    if (!row->chars) {
        perror("Failed to allocate memory for chars");
        exit(EXIT_FAILURE);
    }

    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->chars[at] = c;
    row->size++;
    row->chars[row->size] = '\0';

    editorUpdateRow(row);
}

void editorInsertNewline() {
    if (ec.cursor_x == 0) {
        editorInsertRow(ec.cursor_y, "", 0);
    } else {
        editor_row* row = &ec.row[ec.cursor_y];
        if (!row || !row->chars) return;

        // Создание новой строки с оставшимся содержимым
        editorInsertRow(ec.cursor_y + 1, &row->chars[ec.cursor_x], row->size - ec.cursor_x);
        if (ec.cursor_y + 1 < ec.num_rows) {
            row = &ec.row[ec.cursor_y];  // Обновляем указатель после вставки
            row->size = ec.cursor_x;
            row->chars[row->size] = '\0';
            editorUpdateRow(row);
        }
    }
    ec.cursor_y++;
    ec.cursor_x = 0;
}

void editorGoToLine() {
    char* line_str = editorPrompt("Go to line: %s", NULL);
    if (!line_str) {
        editorSetStatusMessage("Go to line canceled");
        return;
    }

    // Convert input to line number
    int line_number = atoi(line_str);
    free(line_str);

    // Validate line number
    if (line_number < 1 || line_number > ec.num_rows) {
        editorSetStatusMessage("Invalid line number");
        return;
    }

    // Adjust cursor position (lines are 0-based internally)
    ec.cursor_y = line_number - 1;
    ec.cursor_x = 0;

    // Ensure cursor is within visible area
    if (ec.cursor_y < ec.row_offset) {
        ec.row_offset = ec.cursor_y;
    } else if (ec.cursor_y >= ec.row_offset + ec.screen_rows) {
        ec.row_offset = ec.cursor_y - ec.screen_rows + 1;
    }

    // Reset render position
    ec.render_x = 0;
    if (ec.cursor_y < ec.num_rows) {
        ec.render_x = editorRowCursorXToRenderX(&ec.row[ec.cursor_y], ec.cursor_x);
    }

    // Reset column offset
    ec.col_offset = 0;

    editorSetStatusMessage("Moved to line %d", line_number);
    editorRefreshScreen();
}


void editorRowAppendString(editor_row* row, char* s, size_t len) {
    if (!row || !s) return;

    // Выделение памяти для расширенной строки
    char* new_chars = realloc(row->chars, row->size + len + 1);
    if (!new_chars) {
        editorSetStatusMessage("Failed to allocate memory for append");
        return;
    }
    row->chars = new_chars;

    // Копирование новой строки
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';

    editorUpdateRow(row);
    ec.dirty++;
}

void editorRowDelChar(editor_row* row, int at) {
    if (at < 0 || at >= row -> size)
        return;
    // Overwriting the deleted character with the characters that come
    // after it.
    memmove(&row -> chars[at], &row -> chars[at + 1], row -> size - at);
    row -> size--;
    editorUpdateRow(row);
    ec.dirty++;
}

void editorRowDelString(editor_row* row, int at, int len) {
    if (at < 0 || (at + len - 1) >= row -> size)
        return;
    // Overwriting the deleted string with the characters that come
    // after it.
    memmove(&row -> chars[at], &row -> chars[at + len], row -> size - (at + len) + 1);
    row -> size -= len;
    editorUpdateRow(row);
    ec.dirty += len;
}

void editorRowInsertString(editor_row* row, int at, char* str) {
    int len = strlen(str);
    if (at < 0 || at > row -> size)
        return;
    row->chars = realloc(row->chars, row->size + strlen(str) + 2);
    // Move 'after-at' part of string content to the end.
    memmove(&row -> chars[at + len], &row -> chars[at], row -> size - at);
    // Copy contents of str into the created space.
    memcpy(&row -> chars[at], str, strlen(str));
    row -> size += len;
    editorUpdateRow(row);
    ec.dirty += len;
}

/*** Editor operations ***/

void editorInsertChar(int c) {
    if (c <= 0) return;  // Проверка валидности символа

    // Создание новой строки если курсор на тильде
    if (ec.cursor_y == ec.num_rows) {
        editorInsertRow(ec.num_rows, "", 0);
        if (ec.cursor_y != ec.num_rows - 1) return;  // Проверка успешности вставки
    }

    editor_row* row = &ec.row[ec.cursor_y];
    if (!row || !row->chars) return;

    // Выделение памяти для нового символа
    char* new_chars = realloc(row->chars, row->size + 2);
    if (!new_chars) {
        editorSetStatusMessage("Failed to allocate memory for character");
        return;
    }
    row->chars = new_chars;

    // Вставка символа
    memmove(&row->chars[ec.cursor_x + 1], &row->chars[ec.cursor_x], row->size - ec.cursor_x);
    row->size++;
    row->chars[ec.cursor_x] = c;
    row->chars[row->size] = '\0';

    // Обновление строки
    editorUpdateRow(row);
    ec.cursor_x++;
    ec.dirty++;
}


void editorDelChar() {
    // If the cursor is past the end of the file, there's nothing to delete.
    if (ec.cursor_y == ec.num_rows)
        return;
    // Cursor is at the beginning of a file, there's nothing to delete.
    if (ec.cursor_x == 0 && ec.cursor_y == 0)
        return;

    editor_row* row = &ec.row[ec.cursor_y];
    if (ec.cursor_x > 0) {
        editorRowDelChar(row, ec.cursor_x - 1);
        ec.cursor_x--;
    // Deleting a line and moving up all the content.
    } else {
        ec.cursor_x = ec.row[ec.cursor_y - 1].size;
        editorRowAppendString(&ec.row[ec.cursor_y -1], row -> chars, row -> size);
        editorDelRow(ec.cursor_y);
        ec.cursor_y--;
    }
}

/*** File I/O ***/

char* editorRowsToString(int* buf_len) {
    int total_len = 0;
    int j;
    // Adding up the lengths of each row of text, adding 1
    // to each one for the newline character we'll add to
    // the end of each line.
    for (j = 0; j < ec.num_rows; j++) {
        total_len += ec.row[j].size + 1;
    }
    *buf_len = total_len;

    char* buf = malloc(total_len);
    char* p = buf;
    // Copying the contents of each row to the end of the
    // buffer, appending a newline character after each
    // row.
    for (j = 0; j < ec.num_rows; j++) {
        memcpy(p, ec.row[j].chars, ec.row[j].size);
        p += ec.row[j].size;
        *p = '\n';
        p++;
    }

    return buf;
}

static int fileExists(const char* file_name) {
    struct stat s = {0};
    return stat(file_name, &s) == 0;
}

void editorOpen(char* file_name) {
    if (file_name) {
        free(ec.file_name);
        ec.file_name = strdup(file_name);

        FILE* fp = fopen(file_name, "r");
        if (!fp) {
            perror("fopen");
            exit(1);
        }

        char* line = NULL;
        size_t linecap = 0;
        ssize_t linelen;
        while ((linelen = getline(&line, &linecap, fp)) != -1) {
            while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
                linelen--;
            editorInsertRow(ec.num_rows, line, linelen);
        }
        free(line);
        fclose(fp);

        editorSelectSyntaxHighlight();
    } else {
        ec.file_name = NULL;  // No file name, starting fresh
    }

    ec.dirty = 0;
}


void editorSave() {
    if (ec.file_name == NULL) {
        char* new_name = editorPrompt("Save as: %s (ESC to cancel)", NULL);
        if (new_name == NULL) {
            editorSetStatusMessage("Save aborted");
            return;
        }
        ec.file_name = new_name;
        editorSelectSyntaxHighlight();
    }

    // Create backup if requested and file exists
    if (ec.create_backup && access(ec.file_name, F_OK) == 0) {
        if (!createBackupFile(ec.file_name)) {
            editorSetStatusMessage("Warning: Failed to create backup file");
        }
    }

    int len;
    char* buf = editorRowsToString(&len);
    if (!buf) {
        editorSetStatusMessage("Failed to prepare content for saving");
        return;
    }

    FILE* fp = fopen(ec.file_name, "w");
    if (!fp) {
        free(buf);
        editorSetStatusMessage("Can't save file. Error occurred: %s", strerror(errno));
        return;
    }

    size_t written = fwrite(buf, sizeof(char), len, fp);
    int save_errno = errno;
    
    if (fclose(fp) < 0) {
        free(buf);
        editorSetStatusMessage("Error closing file: %s", strerror(errno));
        return;
    }

    free(buf);

    if (written == (size_t)len) {
        ec.dirty = 0;
        editorSetStatusMessage("%d bytes written to disk", len);
    } else {
        editorSetStatusMessage("Can't save file. Error occurred: %s", strerror(save_errno));
    }
}

/*** Search section ***/

void editorReplace() {
    char* search_pattern = editorPrompt("Search pattern: %s (ESC to cancel)", NULL);
    if (!search_pattern) {
        editorSetStatusMessage("Replace canceled");
        return;
    }

    char* replace_pattern = editorPrompt("Replace with: %s (ESC to cancel)", NULL);
    if (!replace_pattern) {
        free(search_pattern);
        editorSetStatusMessage("Replace canceled");
        return;
    }

    int replacements = 0;
    for (int i = 0; i < ec.num_rows; i++) {
        editor_row* row = &ec.row[i];
        char* match = strstr(row->chars, search_pattern);
        
        while (match) {
            int pos = match - row->chars;
            editorRowDelString(row, pos, strlen(search_pattern));
            editorRowInsertString(row, pos, replace_pattern);
            replacements++;
            
            match = strstr(row->chars + pos + strlen(replace_pattern), search_pattern);
        }
    }

    free(search_pattern);
    free(replace_pattern);
    
    editorSetStatusMessage("Replaced %d occurrences", replacements);
}

void editorSearchCallback(char* query, int key) {
   static int last_match = -1;
   static int direction = 1;
   static int saved_cursor_x = -1;
   static int saved_cursor_y = -1;
   static int saved_col_offset = -1;
   static int saved_row_offset = -1;
   static char* saved_query = NULL;

   if (last_match == -1 && saved_cursor_x == -1) {
       saved_cursor_x = ec.cursor_x;
       saved_cursor_y = ec.cursor_y;
       saved_col_offset = ec.col_offset;
       saved_row_offset = ec.row_offset;
       if (query) {
           free(saved_query);
           saved_query = strdup(query);
       }
   }

   if (key == '\x1b' || key == '\r') {
       if (key == '\x1b' && last_match != -1) {
           return;
       }
       last_match = -1;
       direction = 1;
       if (saved_query) {
           free(saved_query);
           saved_query = NULL;
       }
       return;
   }

   if (key == CTRL_KEY('n') || key == CTRL_KEY('r')) {
       direction = (key == CTRL_KEY('n')) ? 1 : -1;
       query = saved_query;
   } else if (query) {
       free(saved_query);
       saved_query = strdup(query);
   }

   if (query) {
       int current = last_match;
       
       for (int i = 0; i < ec.num_rows; i++) {
           current += direction;
           if (current == -1) current = ec.num_rows - 1;
           else if (current == ec.num_rows) current = 0;

           editor_row* row = &ec.row[current];
           char* match = strstr(row->render, query);
           
           if (match) {
               last_match = current;
               ec.cursor_y = current;
               ec.cursor_x = editorRowRenderXToCursorX(row, match - row->render);
               
               if (current < ec.row_offset) {
                   ec.row_offset = current;
               } else if (current >= ec.row_offset + ec.screen_rows) {
                   ec.row_offset = current - ec.screen_rows + 1;
               }
               
               int rx = editorRowCursorXToRenderX(row, ec.cursor_x);
               if (rx < ec.col_offset) {
                   ec.col_offset = rx;
               } else if (rx >= ec.col_offset + ec.screen_cols) {
                   ec.col_offset = rx - ec.screen_cols + 1;
               }
               ec.render_x = rx;
               return;
           }
       }
   }
}



void editorSearch() {
    int saved_cursor_x = ec.cursor_x;
    int saved_cursor_y = ec.cursor_y;
    int saved_col_offset = ec.col_offset;
    int saved_row_offset = ec.row_offset;

    char* query = editorPrompt("Search: %s (Use ESC / Enter / Arrows)", editorSearchCallback);

    if (query) {
        free(query);
    } else {
        editorRefreshScreen();
    }
}

/*** Action section ***/



Action* createAction(char* str, ActionType t) {
    Action* newAction = malloc(sizeof(Action));
    newAction->t = t;
    newAction->cpos_x = ec.cursor_x;
    newAction->cpos_y = ec.cursor_y;
    newAction->cursor_on_tilde = (ec.cursor_y == ec.num_rows);
    newAction->string = str;
    return newAction;
}

void freeAction(Action *action) {
    if(action) {
        if(action->string) free(action->string);
        free(action);
    }
}

void execute(Action* action) {
    if(!action) return;
    switch(action->t) {
        case InsertChar:
            {
                ec.cursor_x = action->cpos_x;
                ec.cursor_y = action->cpos_y;
                if(ec.cursor_y < ec.num_rows) {
                    editorRowInsertString(&ec.row[ec.cursor_y], ec.cursor_x, action->string);
                    ec.cursor_x += strlen(action->string);
                } else {
                    editorInsertChar((int)(*action->string));
                }
            }
            break;
        case DelChar:
            {
                ec.cursor_x = action->cpos_x;
                ec.cursor_y = action->cpos_y;
                editorDelChar();
            }
            break;
        case PasteLine:
            {
                ec.cursor_x = action->cpos_x;
                ec.cursor_y = action->cpos_y;
                // store current copied char buffer
                char* curr_copy_buffer = ec.copied_char_buffer;
                // set editor copy buffer to action string
                ec.copied_char_buffer = action->string;
                editorPaste();
                // reset editor copy buffer
                ec.copied_char_buffer = curr_copy_buffer;
            }
            break;
        case CutLine:
            {
                ec.cursor_x = action->cpos_x;
                ec.cursor_y = action->cpos_y;
                editorCut();
            }
            break;
        case FlipDown:
            {
                ec.cursor_x = action->cpos_x;
                ec.cursor_y = action->cpos_y;
                editorFlipRow(-1);
            }
            break;
        case FlipUp:
            {
                ec.cursor_x = action->cpos_x;
                ec.cursor_y = action->cpos_y;
                editorFlipRow(1);
            }
            break;
        case NewLine:
            {
                ec.cursor_x = action->cpos_x;
                ec.cursor_y = action->cpos_y;
                editorInsertNewline();
            }
            break;
        default: break;
    }
}

void revert(Action *action) {
    if(!action) return;
    switch(action->t) {
        case InsertChar:
            {
                ec.cursor_x = action->cpos_x;
                ec.cursor_y = action->cpos_y;
                editorRowDelString(&ec.row[ec.cursor_y], ec.cursor_x, strlen(action->string));
                if(action->cursor_on_tilde)
                    editorDelRow(ec.cursor_y);
            }
            break;
        case DelChar:
            {
                if(action->string) {
                    ec.cursor_x = action->cpos_x - 1;
                    ec.cursor_y = action->cpos_y;
                    int c = *(action->string);
                    editorInsertChar(c);
                } else {
                    editorInsertNewline();
                }
            }
            break;
        case PasteLine:
            {
                ec.cursor_x = action->cpos_x;
                ec.cursor_y = action->cpos_y;
                editor_row* row = &ec.row[ec.cursor_y];
                if(action->string) {
                    editorRowDelString(row, ec.cursor_x, strlen(action->string));
                    if(action->cursor_on_tilde) editorDelRow(ec.cursor_y);
                }
            }
            break;
        case CutLine:
            {
                ec.cursor_x = 0;
                ec.cursor_y = action->cpos_y;
                editorInsertRow(ec.cursor_y, "", 0);
                // store current copied char buffer
                char* curr_copy_buffer = ec.copied_char_buffer;
                // set editor copy buffer to action string
                ec.copied_char_buffer = action->string;
                editorPaste();
                // reset editor copy buffer
                ec.copied_char_buffer = curr_copy_buffer;
            }
            break;
        case FlipDown:
            {
                ec.cursor_x = action->cpos_x;
                ec.cursor_y = action->cpos_y + 1;
                editorFlipRow(1);
            }
            break;
        case FlipUp:
            {
                ec.cursor_x = action->cpos_x;
                ec.cursor_y = action->cpos_y - 1;
                editorFlipRow(-1);
            }
            break;
        case NewLine:
            {
                ec.cursor_x = 0;
                ec.cursor_y = action->cpos_y + 1;
                editorDelChar();
            }
            break;
        default: break;
    }
}


ActionList* actionListInit() {
    ActionList* list = malloc(sizeof(ActionList));
    list->head = NULL;
    list->tail = NULL;
    list->current = NULL;
    list->size = 0;
    return list;
}

// Frees AListNodes in actions list starting from AListNode ptr begin
// returns number of AListNodes freed.
int clearAlistFrom(AListNode* begin) {
    int nodes_freed = 0;
    if(begin && begin->prev )
        begin->prev->next = NULL;
    AListNode* curr_ptr = begin;
    while( curr_ptr ) {
        AListNode* temp = curr_ptr;
        curr_ptr = curr_ptr->next;
        freeAction(temp->action);
        free(temp);
        nodes_freed += 1;
    }

    return nodes_freed;
}

void freeAlist() {
    ActionList* list = ec.actions;
    if(list){
        clearAlistFrom(list->head);
        free(list);
    }
}

void addAction(Action* action) {
    if(ACTIONS_LIST_MAX_SIZE == 0) return;
    ActionList* list = ec.actions;
    AListNode* node = malloc(sizeof(AListNode));
    node->action = action;
    node->prev = NULL;
    node->next = NULL;

    if(list->head == NULL) {
        list->head = node;
        list->tail = node;
        list->current = node;
    } else if (list->tail == list->current) {
        list->tail->next = node;
        node->prev = list->tail;
        list->tail = node;
        list->current = node;
    } else {
        AListNode* clear_from = list->current == NULL ? list->head : list->current->next;
        int nodes_freed = clearAlistFrom(clear_from);
        list->size -= nodes_freed;
        if(list->current) {
            list->current->next = node;
            node->prev = list->current;
            list->tail = node;
            list->current = node;
        } else {
            list->current = list->head = list->tail = node;
        }
    }
    list->size += 1;

    // Truncate list to fit at max `ACTIONS_LIST_MAX_SIZE` actions
    if((list->size > ACTIONS_LIST_MAX_SIZE) && (ACTIONS_LIST_MAX_SIZE != -1)) {
        AListNode* tmp = list->head;
        list->head = list->head->next;
        list->size -= 1;
        if(list->size == 0)
            list->current = list->tail = NULL;
        freeAction(tmp->action);
        free(tmp);
        if(list->head)
            list->head->prev = NULL;
    }
}

// If last action is InsertChar operation and the current action is also InsertChar
// Instead of creating new action, this function concats the char to the
// stored string in last action provided the current action does append at the end of the row
bool concatWithLastAction(ActionType t, char* str) {
    if(t == InsertChar &&
       ACTIONS_LIST_MAX_SIZE &&
       ec.actions->current &&
       ec.actions->current == ec.actions->tail &&
       ec.actions->current->action->t == t &&
       ec.actions->current->action->cpos_y == ec.cursor_y &&
       (int)(ec.actions->current->action->cpos_x + strlen(ec.actions->current->action->string)) == ec.cursor_x
    ) {
        int c = *(str);
        editorInsertChar(c);
        char* string = ec.actions->current->action->string;
        string = realloc(string, strlen(string) + 2);
        strcat(string, str);
        ec.actions->current->action->string = string;
        free(str);
        return true;
    }
    return false;
}

// Creates Action, adds it to ActionList and executes it.
// Takes ActionType and char* as paramaters for use in undo/redo operation
void makeAction(ActionType t, char* str) {
    if(!concatWithLastAction(t, str)) {
        Action* newAction = createAction(str, t);
        if(ACTIONS_LIST_MAX_SIZE) addAction(newAction);
        execute(newAction);
    }
}

void undo() {
    if(ACTIONS_LIST_MAX_SIZE == 0) return;
    ActionList* list = ec.actions;
    if(list && list->current) {
        revert(list->current->action);
        // may set current to NULL
        list->current = list->current->prev;
    }
    if((list->current == NULL) && (ACTIONS_LIST_MAX_SIZE)) {
        ec.dirty = 0;
    }
}

void redo() {
    if(ACTIONS_LIST_MAX_SIZE == 0) return;
    ActionList* list = ec.actions;
    if(list && list->current && list->current->next) {
        execute(list->current->next->action);
        list->current = list->current->next;
    }
    // when current points to NULL but head is not NULL, do head
    if(list && list->head && !list->current) {
        list->current = list->head;
        execute(list->current->action);
    }
}

/*** Append buffer section **/

void abufAppend(struct a_buf* ab, const char* s, int len) {
    // Using realloc to get a block of free memory that is
    // the size of the current string + the size of the string
    // to be appended.
    char* new = realloc(ab -> buf, ab -> len + len);

    if (new == NULL)
        return;

    // Copying the string s at the end of the current data in
    // the buffer.
    memcpy(&new[ab -> len], s, len);
    ab -> buf = new;
    ab -> len += len;
}

void abufFree(struct a_buf* ab) {
    // Deallocating buffer.
    free(ab -> buf);
}

/*** Output section ***/

void editorScroll() {
    ec.render_x = 0;
    if (ec.cursor_y < ec.num_rows) {
        ec.render_x = editorRowCursorXToRenderX(&ec.row[ec.cursor_y], ec.cursor_x);
    }

    // Vertical scrolling
    if (ec.cursor_y < ec.row_offset) {
        ec.row_offset = ec.cursor_y;
    }
    if (ec.cursor_y >= ec.row_offset + ec.screen_rows) {
        ec.row_offset = ec.cursor_y - ec.screen_rows + 1;
    }

    // Horizontal scrolling
    int effective_width = ec.screen_cols - (ec.show_line_numbers ? 8 : 0);
    if (ec.render_x < ec.col_offset) {
        ec.col_offset = ec.render_x;
    }
    if (ec.render_x >= ec.col_offset + effective_width) {
        ec.col_offset = ec.render_x - effective_width + 1;
    }
}


void editorDrawStatusBar(struct a_buf* ab) {
    // First clear the entire line
    abufAppend(ab, "\x1b[K", 3);

    // Start inverted colors
    abufAppend(ab, "\x1b[7m", 4);

    // Prepare file info for left side
    char left[80];
    int left_len = snprintf(left, sizeof(left), " %.20s - %d lines%s", 
        ec.file_name ? ec.file_name : "[No Name]", 
        ec.num_rows,
        ec.dirty ? " (modified)" : "");

    // Prepare cursor info for right side
    char right[80];
    int right_len = snprintf(right, sizeof(right), "Line %d/%d Col %d ", 
        ec.cursor_y + 1, ec.num_rows, ec.cursor_x + 1);

    // Ensure we don't exceed screen width
    if (left_len > ec.screen_cols) left_len = ec.screen_cols;

    // Write left part
    abufAppend(ab, left, left_len);

    // Calculate and write spaces
    int spaces = ec.screen_cols - left_len - right_len;
    if (spaces > 0) {
        for (int i = 0; i < spaces; i++) {
            abufAppend(ab, " ", 1);
        }
        // Write right part only if there's room
        abufAppend(ab, right, right_len);
    }

    // Reset colors and move to next line
    abufAppend(ab, "\x1b[m\r\n", 5);
}

void editorDrawMessageBar(struct a_buf* ab) {
    // Clear the line first
    abufAppend(ab, "\x1b[K", 3);
    
    int msglen = strlen(ec.status_msg);
    if (msglen > ec.screen_cols) msglen = ec.screen_cols;
    
    if (msglen && time(NULL) - ec.status_msg_time < 5) {
        abufAppend(ab, ec.status_msg, msglen);
    }
}

void editorDrawWelcomeMessage(struct a_buf* ab) {
    char welcome[80];
    // Using snprintf to truncate message in case the terminal
    // is too tiny to handle the entire string.
    int welcome_len = snprintf(welcome, sizeof(welcome),
        "mel %s <https://github.com/igor101964/mel>", MEL_VERSION);
    if (welcome_len > ec.screen_cols)
        welcome_len = ec.screen_cols;
    // Centering the message.
    int padding = (ec.screen_cols - welcome_len) / 2;
    // Remember that everything != 0 is true.
    if (padding) {
        abufAppend(ab, "~", 1);
        padding--;
    }
    while (padding--)
        abufAppend(ab, " ", 1);
    abufAppend(ab, welcome, welcome_len);
}

// The ... argument makes editorSetStatusMessage() a variadic function,
// meaning it can take any number of arguments. C's way of dealing with
// these arguments is by having you call va_start() and va_end() on a
// // value of type va_list. The last argument before the ... (in this
// case, msg) must be passed to va_start(), so that the address of
// the next arguments is known. Then, between the va_start() and
// va_end() calls, you would call va_arg() and pass it the type of
// the next argument (which you usually get from the given format
// string) and it would return the value of that argument. In
// this case, we pass msg and args to vsnprintf() and it takes care
// of reading the format string and calling va_arg() to get each
// argument.
void editorSetStatusMessage(const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    vsnprintf(ec.status_msg, sizeof(ec.status_msg), msg, args);
    va_end(args);
    ec.status_msg_time = time(NULL);
}

void editorDrawRows(struct a_buf* ab) {
    for (int y = 0; y < ec.screen_rows; y++) {
        int file_row = y + ec.row_offset;
        
        // Line numbers if enabled
        if (ec.show_line_numbers) {
            char line_num[16];
            snprintf(line_num, sizeof(line_num), "%7d ", file_row + 1);
            abufAppend(ab, "\x1b[34m", 5);  // Blue color
            abufAppend(ab, line_num, strlen(line_num));
            abufAppend(ab, "\x1b[m", 3);    // Reset color
        }

        if (file_row >= ec.num_rows) {
            abufAppend(ab, "~", 1);
        } else {
            editor_row* row = &ec.row[file_row];
            int len = row->render_size - ec.col_offset;
            if (len < 0) len = 0;
            
            int max_len = ec.screen_cols - (ec.show_line_numbers ? 8 : 0);
            if (len > max_len) len = max_len;

            char* c = &row->render[ec.col_offset];
            unsigned char* hl = &row->highlight[ec.col_offset];
            int current_pos = 0;

            for (int j = 0; j < len; j++) {
                // Handle column marker if enabled
                if (ec.column_marker > 0 && 
                    (j + ec.col_offset) == ec.column_marker - 1) {
                    abufAppend(ab, "\x1b[38;5;242m|\x1b[m", 13);
                    current_pos++;
                    continue;
                }

                // Draw regular character
                if (iscntrl(c[j])) {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    abufAppend(ab, "\x1b[7m", 4);
                    abufAppend(ab, &sym, 1);
                    abufAppend(ab, "\x1b[m", 3);
                } else {
                    int color = editorSyntaxToColor(hl[j]);
                    char colbuf[16];
                    snprintf(colbuf, sizeof(colbuf), "\x1b[%dm", color);
                    abufAppend(ab, colbuf, strlen(colbuf));
                    abufAppend(ab, &c[j], 1);
                }
                current_pos++;
            }

            // Draw column marker after content if needed
            if (ec.column_marker > 0 && 
                ec.column_marker > ec.col_offset + current_pos &&
                ec.column_marker - ec.col_offset < max_len) {
                while (current_pos < ec.column_marker - ec.col_offset - 1) {
                    abufAppend(ab, " ", 1);
                    current_pos++;
                }
                abufAppend(ab, "\x1b[38;5;242m|\x1b[m", 13);
            }
        }

        abufAppend(ab, "\x1b[K", 3);  // Clear to end of line
        abufAppend(ab, "\r\n", 2);
    }
}


void editorClearScreen() {
    // Writing 4 bytes out to the terminal:
    // - (1 byte) \x1b : escape character
    // - (3 bytes) [2J : Clears the entire screen, see
    // http://vt100.net/docs/vt100-ug/chapter3.html#ED
    // for more info.
    write(STDOUT_FILENO, "\x1b[2J", 4);
    // Writing 3 bytes to reposition the cursor back at
    // the top-left corner, see
    // http://vt100.net/docs/vt100-ug/chapter3.html#CUP
    // for more info.
    write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** Input section ***/

char* editorPrompt(char* prompt, void (*callback)(char*, int)) {
    size_t buf_size = 128;
    char* buf = malloc(buf_size);

    size_t buf_len = 0;
    buf[0] = '\0';

    while (1) {
        editorSetStatusMessage(prompt, buf);
        editorRefreshScreen();

        int c = editorReadKey();
        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
            if (buf_len != 0)
                buf[--buf_len] = '\0';
        } else if (c == '\x1b') {
            editorSetStatusMessage("");
            if (callback)
                callback(buf, c);
            free(buf);
            return NULL;
        } else if (c == '\r') {
            if (buf_len != 0) {
                editorSetStatusMessage("");
                if (callback)
                    callback(buf, c);
                return buf;
            }
        } else if (!iscntrl(c) && isprint(c)) {
            if (buf_len == buf_size - 1) {
                buf_size *= 2;
                buf = realloc(buf, buf_size);
            }
            buf[buf_len++] = c;
            buf[buf_len] = '\0';
        }

        if (callback)
            callback(buf, c);
    }
}

void editorMoveCursor(int key) {
    editor_row* row = (ec.cursor_y >= ec.num_rows) ? NULL : &ec.row[ec.cursor_y];

    switch (key) {
        case ARROW_LEFT:
            if (ec.cursor_x > 0) {
                ec.cursor_x--;
            } else if (ec.cursor_y > 0) {
                ec.cursor_y--;
                ec.cursor_x = ec.row[ec.cursor_y].size;
            }
            break;
        case ARROW_RIGHT:
            if (row && ec.cursor_x < row->size) {
                ec.cursor_x++;
            } else if (row && ec.cursor_x == row->size && ec.cursor_y < ec.num_rows - 1) {
                ec.cursor_y++;
                ec.cursor_x = 0;
            }
            break;
        case ARROW_UP:
            if (ec.cursor_y > 0) {
                ec.cursor_y--;
            }
            break;
        case ARROW_DOWN:
            if (ec.cursor_y < ec.num_rows - 1) {
                ec.cursor_y++;
            }
            break;
        case HOME_KEY:
            ec.cursor_x = 0;
            ec.col_offset = 0;
            break;
        case END_KEY:
            if (row) {
                ec.cursor_x = row->size;
                if (ec.show_line_numbers) {
                    int width = ec.screen_cols - 8;  // Account for line numbers
                    if (ec.cursor_x > width) {
                        ec.col_offset = ec.cursor_x - width + 1;
                    }
                } else {
                    if (ec.cursor_x > ec.screen_cols) {
                        ec.col_offset = ec.cursor_x - ec.screen_cols + 1;
                    }
                }
            }
            break;
        case PAGE_UP:
        case PAGE_DOWN:
            {
                if (key == PAGE_UP) {
                    ec.cursor_y = ec.row_offset;
                } else if (key == PAGE_DOWN) {
                    ec.cursor_y = ec.row_offset + ec.screen_rows - 1;
                    if (ec.cursor_y > ec.num_rows) ec.cursor_y = ec.num_rows - 1;
                }

                int times = ec.screen_rows;
                while (times--)
                    editorMoveCursor(key == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;
    }

    row = (ec.cursor_y >= ec.num_rows) ? NULL : &ec.row[ec.cursor_y];
    int row_len = row ? row->size : 0;
    if (ec.cursor_x > row_len) {
        ec.cursor_x = row_len;
    }
}

// Add this with other function declarations near the top of the file
void editorInsertOllamaResponse();

void editorProcessKeypress() {
    static int quit_times = MEL_QUIT_TIMES;
    static int help_screen = 0;
	
    int c = editorReadKey();
	
	if (help_screen) {
        help_screen = 0;
        editorRefreshScreen();
        return;
    }


    switch (c) {
        case '\r': // Enter key
            makeAction(NewLine, NULL);
            break;
        case CTRL_KEY('q'):
            if (ec.dirty && quit_times > 0) {
                editorSetStatusMessage("Warning! File has unsaved changes. Press Ctrl-Q %d more time%s to quit", quit_times, quit_times > 1 ? "s" : "");
                quit_times--;
                return;
            }
            editorClearScreen();
            freeAlist();
            consoleBufferClose();
            exit(0);
            break;
		case CTRL_KEY('j'):
            editorReplace();
            break;
        case CTRL_KEY('s'):
            editorSave();
            break;
        case CTRL_KEY('e'):
            if (ec.cursor_y > 0 && ec.cursor_y <= ec.num_rows - 1)
                makeAction(FlipUp, NULL);
            break;
        case CTRL_KEY('d'):
            if (ec.cursor_y < ec.num_rows - 1)
                makeAction(FlipDown, NULL);
            break;
        case CTRL_KEY('x'):
            {
                if (ec.cursor_y < ec.num_rows) {
                    editorCopy(NO_STATUS);
                    char* string = NULL;
                    if(ec.copied_char_buffer)
                        string = strndup(ec.copied_char_buffer, strlen(ec.copied_char_buffer));
                    makeAction(CutLine, string);
                }
            }
            break;
        case CTRL_KEY('c'):
            if (ec.cursor_y < ec.num_rows)
                editorCopy(STATUS_YES);
            break;
        case CTRL_KEY('v'):
            {
                char* string = NULL;
                if(ec.copied_char_buffer)
                    string = strndup(ec.copied_char_buffer, strlen(ec.copied_char_buffer));
                makeAction(PasteLine, string);
            }
            break;
        case CTRL_KEY('p'):
            consoleBufferClose();
            kill(0, SIGTSTP);
            break;
		case CTRL_KEY('w'):
            editorInsertOllamaResponse();
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
        case PAGE_UP:
        case PAGE_DOWN:
            { // You can't declare variables directly inside a switch statement.
                if (c == PAGE_UP)
                    ec.cursor_y = ec.row_offset;
                else if (c == PAGE_DOWN)
                    ec.cursor_y = ec.row_offset + ec.screen_rows - 1;

                int times = ec.screen_rows;
                while (times--)
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;
        case HOME_KEY:
            ec.cursor_x = 0;
            break;
        case END_KEY:
            if (ec.cursor_y < ec.num_rows)
                ec.cursor_x = ec.row[ec.cursor_y].size;
            break;
        
		
		case CTRL_KEY('f'):
            editorSearch();
            break;
        case CTRL_KEY('n'):
        case CTRL_KEY('r'):
            editorSearchCallback(NULL, c);
            break;

        case CTRL_KEY('h'):
            help_screen = 1;
            editorDisplayHelpPage();
            break;

        case BACKSPACE:
            if(ec.cursor_x == 0 && ec.cursor_y == 0) break;
            if (c == DEL_KEY)
                editorMoveCursor(ARROW_RIGHT);
            editor_row* row = &ec.row[ec.cursor_y];
            char* string = ec.cursor_x > 0 ? strndup(&row->chars[ec.cursor_x-1], 1) : NULL;
            makeAction(DelChar, string);
            break;
        case DEL_KEY:
            {
                if(ec.cursor_x == 0 && ec.cursor_y == 0) break;
                if (c == DEL_KEY)
                    editorMoveCursor(ARROW_RIGHT);
                editor_row* row = &ec.row[ec.cursor_y];
                char* string = ec.cursor_x > 0 ? strndup(&row->chars[ec.cursor_x-1], 1) : NULL;
                makeAction(DelChar, string);
            }
            break;
			
		case CTRL_KEY('g'): // Ctrl+G to go to a specific line
    editorGoToLine();
    break;
			
		case CTRL_KEY('b'): // Ctrl+L to toggle line numbers
    ec.show_line_numbers = !ec.show_line_numbers;
    editorSetStatusMessage("Line numbers %s", ec.show_line_numbers ? "enabled" : "disabled");
    break;

        case CTRL_KEY('l'):
        case '\x1b': // Escape key
            break;
        case CTRL_KEY('z'):
            undo();
            break;
        case CTRL_KEY('y'):
            redo();
            break;
        default:
            makeAction(InsertChar, strndup((char*) &c, 1));
            break;
    }

    quit_times = MEL_QUIT_TIMES;
}

void editorDisplayHelpPage() {
    editorClearScreen();
    
    // Disable line buffering
    setbuf(stdout, NULL);

    printf("MEL - Mini Editor for Linux v.0.2.0\r\n\r\n");
    
    printf("KEYBINDINGS\r\n");
    printf("-----------\r\n\r\n");
    printf("Keybinding    Action\r\n\r\n");
    printf("Ctrl-Q        Exit, 3 times click Ctrl-Q if file was changed without saving\r\n");
    printf("Ctrl-S        Save, requires input of file name, if file didn't exist\r\n");
    printf("Ctrl-F        Search by pattern, Esc - exit from Search, works after Ctrl-F only\r\n");
	printf("Ctrl-N        Forward Search by pattern after Ctrl-F. Esc - exit from Search, works after Ctrl-F only\r\n");
	printf("Ctrl-R        Backward Search by pattern after Ctrl-F. Esc - exit from Search, Enter and Arrows to interact\r\n");
	printf("Ctrl-J        Global replacement of сharacter combinations, Input Search and Replace patterns, Esc to cancel, Enter to input\r\n");
	printf("Ctrl-G        Go to line Number, requires input the line number\r\n");
	printf("Ctrl-B        Hide/Show line numbering\r\n");
    printf("Ctrl-E        Flip line upwards\r\n");
    printf("Ctrl-D        Flip line downwards\r\n");
    printf("Ctrl-C        Copy line\r\n");
    printf("Ctrl-X        Cut line\r\n");
    printf("Ctrl-V        Paste line\r\n");
    printf("Ctrl-Z        Undo\r\n");
    printf("Ctrl-Y        Redo\r\n");
    printf("Ctrl-P        Pause mel (type \"fg\" to resume)\r\n");
    printf("Ctrl-W        Retrieve Ollama LLM response\r\n");
    printf("Ctrl-H        Toggle this help screen\r\n");
	printf("Home          Move the cursor to the beginning of the line\r\n");
	printf("End           Move cursor to end of line\r\n");
	printf("PgUp          Up page scroll\r\n");
	printf("PgDn          Down page scroll\r\n");
	printf("Up            Move cursor up one position\r\n");
	printf("Down          Move cursor down one position\r\n");
	printf("Left          Move cursor left one position\r\n");
	printf("Right         Move cursor right one position\r\n");
	printf("Backspace     Delete character\r\n");
	
    printf("\r\nOPTIONS\r\n");
    printf("-----------------------------------------\r\n");
    printf("Option                                          Action\r\n\r\n");
    printf("-h | --help                                     Prints the help\r\n");
    printf("-v | --version                                  Prints the version of mel\r\n");
	printf("-b | --backup                                   Create backup (.bak) file before saving\r\n");
	printf("-l | --line  <number> <file_name>               Open file with cursor on specified line number\r\n");
	printf("-w | --width <columns>                          Set visual column width marker\r\n");
    printf("-----------------------------------------\r\n");
    printf("Supports highlighting for C,C++,Java,Bash,Mshell,Python,PHP,Javascript,JSON,XML,SQL,Ruby,Go.\r\n");
	printf("License: Public domain libre software GPL3,v.0.2.0, 2025\r\n");
	printf("Initial coding: Igor Lukyanov, igor.lukyanov@appservgrid.com\r\n");
	printf("For now, usage of UTF-8 is recommended.\r\n\r\n");
    printf("Press any key to continue...");
    editorReadKey();
    editorRefreshScreen();
    
    // Restore line buffering
    setbuf(stdout, NULL);
}

// Ollama Configuration Structure
struct OllamaConfig {
    char api_url[256];
    char model[128];
} ollama_config;

size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct a_buf *mem = (struct a_buf *)userp;

    char *ptr = realloc(mem->buf, mem->len + realsize + 1);
    if(!ptr) {
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->buf = ptr;
    memcpy(&(mem->buf[mem->len]), contents, realsize);
    mem->len += realsize;
    mem->buf[mem->len] = 0;

    return realsize;
}

int readOllamaConfig(const char *config_path) {
    FILE *file = fopen(config_path, "r");
    if (!file) {
        editorSetStatusMessage("Could not open config file");
        return 0;
    }

    char line[256];
    ollama_config.api_url[0] = '\0';
    ollama_config.model[0] = '\0';

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        if (strncmp(line, "OLLAMA1_API_URL=", 16) == 0) {
            strncpy(ollama_config.api_url, line + 16, sizeof(ollama_config.api_url) - 1);
        } else if (strncmp(line, "OLLAMA1_MODEL=", 14) == 0) {
            strncpy(ollama_config.model, line + 14, sizeof(ollama_config.model) - 1);
        }
    }
    fclose(file);

    return (ollama_config.api_url[0] != '\0' && ollama_config.model[0] != '\0');
}

char* callOllamaAPI(const char* prompt) {
    CURL *curl;
    CURLcode res;
    struct a_buf response = ABUF_INIT;
    char* output = NULL;

    curl = curl_easy_init();
    if(curl) {
        json_object *request_json = json_object_new_object();
        json_object_object_add(request_json, "model", json_object_new_string(ollama_config.model));
        json_object_object_add(request_json, "prompt", json_object_new_string(prompt));
        json_object_object_add(request_json, "stream", json_object_new_boolean(0));

        const char* json_str = json_object_to_json_string(request_json);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, ollama_config.api_url);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            editorSetStatusMessage("Ollama API call failed: %s", curl_easy_strerror(res));
        } else {
            json_object *parsed_json = json_tokener_parse(response.buf);
            json_object *response_obj;
            if (json_object_object_get_ex(parsed_json, "response", &response_obj)) {
                output = strdup(json_object_get_string(response_obj));
            }
            json_object_put(parsed_json);
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        json_object_put(request_json);
        abufFree(&response);
    }

    return output;
}

void editorInsertOllamaResponse() {
    static char* config_path = NULL;
    if (!config_path) {
        config_path = malloc(PATH_MAX);
        snprintf(config_path, PATH_MAX, "%s/.config/mel/ollama.conf", getenv("HOME"));
    }

    if (!readOllamaConfig(config_path)) {
        editorSetStatusMessage("Invalid Ollama configuration");
        return;
    }

    char* prompt = editorPrompt("Ollama Prompt: %s (ESC to cancel)", NULL);
    if (!prompt) return;

    ec.cursor_y = ec.num_rows;
    ec.cursor_x = 0;

    char* response = callOllamaAPI(prompt);
    free(prompt);

    if (response) {
        makeAction(NewLine, NULL);
        
        char *line, *saveptr;
        for (line = strtok_r(response, "\n", &saveptr); line != NULL; 
             line = strtok_r(NULL, "\n", &saveptr)) {
            makeAction(InsertChar, strndup(line, strlen(line)));
            makeAction(NewLine, NULL);
        }
        
        free(response);
        editorSetStatusMessage("Ollama response inserted");
    }
}

/*** Init section ***/

void initEditor() {
    ec.cursor_x = 0; // Start cursor after line numbering
    ec.cursor_y = 0;
    ec.render_x = 0;
    ec.row_offset = 0;
    ec.col_offset = 0; // Ensure line number padding
    ec.num_rows = 0;
    ec.row = NULL;
    ec.dirty = 0;
	ec.show_line_numbers = 1; // Show line numbers by default
	ec.create_backup = 0;  // Initialize backup flag
	ec.line_number_offset = 0;  // Initialize the line number offset
	ec.column_marker = 0;  // No column marker by default
    ec.file_name = NULL;
    ec.status_msg[0] = '\0';
    ec.status_msg_time = 0;
    ec.copied_char_buffer = NULL;
    ec.syntax = NULL;
    ec.actions = actionListInit();
    if (!ec.actions) {
        die("Failed to initialize actions list");
    }

    // Get the window size first
    if (getWindowSize(&ec.screen_rows, &ec.screen_cols) == -1) {
        die("Failed to get window size");
    }
    // Make room for status bar and message bar
    ec.screen_rows -= 2;

    // Create initial empty row - this is crucial for empty documents
    // editorInsertRow(0, "", 0);
    //if (ec.num_rows == 0) {
    //    die("Failed to create initial row");
    //}

    // Set up signal handlers
    signal(SIGWINCH, editorHandleSigwinch);
    signal(SIGCONT, editorHandleSigcont);
}

void printHelp() {
    printf("Usage: mel [OPTIONS] [FILE]\n\n");
    printf("\nKEYBINDINGS\n-----------\n\n");
    printf("Keybinding    Action\n\n");
	printf("Ctrl-Q        Exit, 3 times click Ctrl-Q if file was changed without saving\n");
	printf("Ctrl-S        Save, requires input of file name, if file didn't exist\n");
	printf("Ctrl-F        Search by pattern, Esc - exit from Search, Enter and Arrows to interact searching\n");
	printf("Ctrl-N        Forward Search by pattern after Ctrl-F. Esc - exit from Search, works after Ctrl-F only\n");
	printf("Ctrl-R        Backward Search by pattern after Ctrl-F. Esc - exit from Search, works after Ctrl-F only\n");
    printf("Ctrl-J        Global replacement of сharacter combinations, Input Search and Replace patterns, Esc to cancel, Enter to input\n");
	printf("Ctrl-G        Go to line Number, requires input the line number\r\n");
	printf("Ctrl-B        Hide/Show line numbering\n");
	printf("Ctrl-E        Flip line upwards\n");
    printf("Ctrl-D        Flip line downwards\n");
    printf("Ctrl-C        Copy line\n");
    printf("Ctrl-X        Cut line\n");
    printf("Ctrl-V        Paste line\n");
    printf("Ctrl-Z        Undo\n");
    printf("Ctrl-Y        Redo\n");
    printf("Ctrl-P        Pause mel (type \"fg\" to resume)\n");
	printf("Ctrl-W        Retrieve Ollama LLM response\n");
    printf("Ctrl-H        Toggle this help screen\n");
	printf("Home          Move the cursor to the beginning of the line\n");
	printf("End           Move cursor to end of line\n");
	printf("PgUp          Up page scroll\n");
	printf("PgDn          Down page scroll\n");
	printf("Up            Move cursor up one position\n");
	printf("Down          Move cursor down one position\n");
	printf("Left          Move cursor left one position\n");
	printf("Right         Move cursor right one position\n");
	printf("Backspace     Delete character\n");
    printf("\n\nOPTIONS\n-------------------------------------\n");
    printf("Option                                          Action\n\n");
    printf("-h | --help                                     Prints the help\n");
    printf("-v | --version                                  Prints the version of mel\n");
	printf("-b | --backup                                   Create backup (.bak) file before saving\n");
	printf("-l | --line  <number> <file_name>               Open file with cursor on specified line number\n");
	printf("-w | --width <columns>                          Set visual column width marker\n");
	printf("-------------------------------------\n");
	printf("Supports highlighting for C,C++,Java,Bash,Mshell,Python,PHP,Javascript,JSON,XML,SQL,Ruby,Go\n");
	printf("License: Public domain libre software GPL3,v.0.2.0, 2025\n");
	printf("Initial coding: Igor Lukyanov, igor.lukyanov@appservgrid.com\n");
	printf("For now, usage of UTF-8 is recommended.\n");
}

// > 0 if editor should load a file, 0 otherwise and -1 if the program should exit
// Modify handleArgs to separate option processing from file handling
int handleArgs(int argc, char* argv[]) {
    if (argc == 1) {
        return 0;
    }

    // Process all arguments
    for (int i = 1; i < argc; i++) {
        if (strncmp("-h", argv[i], 2) == 0 || strncmp("--help", argv[i], 6) == 0) {
            printHelp();
            return -1;
        } else if (strncmp("-v", argv[i], 2) == 0 || strncmp("--version", argv[i], 9) == 0) {
            printf("mel - version %s\n", MEL_VERSION);
            return -1;
        } else if (strncmp("-b", argv[i], 2) == 0 || strncmp("--backup", argv[i], 8) == 0) {
            ec.create_backup = 1;
        } else if (strncmp("-w", argv[i], 2) == 0 || strncmp("--width", argv[i], 7) == 0) {
            if (i + 1 >= argc) {
                printf("[ERROR] Column width value must be specified\n");
                return -1;
            }
            int width = atoi(argv[i + 1]);
            if (width < 0) {
                printf("[ERROR] Column width must be a positive number\n");
                return -1;
            }
            ec.column_marker = width;
            i++; // Skip the width value
        } else if (strncmp("-l", argv[i], 2) == 0 || strncmp("--line", argv[i], 6) == 0) {
            if (i + 1 >= argc) {
                printf("[ERROR] Line number must be specified\n");
                return -1;
            }
            int start_line = atoi(argv[i + 1]);
            if (start_line < 1) {
                printf("[ERROR] Line number must be positive\n");
                return -1;
            }
            ec.cursor_y = start_line - 1;
            i++; // Skip the line number
        }
    }

    return 1;
}

//int main(int argc, char* argv[]) {
  //  initEditor();
    //int arg_response = handleArgs(argc, argv);
    //if (arg_response > 0) {
		// If -t option was used, the file name will be argv[2], otherwise argv[1]
      //  char* filename = (strncmp("-t", argv[1], 2) == 0 || strncmp("--use-tabs", argv[1], 10) == 0) 
        //                ? argv[2] : argv[argc - 1];
	//editorOpen(argv[argc - 1]); } 
    //else if (arg_response == -1)
      //  return 0;
    //enableRawMode();

  //  editorSetStatusMessage(" Ctrl-Q to quit | Ctrl-S to save | (mel -h | --help for more info)");

    //while (1) {
      //  editorRefreshScreen();
        //editorProcessKeypress();
    //}

   // return 0;
// }


int main(int argc, char* argv[]) {
    initEditor();
    
    // Process options first
    int arg_response = handleArgs(argc, argv);
    if (arg_response == -1) {
        return 0;
    }

    // Check if input is being redirected
    if (!isatty(STDIN_FILENO)) {
        // Open terminal device for later use
        int tty = open("/dev/tty", O_RDWR);
        if (tty == -1) die("Failed to open /dev/tty");

        // Save terminal settings from the TTY, not stdin
        if (tcgetattr(tty, &ec.orig_termios) == -1)
            die("tcgetattr");

        // Read from stdin
        editorOpenFromStdin();
        
        // Switch stdin to the terminal
        dup2(tty, STDIN_FILENO);
        close(tty);
    } else if (arg_response > 0) {
        // Find the filename (last non-option argument)
        char* filename = NULL;
        for (int i = argc - 1; i > 0; i--) {
            if (argv[i][0] != '-') {
                // Skip option values
                if (i > 1 && (strncmp(argv[i-1], "-w", 2) == 0 || 
                             strncmp(argv[i-1], "-l", 2) == 0)) {
                    continue;
                }
                filename = argv[i];
                break;
            }
        }
        if (filename) {
            editorOpen(filename);
        } else {
            editorInsertRow(0, "", 0);
        }
    } else {
        editorInsertRow(0, "", 0);
    }
    
    enableRawMode();
    editorSetStatusMessage(" Ctrl-Q to quit | Ctrl-S to save | (mel -h | --help for more info)");
    
    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    
    return 0;
}