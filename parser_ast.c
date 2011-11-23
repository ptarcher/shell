//
//  Filename:       parser_ast.c  
//  Description:    
//  
//  Author:         Paul Archer
//  Creation Date:  November, 2011
//

#define USE_DTRACE 0

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

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
int Shell_RunCommand(int argc, char *argv[], bool background);

AST_Statement_t *AST_ParseStatement(Parser_t *parser);

void AST_FreeAssignment(AST_Assignment_t *assignment);
void AST_FreeRedirect(AST_Redirect_t *redirect);
void AST_FreeCommand(AST_Command_t *command);
void AST_FreeExpression(AST_Expression_t *expression);
void AST_FreeStatement(AST_Statement_t *statement);
void AST_FreeProgram(AST_Program_t *program);

/*****************************************************************************
 *                     G L O B A L     V A R I A B L E S
 ****************************************************************************/

/****************************************************************************/
void AST_PrintAssignment(AST_Assignment_t *assignment)
{
    DTRACE("AST_Assignment: %s = %s\n", assignment->var->str, assignment->value->str);
}

void AST_PrintCommand(AST_Command_t *command)
{
    int i;

    for (i = 0; i < command->argc; i++) {
        DTRACE("%s ", command->argv[i]->str);
    }
    if (command->in) {
        DTRACE("< %s ", command->in->file->str);
    }
    if (command->out) {
        DTRACE("> %s ", command->out->file->str);
    }
    if (command->background) {
        DTRACE("&");
    }
}

void AST_PrintExpression(AST_Expression_t *expression)
{
    AST_Expression_t *e;

    for (e = expression; e; e = e->expression) {
        if (e->command) {
            AST_PrintCommand(e->command);
        }
        if (e->op) {
            DTRACE("%s ", e->op->str);
        }
    }
}

void AST_PrintStatement(AST_Statement_t *statement)
{
    if (statement->assignment) {
        AST_PrintAssignment(statement->assignment);
    } else if (statement->expression) {
        AST_PrintExpression(statement->expression);
    }

    DTRACE("\n");
}

void AST_PrintProgram(AST_Program_t *program)
{
    int i;

    DTRACE("=======================================\n");
    for (i = 0; i < program->nstatements; i++) {
        AST_PrintStatement(program->statements[i]);
    }
    DTRACE("=======================================\n");
}

/****************************************************************************/

AST_Assignment_t *AST_ParseAssignment(Parser_t *parser, Token_t *var)
{
    AST_Assignment_t *assignment;
    if ((assignment = calloc(1, sizeof(*assignment))) == NULL) {
        goto assignment_fail;
    }
    assignment->var   = var;

    /* Variable */
    Scanner_TokenConsume(parser); /* the assignemnt op */
    if (parser->t->type == TOKEN_STRING || 
            parser->t->type == TOKEN_ID ||
            parser->t->type == TOKEN_DOLLAR) {
        assignment->value = parser->t;
        Scanner_TokenAccept(parser);
    } else {
        /* ERROR */
        goto assignment_fail;
    }

    return assignment;

assignment_fail:
    if (assignment) {
        if (assignment->var) {
            Scanner_TokenFree(assignment->var);
            assignment->var = NULL;
        }
        free(assignment);
    }
    return NULL;
}

AST_Redirect_t *AST_ParseRedirect(Parser_t *parser)
{
    AST_Redirect_t *redirect = NULL;
    if ((redirect = calloc(1, sizeof(*redirect))) == NULL) {
        goto redirect_fail;
    }

    /* Consume the direction */
    Scanner_TokenConsume(parser);

    if (parser->t->type == TOKEN_ID ||
        parser->t->type == TOKEN_STRING ||
        parser->t->type == TOKEN_DOLLAR)
    {
        redirect->file = parser->t;
        Scanner_TokenAccept(parser);
    } else {
        goto redirect_fail;
    }

    return redirect;

redirect_fail:
    if (redirect) {
        return redirect;
    }
    return NULL;
}

