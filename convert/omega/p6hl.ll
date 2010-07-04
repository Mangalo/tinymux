%{
#include "omega.h"
#include "p6hgame.h"
#include "p6hl.tab.hpp"
%}

%option 8bit 
%option noyywrap
%option prefix="p6hl"

%x str
%%

\#-?[0-9]+     {
                   p6hllval.i = atoi(p6hltext+1);
                   return DBREF;
               }
\#FALSE        {
                   return BOOLFALSE;
               }
\#TRUE         {
                   return BOOLTRUE;
               }
\=             {
                   return '=';
               }
\+             {
                   return '+';
               }
\@             {
                   return '@';
               }
\$             {
                   return '$';
               }
\&             {
                   return '&';
               }
\|             {
                   return '|';
               }
\!             {
                   return '!';
               }
\:             {
                   return ':';
               }
\/             {
                   return '/';
               }
\^             {
                   return '^';
               }
\(             {
                   return '(';
               }
\)             {
                   return ')';
               }
[^()=+@$&|!:/\n\t ]+  {
                   p6hllval.p = StringClone(p6hltext);
                   return LTEXT;
               }
[\n\t ]+       /* ignore whitespace */ ;
.              { return EOF; }
%%

extern P6H_LOCKEXP *g_p6hKeyExp;
int p6hlparse();

P6H_LOCKEXP *p6hl_ParseKey(char *pKey)
{
    //extern int p6hl_flex_debug;
    //extern int p6hldebug;
    //p6hl_flex_debug = 1;
    //p6hldebug = 1;

    delete g_p6hKeyExp;
    g_p6hKeyExp = NULL;
    
    YY_BUFFER_STATE bp = p6hl_scan_string(pKey);
    p6hl_switch_to_buffer(bp);
    P6H_LOCKEXP *ple = NULL;
    if (p6hlparse())
    {
        delete g_p6hKeyExp;
    }
    else
    {
        ple = g_p6hKeyExp;
    }
    g_p6hKeyExp = NULL;
    return ple;
}
