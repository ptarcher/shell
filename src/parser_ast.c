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

#include <libraries/parser.h>

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

AST_Pipeline_t *AST_ParsePipeline(Parser_t *parser);
AST_List_t *AST_ParseList(Parser_t *parser);
int AST_ProcessList(AST_List_t *pipeline_list);

int AST_ProcessPipeline(AST_Pipeline_t *pipeline);

void AST_FreeAssignment(AST_Assignment_t *assignment);
void AST_FreeRedirect(AST_Redirect_t *redirect);
void AST_FreeCommand(AST_Command_t *command);
void AST_FreeExpression(AST_Expression_t *expression);
void AST_FreeIfPipeline(AST_IfPipeline_t *ifpipeline);
void AST_FreePipeline(AST_Pipeline_t *pipeline);
void AST_FreeList(AST_List_t *pipeline_list);

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

void AST_PrintPipeline(AST_Pipeline_t *pipeline)
{
    if (pipeline->assignment) {
        AST_PrintAssignment(pipeline->assignment);
    } else if (pipeline->expression) {
        AST_PrintExpression(pipeline->expression);
    }

    DTRACE("\n");
}

void AST_PrintList(AST_List_t *pipeline_list)
{
    int i;

    DTRACE("=======================================\n");
    for (i = 0; i < pipeline_list->npipelines; i++) {
        AST_PrintPipeline(pipeline_list->pipelines[i]);
    }
    DTRACE("=======================================\n");
}

/****************************************************************************/

AST_Assignment_t *AST_ParseAssignment(Parser_t *parser, Token_t *var)
{
    AST_Assignment_t *assignment;

    DTRACE("%s: Start\n", __func__);

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
    }

    DTRACE("%s: End\n", __func__);

    return assignment;

assignment_fail:
    if (assignment) {
        if (assignment->var) {
            Scanner_TokenFree(assignment->var);
            assignment->var = NULL;
        }
        free(assignment);
    }

    DTRACE("%s: Fail\n", __func__);

    return NULL;
}

AST_Redirect_t *AST_ParseRedirect(Parser_t *parser)
{
    AST_Redirect_t *redirect = NULL;

    DTRACE("%s: Start\n", __func__);

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

    DTRACE("%s: End\n", __func__);

    return redirect;

redirect_fail:
    if (redirect) {
        if (redirect->file) {
            Scanner_TokenFree(redirect->file);
        }
        free(redirect);
    }

    DTRACE("%s: Fail\n", __func__);

    return NULL;
}