AST_Command_t *AST_ParseCommand(Parser_t *parser, Token_t *cmd)
{
    AST_Command_t *command;
    Token_t **argv;

    if ((command = calloc(1, sizeof(*command))) == NULL) {
        goto command_fail;
    }

    argv = realloc(command->argv, sizeof(*(command->argv))*(command->argc+1));
    command->argv = argv;
    command->argv[command->argc++] = cmd;

    /* Command */
    while (parser->t->type != TOKEN_EOF) {
        if (parser->t->type == TOKEN_STRING || 
            parser->t->type == TOKEN_ID     ||
            parser->t->type == TOKEN_DOLLAR) {

            if ((argv = realloc(command->argv, sizeof(*(command->argv))*(command->argc+1))) == NULL) {
                goto command_fail;
            }
            
            command->argv = argv;
            command->argv[command->argc++] = parser->t;

            Scanner_TokenAccept(parser);
        } else if (parser->t->type == TOKEN_LEFTARROW)  {
            command->in = AST_ParseRedirect(parser);
        } else if (parser->t->type == TOKEN_RIGHTARROW) {
            command->out = AST_ParseRedirect(parser);
        } else if (parser->t->type == TOKEN_AND)        {
            command->background = true;
            break;
        } else {
            break;
        }
    }

    return command;

command_fail:
    if (command) {
        if (command->argv) {
            int i;
            for (i = 0; i < command->argc; i++) {
                free(command->argv[i]);
                command->argv[i] = NULL;
            }
            free(command->argv);
            command->argv = NULL;
        }
        if (command->in) {
            AST_FreeRedirect(command->in);
            command->in = NULL;
        }
        if (command->out) {
            AST_FreeRedirect(command->out);
            command->out = NULL;
        }
        free(command);
        command = NULL;
    }

    return NULL;
}

AST_Expression_t *AST_ParseExpression(Parser_t *parser, Token_t *cmd)
{
    AST_Expression_t *expression = NULL;

    if (cmd == NULL) {
        cmd = parser->t;
        Scanner_TokenAccept(parser);
    }

    /* TODO: Convert this into a while loop */
    if ((expression = calloc(1, sizeof(*expression))) == NULL) {
        goto expression_fail;
    }

    expression->command = AST_ParseCommand(parser, cmd);
    if ((parser->t->type == TOKEN_OROR) || (parser->t->type == TOKEN_ANDAND)) {
        /* Consume the || or && */
        expression->op = parser->t;
        Scanner_TokenAccept(parser);

        /* Parse Expression expects the ID to already be accepted */
        if (parser->t->type != TOKEN_ID     && 
                parser->t->type != TOKEN_STRING &&
                parser->t->type != TOKEN_DOLLAR) {
            goto expression_fail;
        }

        expression->expression = AST_ParseExpression(parser, NULL);
    }

    return expression;

expression_fail:
    {
        AST_Expression_t *e;
        AST_Expression_t *next;
        for (e = expression; e; e = next) {
            next = e->expression;
            
            if (e->op) {
                Scanner_TokenFree(e->op);
            }
            free(e);
            e = next;
        }
    }
    return NULL;
}

AST_Statement_t *AST_ParseExpressionOrAssignment(Parser_t *parser)
{
    AST_Statement_t *statement;
    Token_t         *cmd_or_var;

    if ((statement = calloc(1, sizeof(*statement))) == NULL) {
        return NULL;
    }

    cmd_or_var = parser->t;
    Scanner_TokenAccept(parser);

    if (parser->t->type == TOKEN_EQUALS) {
        statement->assignment = AST_ParseAssignment(parser, cmd_or_var);
    } else {
        statement->expression = AST_ParseExpression(parser, cmd_or_var);
    }

    if (parser->t->type == TOKEN_SEMICOLON) {
        Scanner_TokenConsume(parser);
    }

    return statement;
}

AST_Statement_t *AST_ParseIfStatement(Parser_t *parser)
{
    AST_Statement_t   *statement  = NULL;

    if ((statement = calloc(1, sizeof(*statement))) == NULL) {
        return NULL;
    }

    if ((statement->ifstatement = calloc(1, sizeof(*(statement->ifstatement)))) == NULL) {
        /* TODO: Fail */
        return NULL;
    }

    Scanner_TokenConsume(parser);
    statement->ifstatement->expression = AST_ParseExpression(parser, NULL);

    if (parser->t->type != TOKEN_THEN) {
        goto if_fail;
    }
    Scanner_TokenConsume(parser);

    if (parser->t->type == TOKEN_ELSE) {
        statement->ifstatement->elsestatement = AST_ParseIfStatement(parser);
    }

    if (parser->t->type != TOKEN_FI) {
        goto if_fail;
    }
    Scanner_TokenConsume(parser);

    return statement;

if_fail:
    /* TODO: Fail */

    return NULL;
}

