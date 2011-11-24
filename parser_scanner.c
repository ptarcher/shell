//
//  Filename:       parser_scanner.c  
//  Description:    
//  
//  Author:         Paul Archer
//  Creation Date:  November, 2011
//

#define USE_DTRACE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>

#include "parser.h"

#if USE_DTRACE
#define DTRACE printf
#else
#define DTRACE(...)
#endif /* USE_DTRACE */

/*****************************************************************************
 *                              T Y P E S
 ****************************************************************************/

/*****************************************************************************
 *                 F U N C T I O N     P R O T O T Y P E S
 ****************************************************************************/
int my_setenv(char *name, char *value, int overwrite);
char *my_getenv(char *name);

#define countof(a) (sizeof(a)/sizeof(*a))

/*****************************************************************************
 *                     G L O B A L     V A R I A B L E S
 ****************************************************************************/
typedef struct {
    const char        *keyword;
    const Token_Type_t type;
} Scanner_Keyword_t;

/****************************************************************************/
char *_DEBUG_TokenToString(int t) 
{
    switch(t) {
        case TOKEN_EOF:        return "TOKEN_EOF";

        case TOKEN_NUMBER:     return "TOKEN_NUMBER";
        case TOKEN_ID:         return "TOKEN_ID";
        case TOKEN_DOLLAR:     return "TOKEN_DOLLAR";
        case TOKEN_STRING:     return "TOKEN_STRING";

        case TOKEN_IF:         return "TOKEN_IF";
        case TOKEN_THEN:       return "TOKEN_THEN";
        case TOKEN_ELSE:       return "TOKEN_ELSE";
        case TOKEN_FI:         return "TOKEN_FI";
        case TOKEN_FOR:        return "TOKEN_FOR";
        case TOKEN_IN:         return "TOKEN_IN";
        case TOKEN_WHILE:      return "TOKEN_WHILE";
        case TOKEN_DO:         return "TOKEN_DO";
        case TOKEN_DONE:       return "TOKEN_DONE";

        case TOKEN_AND:        return "TOKEN_AND";
        case TOKEN_ANDAND:     return "TOKEN_ANDAND";
        case TOKEN_OROR:       return "TOKEN_OROR";
        case TOKEN_LEFTARROW:  return "TOKEN_LEFTARROW";
        case TOKEN_RIGHTARROW: return "TOKEN_RIGHTARROW";
        case TOKEN_EQUALS:     return "TOKEN_EQUALS";
        case TOKEN_SEMICOLON:  return "TOKEN_SEMICOLON";
        case TOKEN_TICK:       return "TOKEN_TICK";

        case TOKEN_ERROR:      return "TOKEN_ERROR";
        default:               return "TOKEN_UNKNOWN";
    }
}

int iswordchar(char c)
{
    return ((isalnum(c)) || 
            (c == '_')   || 
            (c == '-')   ||
            (c == '.')   || 
            (c == '/')   ||
            (c == '?')   ||
            (c == '!')   ||
            (c == '@')   ||
            (c == '#')   ||
            (c == '%')   ||
            (c == '^')   ||
            (c == '&')   ||
            (c == '(')   ||
            (c == ')')   ||
            (c == '[')   ||
            (c == ']')   ||
            (c == '+'));
}

int iscontrolchar(char c)
{
    return ((c == '&') || (c == '>') || (c == '<') || (c == '$') || (c == '|'));
}


char Scanner_Inspect(Parser_t *parser, int n)
{
    if (n >= strlen(parser->line)) {
        int i;
        for (i = 1; i <= n; i++) {
            parser->line[i] = parser->getchar(parser, 1000);
        }
    } 

    return parser->line[n];
}

void Scanner_TokenAppend(Parser_t *parser, char c)
{
    parser->token[parser->token_idx++] = c;
}

char Scanner_Accept(Parser_t *parser, bool store_char)
{
    if (store_char == STORE_CHAR) {
        /* Store the character */
        Scanner_TokenAppend(parser, parser->c);
    }

    /* Grab the next one */
    if (strlen(parser->line) == 0) {
        parser->line[0] = parser->getchar(parser, 1000);
    }

    /* Move all the characters up one */
    parser->c = parser->line[0];
    memmove(parser->line, &(parser->line[1]), sizeof(parser->line)-1);
    parser->line[sizeof(parser->line)-1] = 0;

    /* Update line and column numbers */
    if (((parser->c == '\r') && Scanner_Inspect(parser, 1) != '\n') || 
            (parser->c == '\n')) {
        parser->linenum++;
        parser->colnum = 1;
    } else {
        parser->colnum++;
    }

    return parser->c;
}

