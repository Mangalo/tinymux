// eval.cpp -- Command evaluation and cracking.
//
// $Id: eval.cpp,v 1.41 2003-03-13 15:42:07 sdennis Exp $
//

// MUX 2.1
// Portions are derived from MUX 1.6. Portions are original work.
//
// Copyright (C) 1998 through 2002 Solid Vertical Domains, Ltd. All
// rights not explicitly given are reserved. Permission is given to
// use this code for building and hosting text-based game servers.
// Permission is given to use this code for other non-commercial
// purposes. To use this code for commercial purposes other than
// building/hosting text-based game servers, contact the author at
// Stephen Dennis <sdennis@svdltd.com> for another license.
//

#include "copyright.h"
#include "autoconf.h"
#include "config.h"
#include "externs.h"

#include "attrs.h"
#include "functions.h"
#include "ansi.h"

//-----------------------------------------------------------------------------
// parse_to: Split a line at a character, obeying nesting.  The line is
// destructively modified (a null is inserted where the delimiter was found)
// dstr is modified to point to the char after the delimiter, and the function
// return value points to the found string (space compressed if specified). If
// we ran off the end of the string without finding the delimiter, dstr is
// returned as NULL.
//
static char *parse_to_cleanup( int eval, int first, char *cstr, char *rstr,
                               char *zstr, char *strFirewall
                             )
{
    if (  (mudconf.space_compress || (eval & EV_STRIP_TS))
       && !(eval & EV_NO_COMPRESS)
       && !first
       && (cstr > strFirewall)
       && (cstr[-1] == ' ')
       )
    {
        zstr--;
    }

    if (  (eval & EV_STRIP_AROUND)
       && (*rstr == '{')
       && (zstr > strFirewall)
       && (zstr[-1] == '}')
       )
    {
        rstr++;
        if (mudconf.space_compress && !(eval & EV_NO_COMPRESS) || (eval & EV_STRIP_LS))
        {
            while (Tiny_IsSpace[(unsigned char)*rstr])
            {
                rstr++;
            }
        }
        rstr[-1] = '\0';
        zstr--;
        if (mudconf.space_compress && !(eval & EV_NO_COMPRESS) || (eval & EV_STRIP_TS))
        {
            while ((zstr > strFirewall) && Tiny_IsSpace[(unsigned char)zstr[-1]])
            {
                zstr--;
            }
        }
        *zstr = '\0';
    }
    *zstr = '\0';
    return rstr;
}

// During parsing, this table may be modified for a particular terminating delimeter.
// The table is always restored it's original state.
//
// 0 means mundane character.
// 1 is 0x20 ' '  delim overridable (only done by parse_to, not parse_to_lite)
// 2 is 0x5B '['  delim overridable
// 3 is 0x28 '('  delim overridable
// 4 is 0x25 '%', 0x5C '\\', or 0x1B ESC not overridable.
// 5 is 0x29 ')' or 0x5D ']'  not overridable.
// 6 is 0x7B '{' not overridable.
// 7 is 0x00 '\0' not overridable.
// 8 is the client-specific terminator.
//
// A code 4 or above means that the client-specified delim cannot override it.
// A code 8 is temporary.
//
static char isSpecial_L3[256] =
{
    7, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0x00-0x0F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 4, 0, 0, 0, 0, // 0x10-0x1F
    0, 0, 0, 0, 0, 4, 0, 0,  3, 5, 0, 0, 0, 0, 0, 0, // 0x20-0x2F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0x30-0x3F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0x40-0x4F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 2, 4, 5, 0, 0, // 0x50-0x5F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0x60-0x6F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 6, 0, 0, 0, 0, // 0x70-0x7F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0x80-0x8F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0x90-0x9F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0xA0-0xAF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0xB0-0xBF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0xC0-0xCF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0xD0-0xDF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0xE0-0xEF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0  // 0xF0-0xFF
};

static const char isSpecial_L4[256] =
{
    4, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0x00-0x0F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 1, 0, 0, 0, 0, // 0x10-0x1F
    0, 0, 0, 0, 0, 1, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0x20-0x2F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0x30-0x3F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0x40-0x4F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 1, 0, 0, 0, // 0x50-0x5F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0x60-0x6F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 2, 0, 3, 0, 0, // 0x70-0x7F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0x80-0x8F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0x90-0x9F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0xA0-0xAF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0xB0-0xBF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0xC0-0xCF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0xD0-0xDF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0xE0-0xEF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0  // 0xF0-0xFF
};

// Characters that are valid q-registers, and their offsets in the register
// array. -1 for invalid registers.
//
const signed char Tiny_IsRegister[256] =
{
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1, // 0x00-0x0F
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1, // 0x10-0x1F
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1, // 0x20-0x2F
     0, 1, 2, 3, 4, 5, 6, 7,  8, 9,-1,-1,-1,-1,-1,-1, // 0x30-0x3F
    -1,10,11,12,13,14,15,16, 17,18,19,20,21,22,23,24, // 0x40-0x4F
    25,26,27,28,29,30,31,32, 33,34,35,-1,-1,-1,-1,-1, // 0x50-0x5F
    -1,10,11,12,13,14,15,16, 17,18,19,20,21,22,23,24, // 0x60-0x6F
    25,26,27,28,29,30,31,32, 33,34,35,-1,-1,-1,-1,-1, // 0x70-0x7F
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1, // 0x80-0x8F
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1, // 0x90-0x9F
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1, // 0xA0-0xAF
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1, // 0xB0-0xBF
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1, // 0xC0-0xCF
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1, // 0xD0-0xDF
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1, // 0xE0-0xEF
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1  // 0xF0-0xFF
};

// Stephen: Some silly compilers don't handle aliased pointers well. For these
// compilers, we can't change this to just '*zstr++ = *cstr++'. However all
// up-to-date compilers that I know about handle this correctly.
//
#if 1
#define NEXTCHAR *zstr++ = *cstr++;
#else
#define NEXTCHAR \
    if (cstr == zstr) \
    { \
        cstr++; \
        zstr++; \
    } \
    else \
    { \
        *zstr++ = *cstr++; \
    }
#endif


