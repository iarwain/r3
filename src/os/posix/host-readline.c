//
//  File: %host-readline.c
//  Summary: "Simple readline() line input handler"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2017 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Processes special keys for input line editing and recall.
//
// Avoids use of complex OS libraries and GNU readline() but hardcodes some
// parts only for the common standard.
//
// !!! This code is more or less unchanged from R3-Alpha.  It is very
// primitive, and does not support UTF-8.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h> //for read and write

//#define TEST_MODE  // teset as stand-alone program

#ifndef NO_TTY_ATTRIBUTES
    #include <termios.h>
#endif

#include "reb-host.h"

// Configuration:
#define TERM_BUF_LEN 4096   // chars allowed per line
#define READ_BUF_LEN 64     // chars per read()
#define MAX_HISTORY  300    // number of lines stored


#define WRITE_CHAR(s) \
    do { \
        if (write(1, s, 1) == -1) { \
            /* Error here, or better to "just try to keep going"? */ \
        } \
    } while (0)

#define WRITE_CHARS(s,n) \
    do { \
        if (write(1, s, n) == -1) { \
            /* Error here, or better to "just try to keep going"? */ \
        } \
    } while (0)

#define WRITE_STR(s) \
    do { \
        if (write(1, s, strlen(s)) == -1) { \
            /* Error here, or better to "just try to keep going"? */ \
        } \
    } while (0)

#define DBG_INT(t,n) //printf("\r\ndbg[%s]: %d\r\n", t, (n));
#define DBG_STR(t,s) //printf("\r\ndbg[%s]: %s\r\n", t, (s));

typedef struct term_data {
    REBYTE *buffer;
    REBYTE *residue;
    REBYTE *out;
    int pos;
    int end;
    int hist;
} STD_TERM;

// Globals:
static REBOOL Term_Initialized = FALSE;     // Terminal init was successful
static REBYTE **Line_History;                 // Prior input lines
static int Line_Count;                      // Number of prior lines

#ifndef NO_TTY_ATTRIBUTES
static struct termios Term_Attrs;   // Initial settings, restored on exit
#endif


extern STD_TERM *Init_Terminal(void);

//
//  Init_Terminal: C
//
// Change the terminal modes to those required for proper
// REBOL console handling. Return TRUE on success.
//
STD_TERM *Init_Terminal(void)
{
#ifndef NO_TTY_ATTRIBUTES
    struct termios attrs;

    if (Term_Initialized || tcgetattr(0, &Term_Attrs)) return NULL;

    attrs = Term_Attrs;

    // Local modes:
    attrs.c_lflag &= ~(ECHO | ICANON); // raw input

    // Input modes:
    attrs.c_iflag &= ~(ICRNL | INLCR); // leave CR an LF as is

    // Output modes:
    attrs.c_oflag |= ONLCR; // On output, emit CRLF

    // Special modes:
    attrs.c_cc[VMIN] = 1;   // min num of bytes for READ to return
    attrs.c_cc[VTIME] = 0;  // how long to wait for input

    tcsetattr(0, TCSADRAIN, &attrs);
#endif

    // Setup variables:
    Line_History = OS_ALLOC_N(REBYTE*, MAX_HISTORY + 2);

    char empty_line[] = "";
    Line_History[0] = OS_ALLOC_N(REBYTE, LEN_BYTES(empty_line) + 1);
    strcpy(s_cast(Line_History[0]), empty_line);
    Line_Count = 1;

    STD_TERM *term = OS_ALLOC_ZEROFILL(STD_TERM);
    term->buffer = OS_ALLOC_N(REBYTE, TERM_BUF_LEN);
    term->buffer[0] = 0;
    term->residue = OS_ALLOC_N(REBYTE, TERM_BUF_LEN);
    term->residue[0] = 0;

    Term_Initialized = TRUE;

    return term;
}


extern void Quit_Terminal(STD_TERM *term);