AST_Statement_t *AST_ParseForStatement(Parser_t *parser)
{
    AST_Statement_t *statement = NULL;

    printf("Found FOR statement\n");

    if ((statement = calloc(1, sizeof(*statement))) == NULL) {
        goto for_fail;
    }
    if ((statement->forstatement = calloc(1, sizeof(*(statement->forstatement)))) == NULL) {
        goto for_fail;
    }

    Scanner_TokenConsume(parser);
    statement->forstatement->var = parser->t;
    Scanner_TokenAccept(parser);

    if (parser->t->type != TOKEN_IN) {
        fprintf(stderr, "Found wrong token expected %d got %d\n", TOKEN_IN, parser->t->type);
        goto for_fail;
    }
    Scanner_TokenConsume(parser);

    statement->forstatement->statement = AST_ParseStatement(parser);

    if (parser->t->type != TOKEN_DO) {
        fprintf(stderr, "Found wrong token expected %d got %d\n", TOKEN_DO, parser->t->type);
        goto for_fail;
    }

    statement->forstatement->statement = AST_ParseStatement(parser);

    if (parser->t->type != TOKEN_DONE) {
        fprintf(stderr, "Found wrong token expected %d got %d\n", TOKEN_DONE, parser->t->type);
        goto for_fail;
    }

for_fail:
    fprintf(stderr, "ERROR: Parsing %s\n", __func__);
    if (statement) {
        AST_FreeStatement(statement);
    }

    return statement;
}

AST_Statement_t *AST_ParseTickStatement(Parser_t *parser)
{
    AST_Statement_t *statement = NULL;

    printf("Found TICK statement\n");

    Scanner_TokenConsume(parser);

    if ((statement = calloc(1, sizeof(*statement))) == NULL) {
        goto tick_fail;
    }
    if ((statement->tickstatement = calloc(1, sizeof(*(statement->tickstatement)))) == NULL) {
        goto tick_fail;
    }

    statement->tickstatement = AST_ParseStatement(parser);

    if (parser->t->type != TOKEN_TICK) {
        goto tick_fail;
    }

    Scanner_TokenConsume(parser);

    return statement;

tick_fail:
    fprintf(stderr, "ERROR: Parsing %s\n", __func__);
    if (statement) {
        AST_FreeStatement(statement);
    }
    return NULL;
}

AST_Statement_t *AST_ParseWhileStatement(Parser_t *parser)
{
    AST_Statement_t *statement = NULL;

    printf("Found WHILE statement\n");
    goto while_fail;
    return NULL;

while_fail:
    fprintf(stderr, "ERROR: Parsing %s\n", __func__);
    if (statement) {
        AST_FreeStatement(statement);
    }
    return NULL;
}

AST_Statement_t *AST_ParseStatement(Parser_t *parser)
{
    AST_Statement_t *statement;

    switch(parser->t->type) {
        case TOKEN_ID:
        case TOKEN_STRING:
            statement = AST_ParseExpressionOrAssignment(parser);
            break;

        case TOKEN_IF:
            statement = AST_ParseIfStatement(parser);
            break;

        case TOKEN_FOR:
            statement = AST_ParseForStatement(parser);
            break;

        case TOKEN_TICK:
            statement = AST_ParseTickStatement(parser);
            break;

        case TOKEN_WHILE:
            statement = AST_ParseWhileStatement(parser);
            break;

        default:
            statement = NULL;
            break;
    }

    return statement;
}

AST_Program_t *AST_ParseProgram(Parser_t *parser)
{
    AST_Program_t *program;
    AST_Statement_t *statement;
    AST_Statement_t **statements;

    if ((program = calloc(1, sizeof(*program))) == NULL) {
        return NULL;
    }

    while (parser->t->type != TOKEN_EOF) {
        if ((statement = AST_ParseStatement(parser))) {
            statements = realloc(program->statements, 
                    sizeof(*(program->statements))*(program->nstatements+1));
            if (statements == NULL) {
                goto program_fail;
            } else {
                program->statements = statements;
                program->statements[program->nstatements++] = statement;
            }
        } else {
            break;
        }
    }

    Scanner_TokenFree(parser->t);
    parser->t = NULL;

    return program;

program_fail:
    if (program) {
        if (program->statements) {
            free(program->statements);
        }
        free(program);
    }

    Scanner_TokenFree(parser->t);
    parser->t = NULL;

    return NULL;
}