char *parse_to(char **dstr, char delim, int eval)
{
#define stacklim 32
    char stack[stacklim];
    char *rstr, *cstr, *zstr, *strFirewall;
    int sp, tp, first, bracketlev;

    if ((dstr == NULL) || (*dstr == NULL))
        return NULL;

    if (**dstr == '\0')
    {
        rstr = *dstr;
        *dstr = NULL;
        return rstr;
    }
    sp = 0;
    first = 1;
    strFirewall = rstr = *dstr;
    if ((mudconf.space_compress || (eval & EV_STRIP_LS)) && !(eval & EV_NO_COMPRESS))
    {
        while (Tiny_IsSpace[(unsigned char)*rstr])
            rstr++;
        *dstr = rstr;
    }
    zstr = cstr = rstr;
    int iOriginalCode = isSpecial_L3[(unsigned char)delim];
    isSpecial_L3[' '] = 1; // Spaces are special.
    if (iOriginalCode <= 3)
    {
        // We can override this code.
        //
        isSpecial_L3[(unsigned char)delim] = 8;
    }

    for (;;)
    {
        int iCode = isSpecial_L3[(unsigned char)*cstr];

TryAgain:
        if (iCode == 0)
        {
            // Mudane characters and not the delimiter we are looking for.
            //
            first = 0;
            do
            {
                NEXTCHAR
                iCode = isSpecial_L3[(unsigned char)*cstr];
            } while (iCode == 0);
        }

        if (iCode <= 4)
        {
            // 1 is 0x20 ' '  delim overridable
            // 2 is 0x5B '['  delim overridable
            // 3 is 0x28 '('  delim overridable
            // 4 is 0x25 '%', 0x5C '\\', or 0x1B ESC not overridable.
            //
            if (iCode <= 2)
            {
                // 1 is 0x20 ' '  delim overridable
                // 2 is 0x5B '['  delim overridable
                //
                if (iCode == 1)
                {
                    // space
                    //
                    if (mudconf.space_compress && !(eval & EV_NO_COMPRESS))
                    {
                        if (first)
                        {
                            rstr++;
                        }
                        else if ((cstr > strFirewall) && cstr[-1] == ' ')
                        {
                            zstr--;
                        }
                    }
                    NEXTCHAR
                }
                else
                {
                    // '['
                    //
                    first = 0;
                    if (sp < stacklim)
                    {
                        stack[sp++] = ']';
                    }
                    NEXTCHAR
                }
            }
            else
            {
                // 3 is 0x28 '('  delim overridable
                // 4 is 0x25 '%', 0x5C '\\', or 0x1B ESC not overridable.
                //
                if (iCode == 3)
                {
                    first = 0;
                    if (sp < stacklim)
                    {
                        stack[sp++] = ')';
                    }
                    NEXTCHAR
                }
                else
                {
                    // %, \, and ESC escapes.
                    //
                    first = 0;
                    NEXTCHAR
                    if (*cstr)
                    {
                        NEXTCHAR
                    }
                }
            }
        }
        else
        {
            // 5 is 0x29 ')' or 0x5D ']'  not overridable.
            // 6 is 0x7B '{' not overridable.
            // 7 is 0x00 '\0' not overridable.
            // 8 is the client-specific terminator.
            //
            if (iCode <= 6)
            {
                // 5 is 0x29 ')' or 0x5D ']'  not overridable.
                // 6 is 0x7B '{' not overridable.
                //
                if (iCode == 5)
                {
                    // ) and ]
                    //
                    for (tp = sp - 1; (tp >= 0) && (stack[tp] != *cstr); tp--)
                    {
                        ;
                    }

                    // If we hit something on the stack, unwind to it. Otherwise (it's
                    // not on stack), if it's our delim we are done, and we convert the
                    // delim to a null and return a ptr to the char after the null. If
                    // it's not our delimiter, skip over it normally.
                    //
                    if (tp >= 0)
                    {
                        sp = tp;
                    }
                    else if (*cstr == delim)
                    {
                        rstr = parse_to_cleanup(eval, first, cstr, rstr, zstr, strFirewall);
                        *dstr = ++cstr;
                        isSpecial_L3[(unsigned char)delim] = iOriginalCode;
                        isSpecial_L3[' '] = 0; // Spaces aren't special anymore
                        return rstr;
                    }
                    first = 0;
                    NEXTCHAR
                }
                else
                {
                    // {
                    //
                    bracketlev = 1;
                    if (eval & EV_STRIP_CURLY)
                    {
                        cstr++;
                    }
                    else
                    {
                        NEXTCHAR;
                    }
                    for (;;)
                    {
                        int iCodeL4 = isSpecial_L4[(unsigned char)*cstr];
                        if (iCodeL4 == 0)
                        {
                            // Mudane Characters
                            //
                            do
                            {
                                NEXTCHAR
                                iCodeL4 = isSpecial_L4[(unsigned char)*cstr];
                            } while (iCodeL4 == 0);
                        }


                        if (iCodeL4 == 1)
                        {
                            // %, \, and ESC escapes.
                            //
                            if (cstr[1])
                            {
                                NEXTCHAR
                            }
                        }
                        else if (iCodeL4 == 2)
                        {
                            // '{'
                            //
                            bracketlev++;
                        }
                        else if (iCodeL4 == 3)
                        {
                            // '}'
                            //
                            bracketlev--;
                            if (bracketlev <= 0)
                            {
                                break;
                            }
                        }
                        else
                        {
                            // '\0'
                            //
                            break;
                        }
                        NEXTCHAR
                    }

                    if (bracketlev == 0)
                    {
                        if (eval & EV_STRIP_CURLY)
                        {
                            cstr++;
                        }
                        else
                        {
                            NEXTCHAR
                        }
                    }
                    first = 0;
                }
            }
            else
            {
                // 7 is 0x00 '\0' not overridable.
                // 8 is the client-specific terminator.
                //
                if (iCode == 7)
                {
                    // '\0' - End of string.
                    //
                    isSpecial_L3[(unsigned char)delim] = iOriginalCode;
                    isSpecial_L3[' '] = 0; // Spaces aren't special anymore
                    break;
                }
                else
                {
                    // Client-Specific terminator
                    //
                    if (sp == 0)
                    {
                        rstr = parse_to_cleanup(eval, first, cstr, rstr, zstr, strFirewall);
                        *dstr = ++cstr;
                        isSpecial_L3[(unsigned char)delim] = iOriginalCode;
                        isSpecial_L3[' '] = 0; // Spaces aren't special anymore
                        return rstr;
                    }

                    // At this point, we need to process the iOriginalCode.
                    //
                    iCode = iOriginalCode;
                    goto TryAgain;
                }
            }
        }
    }
    rstr = parse_to_cleanup(eval, first, cstr, rstr, zstr, strFirewall);
    *dstr = NULL;
    return rstr;
}