//
//  Quit_Terminal: C
//
// Restore the terminal modes original entry settings,
// in preparation for exit from program.
//
void Quit_Terminal(STD_TERM *term)
{
    int n;

    if (Term_Initialized) {
#ifndef NO_TTY_ATTRIBUTES
        tcsetattr(0, TCSADRAIN, &Term_Attrs);
#endif
        OS_FREE(term->residue);
        OS_FREE(term->buffer);
        OS_FREE(term);
        for (n = 0; n < Line_Count; n++) OS_FREE(Line_History[n]);
        OS_FREE(Line_History);
    }

    Term_Initialized = FALSE;
}


//
//  Write_Char: C
//
// Write out repeated number of chars.
// Unicode: not used
//
static void Write_Char(REBYTE c, int n)
{
    REBYTE buf[4];

    buf[0] = c;
    for (; n > 0; n--)
        WRITE_CHAR(buf);
}


//
//  Store_Line: C
//
// Makes a copy of the current buffer and store it in the
// history list. Returns the copied string.
//
static void Store_Line(STD_TERM *term)
{
    term->buffer[term->end] = 0;
    term->out = OS_ALLOC_N(REBYTE, term->end + 1);
    strcpy(s_cast(term->out), s_cast(term->buffer));

    // If max history, drop older lines (but not [0] empty line):
    if (Line_Count >= MAX_HISTORY) {
        OS_FREE(Line_History[1]);
        memmove(
            Line_History + 1,
            Line_History + 2,
            (MAX_HISTORY - 2) * sizeof(REBYTE*)
        );
        Line_Count = MAX_HISTORY - 1;
    }

    Line_History[Line_Count] = term->out;
    ++Line_Count;
}


//
//  Recall_Line: C
//
// Set the current buffer to the contents of the history
// list at its current position. Clip at the ends.
// Return the history line index number.
// Unicode: ok
//
static void Recall_Line(STD_TERM *term)
{
    if (term->hist < 0) term->hist = 0;

    if (term->hist == 0)
        Write_Char(BEL, 1); // bell

    if (term->hist >= Line_Count) {
        // Special case: no "next" line:
        term->hist = Line_Count;
        term->buffer[0] = 0;
        term->pos = term->end = 0;
    }
    else {
        // Fetch prior line:
        strcpy(s_cast(term->buffer), s_cast(Line_History[term->hist]));
        term->pos = term->end = LEN_BYTES(term->buffer);
    }
}


//
//  Clear_Line: C
//
// Clear all the chars from the current position to the end.
// Reset cursor to current position.
// Unicode: not used
//
static void Clear_Line(STD_TERM *term)
{
    Write_Char(' ', term->end - term->pos); // wipe prior line
    Write_Char(BS, term->end - term->pos); // return to position
}


//
//  Home_Line: C
//
// Reset cursor to home position.
// Unicode: not used
//
static void Home_Line(STD_TERM *term)
{
    Write_Char(BS, term->pos);
    term->pos = 0;
}


//
//  End_Line: C
//
// Move cursor to end position.
// Unicode: not used
//
static void End_Line(STD_TERM *term)
{
    int len = term->end - term->pos;

    if (len > 0) {
        WRITE_CHARS(term->buffer+term->pos, len);
        term->pos = term->end;
    }
}


//
//  Show_Line: C
//
// Refresh a line from the current position to the end.
// Extra blanks can be specified to erase chars off end.
// If blanks is negative, stay at end of line.
// Reset the cursor back to current position.
// Unicode: ok
//
static void Show_Line(STD_TERM *term, int blanks)
{
    int len;

    //printf("\r\nsho pos: %d end: %d ==", term->pos, term->end);

    // Clip bounds:
    if (term->pos < 0) term->pos = 0;
    else if (term->pos > term->end) term->pos = term->end;

    if (blanks >= 0) {
        len = term->end - term->pos;
        WRITE_CHARS(term->buffer+term->pos, len);
    }
    else {
        WRITE_CHARS(term->buffer, term->end);
        blanks = -blanks;
        len = 0;
    }

    Write_Char(' ', blanks);
    Write_Char(BS, blanks + len); // return to position or end
}