AST_Command_t *AST_ParseCommand(Parser_t *parser, Token_t *cmd)
{
    AST_Command_t *command;
    Token_t **argv;

    DTRACE("%s: Start\n", __func__);

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
            Scanner_TokenConsume(parser);
            break;
        } else {
            break;
        }
    }

    DTRACE("%s: End\n", __func__);

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

    DTRACE("%s: Fail\n", __func__);

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

    while (parser->t->type == TOKEN_NEWLINE) {
        Scanner_TokenConsume(parser);
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

AST_Pipeline_t *AST_ParseExpressionOrAssignment(Parser_t *parser)
{
    AST_Pipeline_t *pipeline;
    Token_t         *cmd_or_var;

    DTRACE("%s: Start\n", __func__);

    if ((pipeline = calloc(1, sizeof(*pipeline))) == NULL) {
        return NULL;
    }

    cmd_or_var = parser->t;
    Scanner_TokenAccept(parser);

    if (parser->t->type == TOKEN_EQUALS) {
        pipeline->assignment = AST_ParseAssignment(parser, cmd_or_var);
    } else {
        pipeline->expression = AST_ParseExpression(parser, cmd_or_var);
    }

    if (parser->t->type == TOKEN_SEMICOLON) {
        Scanner_TokenConsume(parser);
    }

    DTRACE("%s: End\n", __func__);

    return pipeline;
}

AST_Pipeline_t *AST_ParseIfPipeline(Parser_t *parser)
{
    AST_Pipeline_t   *pipeline  = NULL;

    if ((pipeline = calloc(1, sizeof(*pipeline))) == NULL) {
        goto if_fail;
    }

    if ((pipeline->ifpipeline = calloc(1, sizeof(*(pipeline->ifpipeline)))) == NULL) {
        goto if_fail;
    }

    Scanner_TokenConsume(parser);
    pipeline->ifpipeline->test = AST_ParseList(parser);

    if (parser->t->type != TOKEN_THEN) {
        goto if_fail;
    }
    Scanner_TokenConsume(parser);
    pipeline->ifpipeline->pipeline = AST_ParsePipeline(parser);

    if (parser->t->type == TOKEN_ELSE) {
        Scanner_TokenConsume(parser);
        pipeline->ifpipeline->elsepipeline = AST_ParsePipeline(parser);
    }

    if (parser->t->type != TOKEN_FI) {
        goto if_fail;
    }
    Scanner_TokenConsume(parser);

    return pipeline;

if_fail:
    fprintf(stderr, "ERROR: Parsing %s\n", __func__);

    if (pipeline) {
        AST_FreePipeline(pipeline);
    }

    return NULL;
}

AST_Words_t *AST_ParseWords(Parser_t *parser)
{
    AST_Words_t *words = NULL;
    Token_t **w;

    if ((words = calloc(1, sizeof(*words))) == NULL) {
        goto words_fail;
    }

    while (parser->t->type  == TOKEN_STRING ||
            parser->t->type == TOKEN_ID     ||
            parser->t->type == TOKEN_DOLLAR) {
        if ((w = realloc(words->words, sizeof(*words->words)*(words->nwords+1))) == NULL) {
            goto words_fail;
        }
        words->words = w;
        words->words[words->nwords++] = parser->t;
        Scanner_TokenAccept(parser);
    }

    while (parser->t->type == TOKEN_NEWLINE) {
        Scanner_TokenConsume(parser);
    }

    return words;

words_fail:
    if (words) {
        if (words->words) {
            int i;
            for (i = 0; i < words->nwords; i++) {
                Scanner_TokenFree(words->words[i]);
            }
            free(words->words);
        }
        free(words);
    }

    return NULL;
}

AST_Pipeline_t *AST_ParseForPipeline(Parser_t *parser)
{
    AST_Pipeline_t *pipeline = NULL;

    DTRACE("%s: Start\n", __func__);

    if ((pipeline = calloc(1, sizeof(*pipeline))) == NULL) {
        goto for_fail;
    }
    if ((pipeline->forpipeline = calloc(1, sizeof(*(pipeline->forpipeline)))) == NULL) {
        goto for_fail;
    }

    Scanner_TokenConsume(parser);
    pipeline->forpipeline->var = parser->t;
    Scanner_TokenAccept(parser);

    /* Support for loops without the IN token */
    if (parser->t->type == TOKEN_ID && (strcmp(parser->t->str, "in") == 0)) {
        Scanner_TokenConsume(parser);
        pipeline->forpipeline->words = AST_ParseWords(parser);
    }

    if (parser->t->type != TOKEN_DO) {
        fprintf(stderr, "Found wrong token expected %d got %d\n", TOKEN_DO, parser->t->type);
        goto for_fail;
    }
    Scanner_TokenConsume(parser);

    pipeline->forpipeline->list = AST_ParseList(parser);

    if (parser->t->type != TOKEN_DONE) {
        fprintf(stderr, "Found wrong token expected %d got %d\n", TOKEN_DONE, parser->t->type);
        goto for_fail;
    }

    printf("%s: Done\n", __func__);

    return pipeline;

for_fail:
    fprintf(stderr, "ERROR: Parsing %s\n", __func__);

    if (pipeline) {
        AST_FreePipeline(pipeline);
    }

    return NULL;
}

char *tick_ch;

int AST_ParseTickGetChar(struct Parser *parser, int timeout)
{
    (void) timeout;
    int c;
    
    c = *tick_ch++;
    if (c == '\0') {
        return EOF;
    }

    return c;
}

AST_Pipeline_t *AST_ParseTickPipeline(Parser_t *parser)
{
    AST_Pipeline_t *pipeline    = NULL;
    AST_List_t     *tick_pipeline_list = NULL;
    Parser_t        tick_parser;

    DTRACE("%s: Start\n", __func__);

    /* Setup a new pipeline_list to consume the text between the ticks */
    memset(&tick_parser, 0, sizeof(tick_parser));
    tick_parser.linenum = parser->linenum;
    tick_parser.colnum  = parser->colnum;
    tick_ch             = parser->t->str;
    tick_parser.getchar = AST_ParseTickGetChar;

    /* Setup first char, and token */
    tick_parser.c       = tick_parser.getchar(&tick_parser, 1000);
    tick_parser.t       = Scanner_TokenNext(&tick_parser);

    /* Run the tick pipeline_list */
    /* TODO: Set the print path so we can absorb the output */
    if ((tick_pipeline_list = AST_ParseProgram(&tick_parser)) == NULL) {
        goto tick_fail;
    }

    AST_ProcessList(tick_pipeline_list);
    AST_FreeList(tick_pipeline_list);

    /* Consume the whole tick token */
    Scanner_TokenConsume(parser);

    DTRACE("%s: Done\n", __func__);

    return pipeline;

tick_fail:
    /* TODO: */
    fprintf(stderr, "ERROR: Parsing %s\n", __func__);
    if (pipeline) {
        AST_FreePipeline(pipeline);
    }
    return NULL;
}

AST_Pipeline_t *AST_ParseWhilePipeline(Parser_t *parser)
{
    AST_Pipeline_t *pipeline = NULL;

    if ((pipeline = calloc(1, sizeof(*pipeline))) == NULL) {
        goto while_fail;
    }
    if ((pipeline->whilepipeline = calloc(1, sizeof(*(pipeline->whilepipeline)))) == NULL) {
        goto while_fail;
    }

    /* Consume the while */
    Scanner_TokenConsume(parser);

    /* Parse the test */
    if (parser->t->type != TOKEN_DO) {
        pipeline->whilepipeline->test = AST_ParseList(parser);
    }

    /* consume the do */
    if (parser->t->type != TOKEN_DO) {
        fprintf(stderr, "Token Type = %d\n", parser->t->type);
        goto while_fail;
    }
    Scanner_TokenConsume(parser);

    /* Parse the body */
    if (parser->t->type != TOKEN_DONE) {
        pipeline->whilepipeline->list = AST_ParseList(parser);
    }

    /* consume the done */
    if (parser->t->type != TOKEN_DONE) {
        goto while_fail;
    }
    Scanner_TokenConsume(parser);

    return pipeline;

while_fail:
    fprintf(stderr, "ERROR: Parsing %s\n", __func__);
    if (pipeline) {
        AST_FreePipeline(pipeline);
    }
    return NULL;
}

AST_Pipeline_t *AST_ParsePipeline(Parser_t *parser)
{
    AST_Pipeline_t *pipeline;

    DTRACE("%s: Start\n", __func__);

    while (parser->t->type == TOKEN_NEWLINE) {
        Scanner_TokenConsume(parser);
    }

    switch(parser->t->type) {
        case TOKEN_ID:
        case TOKEN_STRING:
            pipeline = AST_ParseExpressionOrAssignment(parser);
            break;

        case TOKEN_IF:
            pipeline = AST_ParseIfPipeline(parser);
            break;

        case TOKEN_FOR:
            pipeline = AST_ParseForPipeline(parser);
            break;

        case TOKEN_TICK:
            pipeline = AST_ParseTickPipeline(parser);
            break;

        case TOKEN_WHILE:
            pipeline = AST_ParseWhilePipeline(parser);
            break;

        default:
            pipeline = NULL;
            break;
    }

    while (parser->t->type == TOKEN_NEWLINE) {
        Scanner_TokenConsume(parser);
    }

    DTRACE("%s: End\n", __func__);

    return pipeline;
}

AST_List_t *AST_ParseList(Parser_t *parser)
{
    AST_List_t *pipeline_list;
    AST_Pipeline_t *pipeline;
    AST_Pipeline_t **pipelines;

    DTRACE("%s: Start\n", __func__);

    if ((pipeline_list = calloc(1, sizeof(*pipeline_list))) == NULL) {
        return NULL;
    }

    while (parser->t->type != TOKEN_EOF) {
        if ((pipeline = AST_ParsePipeline(parser))) {
            pipelines = realloc(pipeline_list->pipelines, 
                    sizeof(*(pipeline_list->pipelines))*(pipeline_list->npipelines+1));
            if (pipelines == NULL) {
                goto pipeline_list_fail;
            } else {
                pipeline_list->pipelines = pipelines;
                pipeline_list->pipelines[pipeline_list->npipelines++] = pipeline;
            }
        } else {
            break;
        }
    }

    DTRACE("%s: End\n", __func__);

    return pipeline_list;

pipeline_list_fail:
    if (pipeline_list) {
        if (pipeline_list->pipelines) {
            free(pipeline_list->pipelines);
        }
        free(pipeline_list);
    }

    return NULL;
}

AST_List_t *AST_ParseProgram(Parser_t *parser)
{
    AST_List_t *pipeline_list;

    pipeline_list = AST_ParseList(parser);

    Scanner_TokenFree(parser->t);
    parser->t = NULL;

    DTRACE("%s: End\n", __func__);

    return pipeline_list;
}

void AST_ParseCleanup(Parser_t *parser)
{
}

/****************************************************************************/
int AST_ProcessAssignment(AST_Assignment_t *assignment)
{
    char *value;
    if (assignment->value) {
        value = assignment->value->str;
    } else {
        value = "";
    }
    my_setenv(assignment->var->str, value, true);

    return 0;
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

int AST_ProcessExpression(AST_Expression_t *expression)
{
    int r = 0;

    if (expression) {
        /* TODO: Convert to a while loop */
        r = AST_ProcessCommand(expression->command);

        if (expression->op) {
            if ((expression->op->type == TOKEN_ANDAND && (r == 0)) ||
                (expression->op->type == TOKEN_OROR   && (r != 0)) ||
                (expression->op->type == TOKEN_SEMICOLON)) {
                r = AST_ProcessExpression(expression->expression);
            } else {
                DTRACE("Ignoring expression due to truth condition\n");
            }
        }
    }

    return r;
}

int AST_ProcessIfPipeline(AST_IfPipeline_t *ifpipeline)
{
    int r = 0;

    if (ifpipeline->test) {
        r = AST_ProcessList(ifpipeline->test);
    }

    if (r) {
        r = AST_ProcessPipeline(ifpipeline->pipeline);
    } else if (ifpipeline->elsepipeline) {
        r = AST_ProcessPipeline(ifpipeline->elsepipeline);
    }

    return r;
}

int AST_ProcessForPipeline(AST_ForPipeline_t *forpipeline)
{
    int i;
    int r = 0;

    if (forpipeline->words) {
        for (i = 0; i < forpipeline->words->nwords; i++) {
            my_setenv(forpipeline->var->str, forpipeline->words->words[i]->str, true);
            r = AST_ProcessList(forpipeline->list);
        }
    } else {
        /* TODO */
    }

    return r;
}

int AST_ProcessTickPipeline(AST_Pipeline_t *tickpipeline)
{
    /* TODO: */
    return 0;
}

int AST_ProcessWhilePipeline(AST_WhilePipeline_t *whilepipeline)
{
    int r;

    //printf("%s: Start\n", __func__);

    while ((whilepipeline->test == NULL) || 
          ((r = AST_ProcessList(whilepipeline->test)) == 0)) {

        if (whilepipeline->list) {
            AST_ProcessList(whilepipeline->list);
        }
    }

    return 0;
}

int AST_ProcessPipeline(AST_Pipeline_t *pipeline)
{
    int r = 0;

    if (pipeline) {
        if (pipeline->assignment) {
            r = AST_ProcessAssignment(pipeline->assignment);
        } 
        if (pipeline->expression) {
            r = AST_ProcessExpression(pipeline->expression);
        }
        if (pipeline->ifpipeline) {
            r = AST_ProcessIfPipeline(pipeline->ifpipeline);
        }
        if (pipeline->forpipeline) {
            r = AST_ProcessForPipeline(pipeline->forpipeline);
        }
        if (pipeline->tickpipeline) {
            r = AST_ProcessTickPipeline(pipeline->tickpipeline);
        }
        if (pipeline->whilepipeline) {
            r = AST_ProcessWhilePipeline(pipeline->whilepipeline);
        }
    }

    return r;
}

int AST_ProcessList(AST_List_t *pipeline_list)
{
    int i;
    int r = 0;

    for (i = 0; i < pipeline_list->npipelines; i++) {
        r = AST_ProcessPipeline(pipeline_list->pipelines[i]);
    }

    return r;
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

void AST_FreeIfPipeline(AST_IfPipeline_t *ifpipeline)
{
    if (ifpipeline->test) {
        AST_FreeList(ifpipeline->test);
    }
    if (ifpipeline->pipeline) {
        AST_FreePipeline(ifpipeline->pipeline);
    }
    if (ifpipeline->elsepipeline) {
        AST_FreePipeline(ifpipeline->elsepipeline);
    }
    free(ifpipeline);
}

void AST_FreeWords(AST_Words_t *words) 
{
    int i;

    if (words->words) {
        for (i = 0; i < words->nwords; i++) {
            Scanner_TokenFree(words->words[i]);
            words->words[i] = NULL;
        }
        free(words->words);
        words->words = NULL;
    }

    free(words);
}

void AST_FreeForPipeline(AST_ForPipeline_t *forpipeline)
{
    if (forpipeline->var) {
        Scanner_TokenFree(forpipeline->var);
    } 
    if (forpipeline->words) {
        AST_FreeWords(forpipeline->words);
    } 
    if (forpipeline->list) {
        AST_FreeList(forpipeline->list);
    } 

    free(forpipeline);
}

void AST_FreeTickPipeline(AST_Pipeline_t *tickpipeline)
{
    free(tickpipeline);
}

void AST_FreeWhilePipeline(AST_WhilePipeline_t *whilepipeline)
{
    if (whilepipeline->test) {
        AST_FreeList(whilepipeline->test);
    }
    if (whilepipeline->list) {
        AST_FreeList(whilepipeline->list);
    }
    free(whilepipeline);
}

void AST_FreePipeline(AST_Pipeline_t *pipeline)
{
    if (pipeline->assignment) {
        AST_FreeAssignment(pipeline->assignment);
    } 
    if (pipeline->expression) {
        AST_FreeExpression(pipeline->expression);
    }
    if (pipeline->ifpipeline) {
        AST_FreeIfPipeline(pipeline->ifpipeline);
    }
    if (pipeline->forpipeline) {
        AST_FreeForPipeline(pipeline->forpipeline);
    }
    if (pipeline->tickpipeline) {
        AST_FreeTickPipeline(pipeline->tickpipeline);
    }
    if (pipeline->whilepipeline) {
        AST_FreeWhilePipeline(pipeline->whilepipeline);
    }
    free(pipeline);
}

void AST_FreeList(AST_List_t *list)
{
    int i;

    for (i = 0; i < list->npipelines; i++) {
        AST_FreePipeline(list->pipelines[i]);
    }
    free(list->pipelines);
    free(list);
}

//------------------------------------------------------------------------------