// This version parse_to is less destructive. It only null-terminates the source
// It doesn't process escapes. It's useful with TinyExec which will be copying
// the characters to another buffer anyway and is more than able to perform the
// escapes and trimming.
//
char *parse_to_lite(char **dstr, char delim1, char delim2, int *nLen, int *iWhichDelim)
{
#define stacklim 32
    char stack[stacklim];
    char *rstr, *cstr;
    int sp, tp, bracketlev;

    if ((dstr == NULL) || (*dstr == NULL))
    {
        *nLen = 0;
        return NULL;
    }

    if (**dstr == '\0')
    {
        rstr = *dstr;
        *dstr = NULL;
        *nLen = 0;
        return rstr;
    }
    sp = 0;
    cstr = rstr = *dstr;
    int iOriginalCode1 = isSpecial_L3[(unsigned char)delim1];
    int iOriginalCode2 = isSpecial_L3[(unsigned char)delim2];
    if (iOriginalCode1 <= 3)
    {
        // We can override this code.
        //
        isSpecial_L3[(unsigned char)delim1] = 8;
    }
    if (iOriginalCode2 <= 3)
    {
        // We can override this code.
        //
        isSpecial_L3[(unsigned char)delim2] = 8;
    }

    for (;;)
    {
        int iCode = isSpecial_L3[(unsigned char)*cstr];

TryAgain:
        if (iCode == 0)
        {
            // Mudane characters and not the delimiter we are looking for.
            //
            do
            {
                cstr++;
                iCode = isSpecial_L3[(unsigned char)*cstr];
            } while (iCode == 0);
        }

        if (iCode <= 4)
        {
            // 2 is 0x5B '['  delim overridable
            // 3 is 0x28 '('  delim overridable
            // 4 is 0x25 '%' or 0x5C '\\' not overridable.
            //
            if (iCode <= 3)
            {
                // 2 is 0x5B '['  delim overridable
                // 3 is 0x28 '('  delim overridable
                //
                if (sp < stacklim)
                {
                    static char matcher[2] = { ']', ')'};
                    stack[sp++] = matcher[iCode-2];
                }
                cstr++;
            }
            else
            {
                // 4 is 0x25 '%' or 0x5C '\\' not overridable.
                //
                cstr++;
                if (*cstr)
                {
                    cstr++;
                }
            }
        }
        else
        {
            // 5 is 0x29 ')' or 0x5D ']'  not overridable.
            // 6 is 0x7B '{' not overridable.
            // 7 is 0x00 '\0' not overridable.
            // 8 is the client-specific terminator.
            //
            if (iCode <= 6)
            {
                // 5 is 0x29 ')' or 0x5D ']'  not overridable.
                // 6 is 0x7B '{' not overridable.
                //
                if (iCode == 5)
                {
                    // ) and ]
                    //
                    for (tp = sp - 1; (tp >= 0) && (stack[tp] != *cstr); tp--)
                    {
                        ;
                    }

                    // If we hit something on the stack, unwind to it. Otherwise (it's
                    // not on stack), if it's our delim we are done, and we convert the
                    // delim to a null and return a ptr to the char after the null. If
                    // it's not our delimiter, skip over it normally.
                    //
                    if (tp >= 0)
                    {
                        sp = tp;
                    }
                    else if (*cstr == delim1 || *cstr == delim2)
                    {
                        if (*cstr == delim1)
                        {
                            *iWhichDelim = 1;
                        }
                        else
                        {
                            *iWhichDelim = 2;
                        }
                        *cstr = '\0';
                        *nLen = (cstr - rstr);
                        *dstr = ++cstr;
                        isSpecial_L3[(unsigned char)delim1] = iOriginalCode1;
                        isSpecial_L3[(unsigned char)delim2] = iOriginalCode2;
                        return rstr;
                    }
                    cstr++;
                }
                else
                {
                    // {
                    //
                    bracketlev = 1;
                    cstr++;
                    for (;;)
                    {
                        int iCodeL4 = isSpecial_L4[(unsigned char)*cstr];
                        if (iCodeL4 == 0)
                        {
                            // Mudane Characters
                            //
                            do
                            {
                                cstr++;
                                iCodeL4 = isSpecial_L4[(unsigned char)*cstr];
                            } while (iCodeL4 == 0);
                        }


                        if (iCodeL4 == 1)
                        {
                            // '\\' or '%'
                            //
                            if (cstr[1])
                            {
                                cstr++;
                            }
                        }
                        else if (iCodeL4 == 2)
                        {
                            // '{'
                            //
                            bracketlev++;
                        }
                        else if (iCodeL4 == 3)
                        {
                            // '}'
                            //
                            bracketlev--;
                            if (bracketlev <= 0)
                            {
                                break;
                            }
                        }
                        else
                        {
                            // '\0'
                            //
                            break;
                        }
                        cstr++;
                    }

                    if (bracketlev == 0)
                    {
                        cstr++;
                    }
                }
            }
            else
            {
                // 7 is 0x00 '\0' not overridable.
                // 8 is the client-specific terminator.
                //
                if (iCode == 7)
                {
                    // '\0' - End of string.
                    //
                    isSpecial_L3[(unsigned char)delim1] = iOriginalCode1;
                    isSpecial_L3[(unsigned char)delim2] = iOriginalCode2;
                    break;
                }
                else
                {
                    // Client-Specific terminator
                    //
                    if (sp == 0)
                    {
                        if (*cstr == delim1)
                        {
                            *iWhichDelim = 1;
                        }
                        else
                        {
                            *iWhichDelim = 2;
                        }
                        *cstr = '\0';
                        *nLen = (cstr - rstr);
                        *dstr = ++cstr;
                        isSpecial_L3[(unsigned char)delim1] = iOriginalCode1;
                        isSpecial_L3[(unsigned char)delim2] = iOriginalCode2;
                        return rstr;
                    }

                    // At this point, we need to process the iOriginalCode.
                    //
                    if (*cstr == delim1)
                    {
                        iCode = iOriginalCode1;
                    }
                    else
                    {
                        iCode = iOriginalCode2;
                    }
                    goto TryAgain;
                }
            }
        }
    }
    *iWhichDelim = 0;
    *cstr = '\0';
    *nLen = (cstr - rstr);
    *dstr = NULL;
    return rstr;
}

//-----------------------------------------------------------------------------
// parse_arglist: Parse a line into an argument list contained in lbufs. A
// pointer is returned to whatever follows the final delimiter. If the arglist
// is unterminated, a NULL is returned.  The original arglist is destructively
// modified.
//
char *parse_arglist( dbref player, dbref cause, char *dstr, char delim,
                     dbref eval, char *fargs[], dbref nfargs, char *cargs[],
                     dbref ncargs, int *nArgsParsed )
{
    char *rstr, *tstr, *bp, *str;
    int arg, peval;

    if (dstr == NULL)
    {
        *nArgsParsed = 0;
        return NULL;
    }

    int nLen;
    int iWhichDelim;
    rstr = parse_to_lite(&dstr, delim, '\0', &nLen, &iWhichDelim);
    arg = 0;

    peval = (eval & ~EV_EVAL);

    while ((arg < nfargs) && rstr)
    {
        if (arg < (nfargs - 1))
            tstr = parse_to(&rstr, ',', peval);
        else
            tstr = parse_to(&rstr, '\0', peval);

        bp = fargs[arg] = alloc_lbuf("parse_arglist");
        if (eval & EV_EVAL)
        {
            str = tstr;
            TinyExec(fargs[arg], &bp, 0, player, cause, eval | EV_FCHECK,
                     &str, cargs, ncargs);
            *bp = '\0';
        }
        else
        {
            strcpy(fargs[arg], tstr);
        }
        arg++;
    }
    *nArgsParsed = arg;
    return dstr;
}

char *parse_arglist_lite( dbref player, dbref cause, char *dstr, char delim,
                          int eval, char *fargs[], dbref nfargs, char *cargs[],
                          dbref ncargs, int *nArgsParsed)
{
    char *tstr, *bp, *str;

    if (dstr == NULL)
    {
        *nArgsParsed = 0;
        return NULL;
    }

    int nLen;
    int peval = eval;
    if (eval & EV_EVAL)
    {
        peval = eval | EV_FCHECK;
    }
    else
    {
        peval = ((eval & ~EV_FCHECK)|EV_NOFCHECK);
    }
    int arg = 0;
    int iWhichDelim = 0;
    while ((arg < nfargs) && dstr && iWhichDelim != 2)
    {
        if (arg < (nfargs - 1))
            tstr = parse_to_lite(&dstr, ',', ')', &nLen, &iWhichDelim);
        else
            tstr = parse_to_lite(&dstr, '\0', ')', &nLen, &iWhichDelim);

        if (iWhichDelim == 2 && arg == 0 && tstr[0] == '\0')
        {
            break;
        }

        bp = fargs[arg] = alloc_lbuf("parse_arglist");
        str = tstr;
        TinyExec( fargs[arg], &bp, 0, player, cause, peval, &str, cargs, ncargs );
        *bp = '\0';
        arg++;
    }
    *nArgsParsed = arg;
    return dstr;
}

