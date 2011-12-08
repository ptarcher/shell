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

    TOKEN_IF     = 20,
    TOKEN_THEN,
    TOKEN_ELSE,
    TOKEN_ELIF,
    TOKEN_FI,
    TOKEN_FOR,
    TOKEN_IN,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_WHILE,
    TOKEN_DO,
    TOKEN_DONE,

    TOKEN_AND = 40,
    TOKEN_ANDAND,
    TOKEN_OROR,
    TOKEN_LEFTARROW,
    TOKEN_RIGHTARROW,
    TOKEN_EQUALS,
    TOKEN_SEMICOLON,
    TOKEN_TICK,
    TOKEN_PIPE,
    TOKEN_NEWLINE,

    TOKEN_ERROR = 100,
} Token_Type_t;

typedef struct {
    Token_Type_t type;
    char        *str;
    bool         control;

    int linenum;
    int colnum;
} Token_t;

typedef struct Parser {
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
    int  (*getchar)(struct Parser *parser, int timeout);

    /* Parser Context */
    Token_t *t;
    bool token_control;
} Parser_t;

/* Abstract Syntax Tree */
struct AST_Pipeline;
struct AST_List;

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
    struct AST_List      *test;
    struct AST_Pipeline *pipeline;
    struct AST_Pipeline *elsepipeline;
} AST_IfPipeline_t;;

typedef struct AST_Words {
    Token_t **words;
    int       nwords;
} AST_Words_t;

typedef struct {
    Token_t         *var;
    AST_Words_t     *words;
    struct AST_List *list;
} AST_ForPipeline_t;

typedef struct {
    struct AST_List   *test;
    struct AST_List   *list;
} AST_WhilePipeline_t;

typedef struct AST_Pipeline {
    AST_Assignment_t     *assignment;
    AST_Expression_t     *expression;
    AST_IfPipeline_t    *ifpipeline;
    AST_ForPipeline_t   *forpipeline;
    AST_WhilePipeline_t *whilepipeline;
    struct AST_Pipeline *tickpipeline;
} AST_Pipeline_t;

typedef struct AST_List {
    AST_Pipeline_t  **pipelines;
    int npipelines;
} AST_List_t;

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
AST_List_t *AST_ParseProgram(Parser_t *parser);
void AST_PrintList(AST_List_t *pipeline_list);
int  AST_ProcessList(AST_List_t *pipeline_list);
void AST_FreeList(AST_List_t *pipeline_list);

#define AST_PrintProgram   AST_PrintList
#define AST_ProcessProgram AST_ProcessList
#define AST_FreeProgram    AST_FreeList

#endif /* _PARSER_H_ */

//------------------------------------------------------------------------------