void AST_ParseCleanup(Parser_t *parser)
{
}

/****************************************************************************/
void AST_ProcessAssignment(AST_Assignment_t *assignment)
{
    my_setenv(assignment->var->str, assignment->value->str, true);
}

int AST_ProcessCommand(AST_Command_t *command) 
{
    int argc = command->argc;
    char **argv;
    int i;
    int r;
    char r_str[5];

    if ((argv = calloc(argc, sizeof(*argv))) == NULL) {
        goto process_command_fail;
    }

    /* Create the command arguments */
    for (i = 0; i < argc; i++) {
        char *value;

        if (command->argv[i]->type == TOKEN_DOLLAR) {
            if ((value = my_getenv(command->argv[i]->str)) == NULL) {
                value = "";
            }
        } else {
            value = command->argv[i]->str;
        }

        /* Take a copy of the string */
        if ((argv[i] = strdup(value)) == NULL) {
            goto process_command_fail;
        }
    }

    r = Shell_RunCommand(argc, argv, command->background);
    snprintf(r_str, sizeof(r_str), "%d", r);
    my_setenv("?", r_str, true);

    return r;

process_command_fail:
    if (argv) {
        for (i = 0; i < argc; i++) {
            if (argv[i]) {
                free(argv[i]);
            }
        }
        free(argv);
    }

    return -ENOMEM;
}

void AST_ProcessExpression(AST_Expression_t *expression)
{
    int r;

    r = AST_ProcessCommand(expression->command);

    if (expression->op) {
        if ((expression->op->type == TOKEN_ANDAND && (r == 0)) ||
            (expression->op->type == TOKEN_OROR   && (r != 0)) ||
            (expression->op->type == TOKEN_SEMICOLON)) {
            AST_ProcessExpression(expression->expression);
        } else {
            DTRACE("Ignoring expression due to truth condition\n");
        }
    }
}

void AST_ProcessStatement(AST_Statement_t *statement)
{
    if (statement->assignment) {
        AST_ProcessAssignment(statement->assignment);
    } 
    if (statement->expression) {
        AST_ProcessExpression(statement->expression);
    }
}

void AST_ProcessProgram(AST_Program_t *program)
{
    int i;

    for (i = 0; i < program->nstatements; i++) {
        AST_ProcessStatement(program->statements[i]);
    }
}

/****************************************************************************/

void AST_FreeAssignment(AST_Assignment_t *assignment)
{
    if (assignment->var) {
        Scanner_TokenFree(assignment->var);
        assignment->var = NULL;
    }

    if (assignment->value) {
        Scanner_TokenFree(assignment->value);
        assignment->value = NULL;
    }
    free(assignment);
    assignment = NULL;
}

void AST_FreeRedirect(AST_Redirect_t *redirect)
{
    Scanner_TokenFree(redirect->file);
    free(redirect);
}

void AST_FreeCommand(AST_Command_t *command)
{
    if (command->argv) {
        int i;
        for (i = 0; i < command->argc; i++) {
            Scanner_TokenFree(command->argv[i]);
        }
        free(command->argv);
    }

    if (command->in) {
        AST_FreeRedirect(command->in);
    }
    if (command->out) {
        AST_FreeRedirect(command->out);
    }

    free(command);
}

void AST_FreeExpression(AST_Expression_t *expression)
{
    AST_Expression_t *e;
    AST_Expression_t *next;

    for (e = expression; e; e = next) {
        next = e->expression;

        if (e->command) {
            AST_FreeCommand(e->command);
        }
        if (e->op) {
            Scanner_TokenFree(e->op);
        }

        free(e);
    }
}

void AST_FreeStatement(AST_Statement_t *statement)
{
    if (statement->assignment) {
        AST_FreeAssignment(statement->assignment);
    } 
    if (statement->expression) {
        AST_FreeExpression(statement->expression);
    }
    free(statement);
}

void AST_FreeProgram(AST_Program_t *program)
{
    int i;

    for (i = 0; i < program->nstatements; i++) {
        AST_FreeStatement(program->statements[i]);
    }
    free(program->statements);
    free(program);
}

//------------------------------------------------------------------------------

