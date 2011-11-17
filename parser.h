//
//  Filename:       parser.h  
//  Description:    Shell Parser
//  
//  Author:         Paul Archer
//  Creation Date:  November, 2011
//
//  Licence: BSD
//

#ifndef _PARSER_H_
#define _PARSER_H_

#include <stdbool.h>

#define MAX_ARGS 10

typedef enum {
    TOKEN_EOF,

    TOKEN_NUMBER = 10,
    TOKEN_ID,
    TOKEN_DOLLAR,
    TOKEN_STRING,

    TOKEN_AND = 20,
    TOKEN_ANDAND,
    TOKEN_OROR,
    TOKEN_LEFTARROW,
    TOKEN_RIGHTARROW,
    TOKEN_EQUALS,
    TOKEN_SEMICOLON,

    TOKEN_ERROR = 100,
} Token_Type_t;

typedef struct {
    Token_Type_t type;
    char        *str;

    int linenum;
    int colnum;
} Token_t;

typedef struct {
    int linenum;
    int colnum;

    /* Scanner Context */
    char c;
    char *LineChar;
    char line[1024];

    int  token_startline;
    int  token_startcol;
    char token[1024];
    int  token_idx;

    /* Parser Context */
    Token_t *t;
} Parser_t;

/* Abstract Syntax Tree */
typedef struct {
    Token_t *file;
} AST_Redirect_t;

typedef struct {
    int   argc;
    Token_t **argv;

    bool background;
    AST_Redirect_t *in;
    AST_Redirect_t *out;
} AST_Command_t;

typedef struct {
    Token_t *var;
} AST_Variable_t;

typedef struct {
    Token_t *var;
    Token_t *value;
} AST_Assignment_t;

typedef struct AST_Expression {
    AST_Command_t         *command;
    Token_t               *op;
    struct AST_Expression *expression;
} AST_Expression_t;

typedef struct {
    AST_Assignment_t *assignment;
    AST_Expression_t *expression;
} AST_Statement_t;

typedef struct {
    AST_Statement_t  **statements;
    int nstatements;
} AST_Program_t;

#define STRING_ESC_CHAR '\\'

enum {
    STORE_CHAR,
    IGNORE_CHAR,
};


/* Scanner Functions */
Token_t *Scanner_TokenNext(Parser_t *parser);
void Scanner_TokenConsume(Parser_t *parser);
void Scanner_TokenAccept(Parser_t *parser);
void Scanner_TokenFree(Token_t *t);

/* AST Functions */
AST_Program_t *AST_ParseProgram(Parser_t *parser);
void AST_PrintProgram(AST_Program_t *program);
void AST_ProcessProgram(AST_Program_t *program);
void AST_FreeProgram(AST_Program_t *program);

#endif /* _PARSER_H_ */

//------------------------------------------------------------------------------