//-----------------------------------------------------------------------------
// exec: Process a command line, evaluating function calls and %-substitutions.
//
int get_gender(dbref player)
{
    dbref aowner;
    int aflags;
    char *atr_gotten = atr_pget(player, A_SEX, &aowner, &aflags);
    char first = atr_gotten[0];
    free_lbuf(atr_gotten);
    switch (Tiny_ToLower[(unsigned char)first])
    {
    case 'p':
        return 4;
    case 'm':
        return 3;
    case 'f':
    case 'w':
        return 2;
    }
    return 1;
}

//---------------------------------------------------------------------------
// Trace cache routines.
//
typedef struct tcache_ent TCENT;
struct tcache_ent
{
    dbref player;
    char *orig;
    char *result;
    struct tcache_ent *next;
} *tcache_head;

int tcache_top, tcache_count;

void NDECL(tcache_init)
{
    tcache_head = NULL;
    tcache_top = 1;
    tcache_count = 0;
}

int NDECL(tcache_empty)
{
    if (tcache_top)
    {
        tcache_top = 0;
        tcache_count = 0;
        return 1;
    }
    return 0;
}

static void tcache_add(dbref player, char *orig, char *result)
{
    char *tp;
    TCENT *xp;

    if (strcmp(orig, result))
    {
        tcache_count++;
        if (tcache_count <= mudconf.trace_limit)
        {
            xp = (TCENT *) alloc_sbuf("tcache_add.sbuf");
            tp = alloc_lbuf("tcache_add.lbuf");
            StringCopy(tp, result);
            xp->player = player;
            xp->orig = orig;
            xp->result = tp;
            xp->next = tcache_head;
            tcache_head = xp;
        }
        else
        {
            free_lbuf(orig);
        }
    }
    else
    {
        free_lbuf(orig);
    }
}

static void tcache_finish(void)
{
    TCENT *xp;

    while (tcache_head != NULL)
    {
        xp = tcache_head;
        tcache_head = xp->next;
        notify(Owner(xp->player), tprintf("%s(#%d)} '%s' -> '%s'", Name(xp->player),
            xp->player, xp->orig, xp->result));
        free_lbuf(xp->orig);
        free_lbuf(xp->result);
        free_sbuf(xp);
    }
    tcache_top = 1;
    tcache_count = 0;
}

const char *ColorTable[256] =
{
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,      // 0x00-0x0F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,      // 0x10-0x1F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,      // 0x20-0x2F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,      // 0x30-0x3F
    0,           0,             ANSI_BBLUE,  ANSI_BCYAN,  // 0x40-0x43
    0,           0,             0,           ANSI_BGREEN, // 0x44-0x47
    0,           0,             0,           0,           // 0x48-0x4B
    0,           ANSI_BMAGENTA, 0,           0,           // 0x4B-0x4F
    0,           0,             ANSI_BRED,   0,           // 0x50-0x53
    0,           0,             0,           ANSI_BWHITE, // 0x54-0x57
    ANSI_BBLACK, ANSI_BYELLOW,  0,           0,           // 0x58-0x5B
    0,           0,             0,           0,           // 0x5B-0x5F
    0,           0,             ANSI_BLUE,   ANSI_CYAN,   // 0x60-0x63
    0,           0,             ANSI_BLINK,  ANSI_GREEN,  // 0x64-0x67
    ANSI_HILITE, ANSI_INVERSE,  0,           0,           // 0x68-0x6B
    0,           ANSI_MAGENTA,  ANSI_NORMAL, 0,           // 0x6C-0x6F
    0,           0,             ANSI_RED,    0,           // 0x70-0x73
    0,           ANSI_UNDER,    0,           ANSI_WHITE,  // 0x74-0x77
    ANSI_BLACK,  ANSI_YELLOW,   0,           0,           // 0x78-0x7B
    0,           0,             0,           0,           // 0x7B-0x7F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,      // 0x80-0x8F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,      // 0x90-0x9F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,      // 0xA0-0xAF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,      // 0xB0-0xBF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,      // 0xC0-0xCF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,      // 0xD0-0xDF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,      // 0xE0-0xEF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0       // 0xF0-0xFF
};

static char isSpecial_L1[256] =
{
    1, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0x00-0x0F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 1, 0, 0, 0, 0, // 0x10-0x1F
    1, 0, 0, 0, 0, 1, 0, 0,  1, 0, 0, 0, 0, 0, 0, 0, // 0x20-0x2F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0x30-0x3F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0x40-0x4F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 1, 1, 0, 0, 0, // 0x50-0x5F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0x60-0x6F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 1, 0, 0, 0, 0, // 0x70-0x7F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0x80-0x8F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0x90-0x9F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0xA0-0xAF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0xB0-0xBF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0xC0-0xCF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0xD0-0xDF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, // 0xE0-0xEF
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0  // 0xF0-0xFF
};

static const unsigned char isSpecial_L2[256] =
{
     18,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0, // 0x00-0x0F
      0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0, // 0x10-0x1F
      0,  4,  0,  3,  0, 11,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0, // 0x20-0x2F
      1,  1,  1,  1,  1,  1,  1,  1,   1,  1,  0,  0,  0,  0,  0,  0, // 0x30-0x3F
      0,145,  7,  6,  0,  0,  0,  0,   0,  0,  0,  0,  9,147,140,144, // 0x40-0x4F
    143,130,  5,142,  8,  0,138,  0,   6,  0,  0,  0,  0,  0,  0,  0, // 0x50-0x5F
      0, 17,  7,  6,  0,  0,  0,  0,   0,  0,  0,  0,  9, 19, 12, 16, // 0x60-0x6F
     15,  2,  5, 14,  8,  0, 10,  0,   6,  0,  0,  0, 13,  0,  0,  0, // 0x70-0x7F
      0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0, // 0x80-0x8F
      0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0, // 0x90-0x9F
      0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0, // 0xA0-0xAF
      0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0, // 0xB0-0xBF
      0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0, // 0xC0-0xCF
      0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0, // 0xD0-0xDF
      0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0, // 0xE0-0xEF
      0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0  // 0xF0-0xFF
};

#define PTRS_PER_FRAME ((LBUF_SIZE - sizeof(char *) - sizeof(int))/sizeof(char *))
typedef struct tag_ptrsframe
{
    int   nptrs;
    char *ptrs[PTRS_PER_FRAME];
    struct tag_ptrsframe *next;
} PtrsFrame;

static PtrsFrame *pPtrsFrame = NULL;