void Scanner_ScanNumber(Parser_t *parser)
{
    while (isdigit(parser->c)) {
        Scanner_Accept(parser, STORE_CHAR);
    }
}

void Scanner_ScanWord(Parser_t *parser)
{
    while (iswordchar(parser->c)) {
        Scanner_Accept(parser, STORE_CHAR);
    }
}

void Scanner_SkipComments(Parser_t *parser)
{
    while (isspace(parser->c) || parser->c == '#') {
        /* Skip any leading whitespace */
        while (isspace(parser->c)) {
            Scanner_Accept(parser, IGNORE_CHAR);
        }

        /* Skip any comments */
        if (parser->c == '#') {
            /* Find the end of line */
            while ((parser->c != '\n') && (parser->c != '\0')) {
                Scanner_Accept(parser, IGNORE_CHAR);
            }
        }
    }
}

void Scanner_TokenFree(Token_t *t) 
{
    if (t) {
        if (t->str) {
            free(t->str);
        }
        free(t);
    }
}

Token_t *Scanner_TokenNext(Parser_t *parser)
{
    Token_t *token = NULL;
    Token_Type_t type;

    const Scanner_Keyword_t keywords[] = {
        {"if",      TOKEN_IF},
        {"then",    TOKEN_THEN},
        {"else",    TOKEN_ELSE},
        {"fi",      TOKEN_FI},
        {"for",     TOKEN_FOR},
        {"in",      TOKEN_IN},
        {"while",   TOKEN_WHILE},
        {"do",      TOKEN_DO},
        {"done",    TOKEN_DONE},
    };

    Scanner_SkipComments(parser);

    parser->token_startline = parser->linenum;
    parser->token_startcol  = parser->colnum;
    memset(parser->token, 0, sizeof(parser->token));
    parser->token_idx = 0;

    switch (parser->c) {
        case '&':
            Scanner_Accept(parser, STORE_CHAR);
            if (parser->c == '&') {
                Scanner_Accept(parser, STORE_CHAR);
                type = TOKEN_ANDAND;
            } else {
                type = TOKEN_AND;
            }
            break;

        case '|':
            if (Scanner_Inspect(parser, 1) == '|') {
                Scanner_Accept(parser, STORE_CHAR);
                Scanner_Accept(parser, STORE_CHAR);
                type = TOKEN_OROR;
            } else {
                type = TOKEN_ERROR;
            }
            break;

#if 0
        case '0' ... '9':
            Scanner_ScanNumber(parser);
            type = TOKEN_NUMBER;
            break;
#endif

        case '>':
            Scanner_Accept(parser, STORE_CHAR);
            type = TOKEN_RIGHTARROW;
            break;

        case '<':
            Scanner_Accept(parser, STORE_CHAR);
            type = TOKEN_LEFTARROW;
            break;

        case '$':
        {
            int require_bracket = 0;

            Scanner_Accept(parser, IGNORE_CHAR);

            if (parser->c == '{') {
                require_bracket = true;
                Scanner_Accept(parser, IGNORE_CHAR);
            }

            Scanner_ScanWord(parser);

            if (require_bracket) {
                if (parser->c != '}') {
                    type = TOKEN_ERROR;
                    break;
                }
                Scanner_Accept(parser, IGNORE_CHAR);
            } 

            type = TOKEN_DOLLAR;
            break;
        }

        case '=':
            Scanner_Accept(parser, STORE_CHAR);
            type = TOKEN_EQUALS;
            break;

        case ';':
            Scanner_Accept(parser, STORE_CHAR);
            type = TOKEN_SEMICOLON;
            break;

        case EOF:
        case 0:
            type = TOKEN_EOF;
            break;

        case '"':
            Scanner_Accept(parser, IGNORE_CHAR);
            do {
                if (parser->c == STRING_ESC_CHAR) {
                    if (Scanner_Inspect(parser, 1) == STRING_ESC_CHAR) {
                        Scanner_Accept(parser, IGNORE_CHAR);
                        Scanner_Accept(parser, IGNORE_CHAR);
                        Scanner_TokenAppend(parser, STRING_ESC_CHAR);
                    } else if (Scanner_Inspect(parser, 1) == '"') {
                        Scanner_Accept(parser, IGNORE_CHAR);
                        Scanner_Accept(parser, IGNORE_CHAR);
                        Scanner_TokenAppend(parser, '"');
                    } else if (Scanner_Inspect(parser, 1) == 'n') {
                        Scanner_Accept(parser, IGNORE_CHAR);
                        Scanner_Accept(parser, IGNORE_CHAR);
                        Scanner_TokenAppend(parser, '\n');
                    } else if (Scanner_Inspect(parser, 1) == 'r') {
                        Scanner_Accept(parser, IGNORE_CHAR);
                        Scanner_Accept(parser, IGNORE_CHAR);
                        Scanner_TokenAppend(parser, '\r');
                    } else if (Scanner_Inspect(parser, 1) == 't') {
                        Scanner_Accept(parser, IGNORE_CHAR);
                        Scanner_Accept(parser, IGNORE_CHAR);
                        Scanner_TokenAppend(parser, '\t');
                    } else if (Scanner_Inspect(parser, 1) == 'b') {
                        Scanner_Accept(parser, IGNORE_CHAR);
                        Scanner_Accept(parser, IGNORE_CHAR);
                        Scanner_TokenAppend(parser, '\b');
                    } else {
                        /* Just accept it */
                        Scanner_Accept(parser, STORE_CHAR);
                    }
                } else if (parser->c == '"') {
                    /* Found the end of the string */
                    Scanner_Accept(parser, IGNORE_CHAR);
                    break;
                } else {
                    Scanner_Accept(parser, STORE_CHAR);
                }
            } while (parser->c != 0);

            type = TOKEN_STRING;
            break;

        case '`':
            Scanner_Accept(parser, IGNORE_CHAR);

            while (parser->c != '`') {
                Scanner_Accept(parser, STORE_CHAR);
            }

            Scanner_Accept(parser, IGNORE_CHAR);
            type = TOKEN_TICK;
            break;

        case '\'':
            Scanner_Accept(parser, IGNORE_CHAR);
            do {
                if (parser->c == STRING_ESC_CHAR) {
                    if (Scanner_Inspect(parser, 1) == STRING_ESC_CHAR) {
                        Scanner_Accept(parser, IGNORE_CHAR);
                        Scanner_Accept(parser, STORE_CHAR);
                    } else if (Scanner_Inspect(parser, 1) == '\'') {
                        Scanner_Accept(parser, IGNORE_CHAR);
                        Scanner_Accept(parser, STORE_CHAR);
                    } else {
                        /* Just accept it */
                        Scanner_Accept(parser, STORE_CHAR);
                    }
                } else if (parser->c == '\'') {
                    /* Found the end of the string */
                    Scanner_Accept(parser, IGNORE_CHAR);
                    break;
                } else {
                    Scanner_Accept(parser, STORE_CHAR);
                }
            } while (parser->c != 0);

            type = TOKEN_STRING;
            break;

        default:
            if (iswordchar(parser->c)) {
                int i;

                Scanner_ScanWord(parser);
                type = TOKEN_ID;

                /* Search to see if it is a keyword */
                for (i = 0; i < countof(keywords); i++) {
                    if (strcmp(parser->token, keywords[i].keyword) == 0) {
                        type =  keywords[i].type;
                        break;
                    }
                }

            } else {
                type = TOKEN_ERROR;
            }
            break;
    }

    /* Allocate the token */
    if ((token = calloc(1, sizeof(*token))) == NULL) {
        goto fail;
    }

    /* Take a copy of the string */
    if (((token->str = strdup(parser->token))) == NULL) {
        goto fail;
    }
    token->type = type;
    token->linenum = parser->token_startline;
    token->colnum  = parser->token_startcol;

    /* Debug Print */
    if (token->type == TOKEN_ERROR) {
        DTRACE("%3d: t = %-20s token = '%s' %d:%d %c\n", 
                token->linenum, _DEBUG_TokenToString(token->type), 
                token->str, token->linenum, token->colnum, parser->c);
    } else {
        DTRACE("%3d: t = %-20s token = '%s'\n", 
                token->linenum, _DEBUG_TokenToString(token->type), 
                token->str);
    }

    return token;

fail:
    if (token) {
        if (token->str) {
            free(token->str);
        }
        free(token);
    }

    return NULL;
}

void Scanner_TokenAccept(Parser_t *parser)
{
    parser->t = Scanner_TokenNext(parser);
}

void Scanner_TokenConsume(Parser_t *parser)
{
    Scanner_TokenFree(parser->t);
    Scanner_TokenAccept(parser);
}

//------------------------------------------------------------------------------