//
//  Insert_Char: C
//
// Insert a char at the current position. Adjust end position.
// Redisplay the line.
// Unicode: not yet supported!
//
static REBYTE *Insert_Char(STD_TERM *term, REBYTE *cp)
{
    //printf("\r\nins pos: %d end: %d ==", term->pos, term->end);
    if (term->end < TERM_BUF_LEN-1) { // avoid buffer overrun

        if (term->pos < term->end) { // open space for it:
            memmove(
                term->buffer + term->pos + 1, // dest pointer
                term->buffer + term->pos, // source pointer
                1 + term->end - term->pos // length
            );
        }
        WRITE_CHAR(cp);
        term->buffer[term->pos] = *cp;
        term->end++;
        term->pos++;
        Show_Line(term, 0);
    }

    return ++cp;
}


//
//  Delete_Char: C
//
// Delete a char at the current position. Adjust end position.
// Redisplay the line. Blank out extra char at end.
// Unicode: not yet supported!
//
static void Delete_Char(STD_TERM *term, REBOOL back)
{
    int len;

    if ( (term->pos == term->end) && back == 0) return; //Ctrl-D at EOL

    if (back) term->pos--;

    len = 1 + term->end - term->pos;

    if (term->pos >= 0 && len > 0) {
        memmove(term->buffer + term->pos, term->buffer + term->pos + 1, len);
        if (back) Write_Char(BS, 1);
        term->end--;
        Show_Line(term, 1);
    }
    else term->pos = 0;
}


//
//  Move_Cursor: C
//
// Move cursor right or left by one char.
// Unicode: not yet supported!
//
static void Move_Cursor(STD_TERM *term, int count)
{
    if (count < 0) {
        if (term->pos > 0) {
            term->pos--;
            Write_Char(BS, 1);
        }
    }
    else {
        if (term->pos < term->end) {
            WRITE_CHAR(term->buffer + term->pos);
            term->pos++;
        }
    }
}


//
//  Process_Key: C
//
// Process the next key. If it's an edit key, perform the
// necessary editing action. Return position of next char.
// Unicode: not yet supported!
//
static REBYTE *Process_Key(STD_TERM *term, REBYTE *cp)
{
    if (*cp == 0)
        return cp;

    // No UTF-8 yet
    if (*cp > 127)
        *cp = '?';

    if (*cp == ESC) {
        // Escape sequence:
        cp++;
        if (*cp == '[' || *cp == 'O') {

            // Special key:
            switch (*++cp) {

            // Arrow keys:
            case 'A':   // up arrow
                term->hist -= 2;
                // falls through
            case 'B': {  // down arrow
                int len = term->end;

                ++term->hist;

                Home_Line(term);
                Recall_Line(term);

                if (len <= term->end)
                    len = 0;
                else
                    len = term->end - len;

                Show_Line(term, len - 1); // len < 0 (stay at end)
                break; }

            case 'D':   // left arrow
                Move_Cursor(term, -1);
                break;
            case 'C':   // right arrow
                Move_Cursor(term, 1);
                break;

            // Other special keys:
            case '1':   // home
                Home_Line(term);
                cp++; // remove ~
                break;
            case '4':   // end
                End_Line(term);
                cp++; // remove ~
                break;
            case '3':   // delete
                Delete_Char(term, FALSE);
                cp++; // remove ~
                break;

            case 'H':   // home
                Home_Line(term);
                break;
            case 'F':   // end
                End_Line(term);
                break;

            case 'J':   // erase to end of screen
                Clear_Line(term);
                break;

            default:
                WRITE_STR("[ESC]");
                cp--;
            }
        }
        else {
            switch (*++cp) {
            case 'H':   // home
                Home_Line(term);
                break;
            case 'F':   // end
                End_Line(term);
                break;
            default:
                // Q: what other keys do we want to support ?!
                WRITE_STR("[ESC]");
                cp--;
            }
        }
    }
    else {
        // ASCII char:
        switch (*cp) {

        case  BS:   // backspace
        case DEL:   // delete
            Delete_Char(term, TRUE);
            break;

        case CR:    // CR
            if (cp[1] == LF) cp++; // eat
            // falls through
        case LF:    // LF
            WRITE_STR("\r\n");
            Store_Line(term);
            break;

        case 1: // CTRL-A
            Home_Line(term);
            break;
        case 2: // CTRL-B
            Move_Cursor(term, -1);
            break;
        case 4: // CTRL-D
            Delete_Char(term, FALSE);
            break;
        case 5: // CTRL-E
            End_Line(term);
            break;
        case 6: // CTRL-F
            Move_Cursor(term, 1);
            break;

        default:
            cp = Insert_Char(term, cp);
            cp--;
        }
    }

    return ++cp;
}