static DCL_INLINE char **PushPointers(int nNeeded)
{
    if (  !pPtrsFrame
       || nNeeded > pPtrsFrame->nptrs)
    {
        PtrsFrame *p = (PtrsFrame *)alloc_lbuf("PushPointers");
        p->next = pPtrsFrame;
        p->nptrs = PTRS_PER_FRAME;
        pPtrsFrame = p;
    }
    pPtrsFrame->nptrs -= nNeeded;
    return pPtrsFrame->ptrs + pPtrsFrame->nptrs;
}

static DCL_INLINE void PopPointers(char **p, int nNeeded)
{
    if (pPtrsFrame->nptrs == PTRS_PER_FRAME)
    {
        PtrsFrame *p = pPtrsFrame->next;
        free_lbuf((char *)pPtrsFrame);
        pPtrsFrame = p;
    }
    //Tiny_Assert(p == pPtrsFrame->ptrs + pPtrsFrame->nptrs);
    pPtrsFrame->nptrs += nNeeded;
}

#define INTS_PER_FRAME ((LBUF_SIZE - sizeof(char *) - sizeof(int))/sizeof(int))
typedef struct tag_intsframe
{
    int   nints;
    int   ints[INTS_PER_FRAME];
    struct tag_intsframe *next;
} IntsFrame;

static IntsFrame *pIntsFrame = NULL;

static DCL_INLINE int *PushIntegers(int nNeeded)
{
    if (  !pIntsFrame
       || nNeeded > pIntsFrame->nints)
    {
        IntsFrame *p = (IntsFrame *)alloc_lbuf("PushIntegers");
        p->next = pIntsFrame;
        p->nints = INTS_PER_FRAME;
        pIntsFrame = p;
    }
    pIntsFrame->nints -= nNeeded;
    return pIntsFrame->ints + pIntsFrame->nints;
}

static DCL_INLINE void PopIntegers(int *pi, int nNeeded)
{
    if (pIntsFrame->nints == INTS_PER_FRAME)
    {
        IntsFrame *p = pIntsFrame->next;
        free_lbuf((char *)pIntsFrame);
        pIntsFrame = p;
    }
    //Tiny_Assert(pi == pIntsFrame->ints + pIntsFrame->nints);
    pIntsFrame->nints += nNeeded;
}

