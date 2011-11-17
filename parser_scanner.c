//
//  Filename:       parser_scanner.c  
//  Description:    
//  
//  Author:         Paul Archer
//  Creation Date:  November, 2011
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>

#include "parser.h"

/*****************************************************************************
 *                              T Y P E S
 ****************************************************************************/

/*****************************************************************************
 *                 F U N C T I O N     P R O T O T Y P E S
 ****************************************************************************/
int my_setenv(char *name, char *value, int overwrite);
char *my_getenv(char *name);


/*****************************************************************************
 *                     G L O B A L     V A R I A B L E S
 ****************************************************************************/

/****************************************************************************/
char *_DEBUG_TokenToString(int t) 
{
    switch(t) {
        case TOKEN_EOF:        return "TOKEN_EOF";

        case TOKEN_NUMBER:     return "TOKEN_NUMBER";
        case TOKEN_ID:         return "TOKEN_ID";
        case TOKEN_DOLLAR:     return "TOKEN_DOLLAR";
        case TOKEN_STRING:     return "TOKEN_STRING";

        case TOKEN_AND:        return "TOKEN_AND";
        case TOKEN_ANDAND:     return "TOKEN_ANDAND";
        case TOKEN_OROR:       return "TOKEN_OROR";
        case TOKEN_LEFTARROW:  return "TOKEN_LEFTARROW";
        case TOKEN_RIGHTARROW: return "TOKEN_RIGHTARROW";
        case TOKEN_EQUALS:     return "TOKEN_EQUALS";
        case TOKEN_SEMICOLON:  return "TOKEN_SEMICOLON";

        case TOKEN_ERROR:      return "TOKEN_ERROR";
        default:               return "TOKEN_UNKNOWN";
    }
}

int iswordchar(char c)
{
    return ((isalnum(c)) || (c == '_') || (c == '.') || (c == '/'));
}

int iscontrolchar(char c)
{
    return ((c == '&') || (c == '>') || (c == '<') || (c == '$') || (c == '|'));
}


char Scanner_Inspect(Parser_t *parser, int n)
{
    return parser->LineChar[n];
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
    parser->c = *(++parser->LineChar);

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

        case '0' ... '9':
            Scanner_ScanNumber(parser);
            type = TOKEN_NUMBER;
            break;

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
            char *value;

            Scanner_Accept(parser, IGNORE_CHAR);
            Scanner_ScanWord(parser);
            type = TOKEN_ID;
            value = my_getenv(parser->token);
            if (value) {
                strcpy(parser->token, value);
            } else {
                strcpy(parser->token, "");
            }
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
                Scanner_ScanWord(parser);
                type = TOKEN_ID;
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
        printf("%3d: t = %-20s token = '%s' %d:%d %c\n", 
                token->linenum, _DEBUG_TokenToString(token->type), 
                token->str, token->linenum, token->colnum, parser->c);
    } else {
        printf("%3d: t = %-20s token = '%s'\n", 
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