//
//  Read_Bytes: C
//
// Read the next "chunk" of data into the terminal buffer.
//
static int Read_Bytes(STD_TERM *term, REBYTE *buf, int len)
{
    int end;

    // If we have leftovers:
    if (term->residue[0]) {
        end = LEN_BYTES(term->residue);
        if (end < len) len = end;
        strncpy(s_cast(buf), s_cast(term->residue), len); // terminated below
        memmove(term->residue, term->residue+len, end-len); // remove
        term->residue[end-len] = 0;
    }
    else {
        // Read next few bytes. We don't know how many may be waiting.
        // We assume that escape-sequences are always complete in buf.
        // (No partial escapes.) If this is not true, then we will need
        // to add an additional "collection" loop here.
        if ((len = read(0, buf, len)) < 0) {
            WRITE_STR("\r\nI/O terminated\r\n");
            Quit_Terminal(term); // something went wrong
            exit(100);
        }
    }

    buf[len] = 0;
    buf[len+1] = 0;

    DBG_INT("read len", len);

    return len;
}


extern int Read_Line(STD_TERM *term, REBYTE *result, int limit);

//
//  Read_Line: C
//
// Read a line (as a sequence of bytes) from the terminal.
// Handles line editing and line history recall.
// Returns number of bytes in line.
//
int Read_Line(STD_TERM *term, REBYTE *result, int limit)
{
    REBYTE buf[READ_BUF_LEN];
    REBYTE *cp;
    int len;        // length of IO read

    term->pos = term->end = 0;
    term->hist = Line_Count;
    term->out = 0;
    term->buffer[0] = 0;

    do {
        Read_Bytes(term, buf, READ_BUF_LEN-2);
        for (cp = buf; *cp;) {
            cp = Process_Key(term, cp);
        }
    } while (!term->out);

    // Not at end of input? Save any unprocessed chars:
    if (*cp) {
        if (LEN_BYTES(term->residue) + LEN_BYTES(cp) >= TERM_BUF_LEN - 1) {
            //
            // avoid overrun
        }
        else
            strcat(s_cast(term->residue), s_cast(cp));
    }

    // Fill the output buffer:
    len = LEN_BYTES(term->out);
    if (len >= limit-1) len = limit-2;
    strncpy(s_cast(result), s_cast(term->out), limit);
    result[len++] = LF;
    result[len] = 0;

    return len;
}

#ifdef TEST_MODE
test(STD_TERM *term, char *cp) {
    term->hist = Line_Count;
    term->pos = term->end = 0;
    term->out = 0;
    term->buffer[0] = 0;
    while (*cp) cp = Process_Key(term, cp);
}

main() {
    int i;
    char buf[1024];
    STD_TERM *term;

    term = Init_Terminal();

    Write_Char('-', 50);
    WRITE_STR("\r\n");

#ifdef WIN32
    test(term, "text\010\010st\n"); //bs bs
    test(term, "test\001xxxx\n"); // home
    test(term, "test\001\005xxxx\n"); // home
    test(term, "\033[A\n"); // up arrow
#endif

    do {
        WRITE_STR(">> ");
        i = Read_Line(term, buf, 1000);
        printf("len: %d %s\r\n", i, term->out);
    } while (i > 0);

    Quit_Terminal(term);
}
#endif