void TinyExec( char *buff, char **bufc, int tflags, dbref player, dbref cause,
               int eval, char **dstr, char *cargs[], int ncargs)
{
    char *TempPtr;
    char *tstr, *tbuf, *start, *oldp, *savestr;
    int ch;
    char *realbuff = NULL, *realbp = NULL;
    dbref aowner;
    int at_space, nfargs, gender, aflags, feval, i;
    int is_trace, is_top;
    int ansi = 0;
    FUN *fp;
    UFUN *ufp;

    static const char *subj[5] = {"", "it", "she", "he", "they"};
    static const char *poss[5] = {"", "its", "her", "his", "their"};
    static const char *obj[5] =  {"", "it", "her", "him", "them"};
    static const char *absp[5] = {"", "its", "hers", "his", "theirs"};

    // This is scratch buffer is used potentially on every invocation of
    // TinyExec. Do not assume that it's contents are valid after you
    // execute any function that could re-enter TinyExec.
    //
    static char TinyExec_scratch[LBUF_SIZE];

    if (*dstr == NULL || **dstr == '\0')
    {
        return;
    }

    char *pdstr = *dstr;

    at_space = 1;
    gender = -1;

    is_trace = Trace(player) && !(eval & EV_NOTRACE);
    is_top = 0;

    // Extend the buffer if we need to.
    //
    if (((*bufc) - buff) > (LBUF_SIZE - SBUF_SIZE))
    {
        realbuff = buff;
        realbp = *bufc;
        buff = (char *)MEMALLOC(LBUF_SIZE);
        (void)ISOUTOFMEMORY(buff);
        *bufc = buff;
    }

    oldp = start = *bufc;

    // If we are tracing, save a copy of the starting buffer.
    //
    savestr = NULL;
    if (is_trace)
    {
        is_top = tcache_empty();
        savestr = alloc_lbuf("exec.save");
        strcpy(savestr, pdstr);
    }

    // Save Parser Mode.
    //
    char bSpaceIsSpecialSave = isSpecial_L1[' '];
    char bParenthesisIsSpecialSave = isSpecial_L1['('];
    char bBracketIsSpecialSave = isSpecial_L1['['];

    // Setup New Parser Mode.
    //
    char bSpaceIsSpecial = mudconf.space_compress && !(eval & EV_NO_COMPRESS);
    isSpecial_L1[' '] = bSpaceIsSpecial;
    isSpecial_L1['('] = (eval & EV_FCHECK) != 0;
    isSpecial_L1['['] = (eval & EV_NOFCHECK) == 0;

    int nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;
    for (;;)
    {
        // Handle Mudane characters specially. There are usually a lot of them.
        // Just copy them.
        //
        if (!isSpecial_L1[(unsigned char)*pdstr])
        {
            char *p = pdstr + 1;
            while (!isSpecial_L1[(unsigned char)*p++])
            {
                // Nothing.
                //
                ;
            }
            i = p - pdstr - 1;
            if (i > nBufferAvailable)
            {
                i = nBufferAvailable;
            }
            memcpy(*bufc, pdstr, i);
            nBufferAvailable -= i;
            *bufc += i;
            at_space = 0;
            pdstr = p - 1;
        }


        // At this point, **dstr must be one of the following characters:
        //
        // 0x00 0x20 0x25 0x28 0x5B 0x5C 0x7B
        // NULL  SP   %    (     [    \   {
        //
        // Test softcode shows the following distribution:
        //
        // NULL occurs 116948 times
        //   (  occurs  49567 times
        //   %  occurs  24553 times
        //   [  occurs   7618 times
        //  SP  occurs   1323 times
        //
        if (*pdstr == '\0')
        {
            break;
        }
        else if (*pdstr == '(')
        {
            // *pdstr == '('
            //
            // Arglist start.  See if what precedes is a function. If so,
            // execute it if we should.
            //
            at_space = 0;

            // Load an sbuf with an uppercase version of the func name, and see
            // if the func exists. Trim trailing spaces from the name if
            // configured.
            //
            char *pEnd = *bufc - 1;
            if (mudconf.space_compress)
            {
                while ((pEnd >= oldp) && Tiny_IsSpace[(unsigned char)*pEnd])
                {
                    pEnd--;
                }
            }

            // _strlwr(tbuf);
            //
            char *p2 = TinyExec_scratch;
            for (char *p = oldp; p <= pEnd; p++)
            {
                *p2++ = Tiny_ToLower[(unsigned char)*p];
            }
            *p2 = '\0';

            int ntbuf = p2 - TinyExec_scratch;
            fp = (FUN *)hashfindLEN(TinyExec_scratch, ntbuf, &mudstate.func_htab);

            // If not a builtin func, check for global func.
            //
            ufp = NULL;
            if (fp == NULL)
            {
                ufp = (UFUN *)hashfindLEN(TinyExec_scratch, ntbuf, &mudstate.ufunc_htab);
            }

            // Do the right thing if it doesn't exist.
            //
            if (!fp && !ufp)
            {
                if (eval & EV_FMAND)
                {
                    *bufc = oldp;
                    safe_str("#-1 FUNCTION (", buff, bufc);
                    safe_str(TinyExec_scratch, buff, bufc);
                    safe_str(") NOT FOUND", buff, bufc);
                    nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;
                    break;
                }
                else if (nBufferAvailable)
                {
                    *(*bufc)++ = '(';
                    nBufferAvailable--;
                }
            }
            else
            {
                // Get the arglist and count the number of args. Neg # of args
                // means catenate subsequent args.
                //
                if (ufp)
                {
                    nfargs = MAX_ARG;
                }
                else
                {
                    nfargs = fp->maxArgsParsed;
                }

                tstr = pdstr;
                if (fp && (fp->flags & FN_NO_EVAL))
                {
                    feval = eval & ~(EV_EVAL|EV_TOP);
                }
                else
                {
                    feval = eval & ~EV_TOP;
                }

                char **fargs = PushPointers(MAX_ARG);
                pdstr = parse_arglist_lite(player, cause, pdstr + 1,
                      ')', feval, fargs, nfargs, cargs, ncargs, &nfargs);


                // If no closing delim, just insert the '(' and continue normally.
                //
                if (!pdstr)
                {
                    pdstr = tstr;
                    if (nBufferAvailable)
                    {
                        *(*bufc)++ = *pdstr;
                        nBufferAvailable--;
                    }
                }
                else
                {
                    pdstr--;

                    // If it's a user-defined function, perform it now.
                    //
                    mudstate.func_nest_lev++;
                    mudstate.func_invk_ctr++;
                    if (mudstate.func_nest_lev >= mudconf.func_nest_lim )
                    {
                         safe_str("#-1 FUNCTION RECURSION LIMIT EXCEEDED", buff, &oldp);
                    }
                    else if (mudstate.func_invk_ctr >= mudconf.func_invk_lim)
                    {
                        safe_str("#-1 FUNCTION INVOCATION LIMIT EXCEEDED", buff, &oldp);
                    }
                    else if (Going(player))
                    {
                        safe_str("#-1 BAD INVOKER", buff, &oldp);
                    }
                    else if (!check_access(player, ufp ? ufp->perms : fp->perms))
                    {
                        safe_noperm(buff, &oldp);
                    }
                    else if (ufp)
                    {
                        tstr = atr_get(ufp->obj, ufp->atr, &aowner, &aflags);
                        if (ufp->flags & FN_PRIV)
                            i = ufp->obj;
                        else
                            i = player;
                        TempPtr = tstr;

                        char **preserve = NULL;
                        int *preserve_len = NULL;

                        if (ufp->flags & FN_PRES)
                        {
                            preserve = PushPointers(MAX_GLOBAL_REGS);
                            preserve_len = PushIntegers(MAX_GLOBAL_REGS);
                            save_global_regs("eval_save", preserve, preserve_len);
                        }

                        TinyExec(buff, &oldp, 0, i, cause, feval, &TempPtr, fargs, nfargs);

                        if (ufp->flags & FN_PRES)
                        {
                            restore_global_regs("eval_restore", preserve, preserve_len);
                            PopIntegers(preserve_len, MAX_GLOBAL_REGS);
                            PopPointers(preserve, MAX_GLOBAL_REGS);
                            preserve = NULL;
                            preserve_len = NULL;
                        }
                        free_lbuf(tstr);
                    }
                    else
                    {
                        // If the number of args is right, perform the func. Otherwise
                        // return an error message.
                        //
                        if (  fp->minArgs <= nfargs
                           && nfargs <= fp->maxArgs)
                        {
                            fp->fun(buff, &oldp, player, cause, fargs, nfargs, cargs, ncargs);
                        }
                        else
                        {
                            if (fp->minArgs == fp->maxArgs)
                            {
                                sprintf(TinyExec_scratch, "#-1 FUNCTION (%s) EXPECTS %d ARGUMENTS", fp->name, fp->minArgs);
                            }
                            else if (fp->minArgs + 1 == fp->maxArgs)
                            {
                                sprintf(TinyExec_scratch, "#-1 FUNCTION (%s) EXPECTS %d OR %d ARGUMENTS", fp->name, fp->minArgs, fp->maxArgs);
                            }
                            else
                            {
                                sprintf(TinyExec_scratch, "#-1 FUNCTION (%s) EXPECTS BETWEEN %d AND %d ARGUMENTS", fp->name, fp->minArgs, fp->maxArgs);
                            }
                            safe_str(TinyExec_scratch, buff, &oldp);
                        }
                    }
                    *bufc = oldp;
                    nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;
                    mudstate.func_nest_lev--;
                }

                // Return the space allocated for the arguments.
                //
                for (i = 0; i < nfargs; i++)
                {
                    free_lbuf(fargs[i]);
                }
                PopPointers(fargs, MAX_ARG);
                fargs = NULL;
            }
            eval &= ~EV_FCHECK;
            isSpecial_L1['('] = 0;
        }
        else if (*pdstr == '%')
        {
            // Percent-replace start.  Evaluate the chars following and
            // perform the appropriate substitution.
            //
            at_space = 0;
            if (!(eval & EV_EVAL))
            {
                if (nBufferAvailable)
                {
                    *(*bufc)++ = '%';
                    nBufferAvailable--;
                }
                pdstr++;
                if (nBufferAvailable)
                {
                    *(*bufc)++ = *pdstr;
                    nBufferAvailable--;
                }
            }
            else
            {
                pdstr++;
                ch = *pdstr;
                unsigned char cType_L2 = isSpecial_L2[(unsigned char)ch];
                TempPtr = *bufc;
                int iCode = cType_L2 & 0x7F;
                if (iCode == 1)
                {
                    // 30 31 32 33 34 35 36 37 38 39
                    // 0  1  2  3  4  5  6  7  8  9
                    //
                    // Command argument number N.
                    //
                    i = ch - '0';
                    if (i < ncargs && cargs[i])
                    {
                        safe_str(cargs[i], buff, bufc);
                        nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;
                    }
                }
                else if (iCode == 2)
                {
                    // 51
                    // Q
                    //
                    pdstr++;
                    i = Tiny_IsRegister[(unsigned char)*pdstr];
                    if (0 <= i && i < MAX_GLOBAL_REGS)
                    {
                        if (  mudstate.glob_reg_len[i] > 0
                           && mudstate.global_regs[i])
                        {
                            safe_copy_buf(mudstate.global_regs[i],
                                mudstate.glob_reg_len[i], buff, bufc);
                            nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;
                        }
                    }
                    else if (*pdstr == '\0')
                    {
                        pdstr--;
                    }
                }
                else if (iCode <= 4)
                {
                    if (iCode == 3)
                    {
                        // 23
                        // #
                        //
                        // Invoker DB number.
                        //
                        TinyExec_scratch[0] = '#';
                        i = Tiny_ltoa(cause, TinyExec_scratch+1);
                        safe_copy_buf(TinyExec_scratch, i+1, buff, bufc);
                        nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;
                    }
                    else if (iCode == 4)
                    {
                        // 21
                        // !
                        //
                        // iCode == '!'
                        // Executor DB number.
                        //
                        TinyExec_scratch[0] = '#';
                        i = Tiny_ltoa(player, TinyExec_scratch+1);
                        safe_copy_buf(TinyExec_scratch, i+1, buff, bufc);
                        nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;
                    }
                    else
                    {
                        // iCode == 0
                        //
                        // Just copy
                        //
                        if (nBufferAvailable)
                        {
                            *(*bufc)++ = ch;
                            nBufferAvailable--;
                        }
                    }
                }
                else if (iCode <= 6)
                {
                    if (iCode == 6)
                    {
                        // 43 58
                        // C  X
                        //
                        // Color
                        //
                        const char *pColor = ColorTable[(unsigned char)pdstr[1]];
                        if (pColor)
                        {
                            pdstr++;
                            ansi = 1;
                            safe_str(pColor, buff, bufc);
                            nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;
                        }
                        else if (pdstr[1] && nBufferAvailable)
                        {
                            *(*bufc)++ = *pdstr;
                            nBufferAvailable--;
                        }
                    }
                    else
                    {
                        // 52
                        // R
                        //
                        // Carriage return.
                        //
                        safe_copy_buf("\r\n", 2, buff, bufc);
                        nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;
                    }
                }
                else if (iCode <= 8)
                {
                    if (iCode == 7)
                    {
                        // 42
                        // B
                        //
                        // Blank.
                        //
                        if (nBufferAvailable)
                        {
                            *(*bufc)++ = ' ';
                            nBufferAvailable--;
                        }
                    }
                    else
                    {
                        // 54
                        // T
                        //
                        // Tab.
                        //
                        if (nBufferAvailable)
                        {
                            *(*bufc)++ = '\t';
                            nBufferAvailable--;
                        }
                    }
                }
                else if (iCode <= 10)
                {
                    if (iCode == 9)
                    {
                        // 4C
                        // L
                        //
                        // Invoker location db#
                        //
                        if (!(eval & EV_NO_LOCATION))
                        {
                            TinyExec_scratch[0] = '#';
                            i = Tiny_ltoa(where_is(cause), TinyExec_scratch+1);
                            safe_copy_buf(TinyExec_scratch, i+1, buff, bufc);
                            nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;
                        }
                    }
                    else
                    {
                        // 56
                        // V
                        //
                        // Variable attribute.
                        //
                        pdstr++;
                        if (Tiny_IsAlpha[(unsigned char)(*pdstr)])
                        {
                            i = A_VA + Tiny_ToUpper[(unsigned char)(*pdstr)] - 'A';
                            int nAttrGotten;
                            atr_pget_str_LEN(TinyExec_scratch, player, i,
                                &aowner, &aflags, &nAttrGotten);
                            if (nAttrGotten > 0)
                            {
                                if (nAttrGotten > nBufferAvailable)
                                {
                                    nAttrGotten = nBufferAvailable;
                                }
                                memcpy(*bufc, TinyExec_scratch, nAttrGotten);
                                *bufc += nAttrGotten;
                                nBufferAvailable -= nAttrGotten;
                            }
                        }
                        else if (ch == '\0')
                        {
                            pdstr--;
                        }
                    }
                }
                else if (iCode <= 14)
                {
                    if (iCode <= 12)
                    {
                        if (iCode == 11)
                        {
                            // 25
                            // %
                            //
                            // Percent - a literal %
                            //
                            if (nBufferAvailable)
                            {
                                *(*bufc)++ = '%';
                                nBufferAvailable--;
                            }
                        }
                        else
                        {
                            // 4E
                            // N
                            //
                            // Invoker name
                            //
                            safe_str(Name(cause), buff, bufc);
                            nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;
                        }
                    }
                    else
                    {
                        if (iCode == 13)
                        {
                            // 7C
                            // |
                            //
                            // piped command output.
                            //
                            safe_str(mudstate.pout, buff, bufc);
                            nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;
                        }
                        else
                        {
                            // 53
                            // S
                            //
                            // Subjective pronoun.
                            //
                            if (gender < 0)
                            {
                                gender = get_gender(cause);
                            }
                            if (!gender)
                            {
                                tbuf = Name(cause);
                            }
                            else
                            {
                                tbuf = (char *)subj[gender];
                            }
                            safe_str(tbuf, buff, bufc);
                            nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;
                        }
                    }
                }
                else
                {
                    if (iCode <= 16)
                    {
                        if (iCode == 15)
                        {
                            // 50
                            // P
                            //
                            // Personal pronoun.
                            //
                            if (gender < 0)
                            {
                                gender = get_gender(cause);
                            }

                            if (!gender)
                            {
                                safe_str(Name(cause), buff, bufc);
                                nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;
                                if (nBufferAvailable)
                                {
                                    *(*bufc)++ = 's';
                                    nBufferAvailable--;
                                }
                            }
                            else
                            {
                                safe_str((char *)poss[gender], buff, bufc);
                                nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;
                            }
                        }
                        else
                        {
                            // 4F
                            // O
                            //
                            // Objective pronoun.
                            //
                            if (gender < 0)
                            {
                                gender = get_gender(cause);
                            }
                            if (!gender)
                            {
                                tbuf = Name(cause);
                            }
                            else
                            {
                                tbuf = (char *)obj[gender];
                            }
                            safe_str(tbuf, buff, bufc);
                            nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;
                        }
                    }
                    else
                    {
                        if (iCode == 17)
                        {
                            // 41
                            // A
                            //
                            // Absolute posessive.
                            // Idea from Empedocles.
                            //
                            if (gender < 0)
                            {
                                gender = get_gender(cause);
                            }

                            if (!gender)
                            {
                                safe_str(Name(cause), buff, bufc);
                                nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;
                                if (nBufferAvailable)
                                {
                                    *(*bufc)++ = 's';
                                    nBufferAvailable--;
                                }
                            }
                            else
                            {
                                safe_str(absp[gender], buff, bufc);
                                nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;
                            }
                        }
                        else if (iCode == 18)
                        {
                            // 00
                            // \0
                            //
                            // All done.
                            //
                            pdstr--;
                        }
                        else
                        {
                            // 4D
                            // M
                            //
                            // Last command
                            //
                            safe_str(mudstate.curr_cmd, buff, bufc);
                            nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;
                        }
                    }
                }

                // For some escape letters, if the escape letter
                // was upper-case, then upper-case the first
                // letter of the value.
                //
                if (cType_L2 & 0x80)
                {
                    *TempPtr = Tiny_ToUpper[(unsigned char)*TempPtr];
                }
            }
        }
        else if (*pdstr == '[')
        {
            // Function start.  Evaluate the contents of the square brackets
            // as a function. If no closing bracket, insert the '[' and
            // continue.
            //
            tstr = pdstr++;
            tbuf = parse_to_lite(&pdstr, ']', '\0', &at_space, &at_space);
            at_space = 0;
            if (pdstr == NULL)
            {
                if (nBufferAvailable)
                {
                    *(*bufc)++ = '[';
                    nBufferAvailable--;
                }
                pdstr = tstr;
            }
            else
            {
                TempPtr = tbuf;
                TinyExec( buff, bufc, 0, player, cause,
                          (eval | EV_FCHECK | EV_FMAND) & ~EV_TOP, &TempPtr,
                          cargs, ncargs
                        );
                nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;
                pdstr--;
            }
        }

        // At this point, *pdstr must be one of the following characters:
        //
        // 0x20 0x5C 0x7B
        // SP    \    {
        //
        else if (*pdstr == ' ')
        {
            // A space. Add a space if not compressing or if previous char was
            // not a space.
            //
            if (bSpaceIsSpecial && !at_space)
            {
                if (nBufferAvailable)
                {
                    *(*bufc)++ = ' ';
                    nBufferAvailable--;
                }
                at_space = 1;
            }
        }
        else if (*pdstr == '{')
        {
            // *pdstr == '{'
            //
            // Literal start.  Insert everything up to the terminating '}'
            // without parsing. If no closing brace, insert the '{' and
            // continue.
            //
            tstr = pdstr++;
            tbuf = parse_to_lite(&pdstr, '}', '\0', &at_space, &at_space);
            at_space = 0;
            if (pdstr == NULL)
            {
                if (nBufferAvailable)
                {
                    *(*bufc)++ = '{';
                    nBufferAvailable--;
                }
                pdstr = tstr;
            }
            else
            {
                if (!(eval & EV_STRIP_CURLY))
                {
                    if (nBufferAvailable)
                    {
                        *(*bufc)++ = '{';
                        nBufferAvailable--;
                    }
                }

                if (eval & EV_EVAL)
                {
                    // Preserve leading spaces (Felan)
                    //
                    if (*tbuf == ' ')
                    {
                        if (nBufferAvailable)
                        {
                            *(*bufc)++ = ' ';
                            nBufferAvailable--;
                        }
                        tbuf++;
                    }

                    TempPtr = tbuf;
                    TinyExec(buff, bufc, 0, player, cause,
                        (eval & ~(EV_STRIP_CURLY | EV_FCHECK | EV_TOP)),
                        &TempPtr, cargs, ncargs);
                }
                else
                {
                    TempPtr = tbuf;
                    TinyExec(buff, bufc, 0, player, cause, eval & ~EV_TOP,
                        &TempPtr, cargs, ncargs);
                }
                nBufferAvailable = LBUF_SIZE - (*bufc - buff) - 1;

                if (!(eval & EV_STRIP_CURLY))
                {
                    if (nBufferAvailable)
                    {
                        *(*bufc)++ = '}';
                        nBufferAvailable--;
                    }
                }
                pdstr--;
            }
        }
        else if (*pdstr == '\\')
        {
            // *pdstr must be \.
            //
            // General escape. Add the following char without special
            // processing.
            //
            at_space = 0;
            pdstr++;
            if (*pdstr)
            {
                if (nBufferAvailable)
                {
                    *(*bufc)++ = *pdstr;
                    nBufferAvailable--;
                }
            }
            else
            {
                pdstr--;
            }
        }
        else
        {
            // *pdstr must be ESC.
            //
            at_space = 0;
            if (nBufferAvailable)
            {
                *(*bufc)++ = *pdstr;
                nBufferAvailable--;
            }
            pdstr++;
            if (*pdstr)
            {
                if (nBufferAvailable)
                {
                    *(*bufc)++ = *pdstr;
                    nBufferAvailable--;
                }
            }
            else
            {
                pdstr--;
            }
        }
        pdstr++;
    }

    // If we're eating spaces, and the last thing was a space, eat it up.
    // Complicated by the fact that at_space is initially true. So check to
    // see if we actually put something in the buffer, too.
    //
    if (  bSpaceIsSpecial
       && at_space
       && (start != *bufc)
       )
    {
        (*bufc)--;
    }

    **bufc = '\0';

    // If the player used a %x sub in the string, and hasn't yet terminated
    // the color with a %xn yet, we'll have to do it for them. Certain
    // overflows can trim ANSI off as well.
    //
    if (ansi || (eval & EV_TOP))
    {
        // ANSI_NORMAL is guaranteed to be written on the end.
        //
        int nVisualWidth;
        int nLen = ANSI_TruncateToField(buff, sizeof(TinyExec_scratch),
            TinyExec_scratch, sizeof(TinyExec_scratch), &nVisualWidth,
            ANSI_ENDGOAL_NORMAL);
        memcpy(buff, TinyExec_scratch, nLen+1);
        *bufc = buff + nLen;
    }

    // Report trace information.
    //
    if (is_trace)
    {
        tcache_add(player, savestr, start);
        if (is_top || !mudconf.trace_topdown)
        {
            tcache_finish();
        }
        if (  is_top
           && tcache_count - mudconf.trace_limit > 0)
        {
            tbuf = alloc_mbuf("exec.trace_diag");
            sprintf(tbuf, "%d lines of trace output discarded.", tcache_count
                - mudconf.trace_limit);
            notify(player, tbuf);
            free_mbuf(tbuf);
        }
    }
    if (realbuff)
    {
        *bufc = realbp;
        safe_str(buff, realbuff, bufc);
        MEMFREE(buff);
        buff = realbuff;
    }
    *dstr = pdstr;

    // Restore Parser Mode.
    //
    isSpecial_L1[' '] = bSpaceIsSpecialSave;
    isSpecial_L1['('] = bParenthesisIsSpecialSave;
    isSpecial_L1['['] = bBracketIsSpecialSave;
}

