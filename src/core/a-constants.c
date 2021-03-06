//
//  File: %a-constants.c
//  Summary: "special global constants and strings"
//  Section: environment
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
// Most text strings in Rebol should appear in the bootstrap files as Rebol
// code.  This allows for "internationalization" without needing to update
// the C code.  Other advantages are that the strings are compressed,
// "reduces tampering", etc.
//
// So to keep track of any stray English strings in the executable which make
// it into the user's view, they should be located here.
//
// Note: It's acceptable for hardcoded English strings to appear in the debug
// build or in other debug settings, as anyone working with the C code itself
// is basically expected to be able to read English (given the variable names
// and comments in the C are English).
//

#include "sys-core.h"

const char Str_REBOL[] = "REBOL";

// A panic() indicates a serious malfunction, and should not make use of
// Rebol-structured error message delivery in the release build.

const char Str_Panic_Title[] = "Rebol Internal Error";

const char Str_Panic_Directions[] = {
    "If you need to file a bug in the issue tracker, please give thorough\n"
    "details on how to reproduce the problem:\n"
    "\n"
    "    https://github.com/metaeducation/ren-c/issues\n"
    "\n"
    "Include the following information in the report:\n\n"
};

const char * Hex_Digits = "0123456789ABCDEF";

const char * const Esc_Names[] = {
    // Must match enum REBOL_Esc_Codes!
    "line",
    "tab",
    "page",
    "escape",
    "esc",
    "back",
    "del",
    "null"
};

const REBYTE Esc_Codes[] = {
    // Must match enum REBOL_Esc_Codes!
    10,     // line
    9,      // tab
    12,     // page
    27,     // escape
    27,     // esc
    8,      // back
    127,    // del
    0       // null
};

// Zen Point on naming cues: was "Month_Lengths", but said 29 for Feb! --@HF
const REBYTE Month_Max_Days[12] = {
    31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

const char * const Month_Names[12] = {
    "January", "February", "March", "April", "May", "June", "July", "August",
    "September", "October", "November", "December"
};


// Used by scanner. Keep in sync with enum Reb_Token in %scan.h file!
//
const char * const Token_Names[] = {
    "end-of-script",
    "newline",
    "block-end",
    "group-end",
    "word",
    "set",
    "get",
    "lit",
    "blank",
    "bar",
    "lit-bar",
    "logic",
    "integer",
    "decimal",
    "percent",
    "money",
    "time",
    "date",
    "char",
    "block-begin",
    "group-begin",
    "string",
    "binary",
    "pair",
    "tuple",
    "file",
    "email",
    "url",
    "issue",
    "tag",
    "path",
    "refine",
    "construct",
    NULL
};


// !!! For now, (R)ebol (M)essages use the historical Debug_Fmt() output
// method, which is basically like `printf()`.  Over the long term, they
// should use declarations like the (R)ebol (E)rrors do with RE_XXX values
// loaded during boot.
//
// The goal should be that any non-debug-build only strings mentioned from C
// that can be seen in the course of normal operation should go through this
// abstraction.  Ultimately that would permit internationalization, and the
// benefit of not needing to ship a release build binary with a string-based
// format dialect.
//
// Switching strings to use this convention should ultimately parallel the
// `Error()` generation, where the arguments are Rebol values and not C
// raw memory as parameters.  Debug_Fmt() should also just be changed to
// a normal `Print()` naming.
//
const char RM_ERROR_LABEL[] = "Error: ";
const char RM_BAD_ERROR_FORMAT[] = "(improperly formatted error)";
const char RM_ERROR_WHERE[] = "** Where: ";
const char RM_ERROR_NEAR[] = "** Near: ";
const char RM_ERROR_FILE[] = "** File: ";
const char RM_ERROR_LINE[] = "** Line: ";

const char RM_WATCH_RECYCLE[] = "RECYCLE: %d series";

const char RM_TRACE_FUNCTION[] = "--> %s";
const char RM_TRACE_RETURN[] = "<-- %s ==";
const char RM_TRACE_ERROR[] = "**: error : %r %r";

const char RM_TRACE_PARSE_VALUE[] = "Parse %s: %r";
const char RM_TRACE_PARSE_INPUT[] = "Parse input: %s";

const char RM_BACKTRACE_NOT_ENABLED[] = "backtrace not enabled";

const char RM_EVOKE_HELP[] = "Evoke values:\n"
    "[stack-size n] crash-dump delect\n"
    "watch-recycle watch-obj-copy crash\n"
    "1: watch expand\n"
    "2: check memory pools\n"
    "3: check bind table\n";