/* ---------------------------------------------------------------------------
 * save_global_regs, restore_global_regs:  Save and restore the global
 * registers to protect them from various sorts of munging.
 */

void save_global_regs
(
    const char *funcname,
    char *preserve[],
    int preserve_len[]
)
{
    int i;

    for (i = 0; i < MAX_GLOBAL_REGS; i++)
    {
        if (mudstate.global_regs[i])
        {
            preserve[i] = alloc_lbuf(funcname);
            int n = mudstate.glob_reg_len[i];
            memcpy(preserve[i], mudstate.global_regs[i], n);
            preserve[i][n] = '\0';
            preserve_len[i] = n;
        }
        else
        {
            preserve[i] = NULL;
            preserve_len[i] = 0;
        }
    }
}

void save_and_clear_global_regs
(
    const char *funcname,
    char *preserve[],
    int preserve_len[]
)
{
    int i;

    for (i = 0; i < MAX_GLOBAL_REGS; i++)
    {
        preserve[i] = mudstate.global_regs[i];
        preserve_len[i] = mudstate.glob_reg_len[i];

        mudstate.global_regs[i] = NULL;
        mudstate.glob_reg_len[i] = 0;
    }
}

void restore_global_regs
(
    const char *funcname,
    char *preserve[],
    int preserve_len[]
)
{
    int i;

    for (i = 0; i < MAX_GLOBAL_REGS; i++)
    {
        if (preserve[i])
        {
            if (mudstate.global_regs[i])
            {
                free_lbuf(mudstate.global_regs[i]);
            }
            mudstate.global_regs[i] = preserve[i];
            mudstate.glob_reg_len[i] = preserve_len[i];
        }
        else
        {
            if (mudstate.global_regs[i])
            {
                mudstate.global_regs[i][0] = '\0';
            }
            mudstate.glob_reg_len[i] = 0;
        }
    }
}
