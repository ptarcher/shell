//
//  Filename:       shell_parser.c  
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
#include <errno.h>

#include "parser.h"

/*****************************************************************************
 *                              T Y P E S
 ****************************************************************************/
typedef struct {
    char *name;
    char *value;
} Env_t;

#define MAX_ENVS 16

/*****************************************************************************
 *                 F U N C T I O N     P R O T O T Y P E S
 ****************************************************************************/

/*****************************************************************************
 *                     G L O B A L     V A R I A B L E S
 ****************************************************************************/
Env_t env[MAX_ENVS];

/****************************************************************************/
int my_setenv(char *name, char *value, int overwrite)
{
    size_t i;

    /* First search for the name and see if we need to replace it */
    for (i = 0; i < MAX_ENVS; i++) {
        if (env[i].name && strcmp(env[i].name, name) == 0) {
            if (overwrite) {
                char *v;

                if ((v = strdup(value)) == NULL) {
                    return -ENOMEM;
                }

                free(env[i].value);
                env[i].value = v;
                return 0;
            } else {
                /* We found it, but don't wont to overwrite */
                return 0;
            }
        }
    }

    /* No entry has been found, just find the first empty spot */
    for (i = 0; i < MAX_ENVS; i++) {
        if ((env[i].name == NULL)) {
            if ((env[i].name  = strdup(name)) == NULL) {
                return -ENOMEM;
            }

            if ((env[i].value = strdup(value)) == NULL) {
                free(env[i].name);
                env[i].name = NULL;
                return -ENOMEM;
            }
        }
    }

    return 0;
}

char *my_getenv(char *name)
{
    size_t i;

    for (i = 0; i < MAX_ENVS; i++) {
        if (strcmp(env[i].name, name) == 0) {
            return env[i].value;
        }
    }

    return NULL;
}

int Shell_RunCommand(int argc, char *argv[], bool background)
{
    int i;

    printf("Running command %s with %d args %s\n", argv[0], argc, background ? "in the background" : "");

    /* Clean up */
    for (i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);

    return 0;
}

void Shell_ParseInput(Parser_t *parser)
{
    AST_Program_t *program;

    /* Reset the line */
    parser->LineChar =  parser->line;
    parser->c        = *parser->LineChar;

    /* Setup the first token */
    if ((parser->t = Scanner_TokenNext(parser)) == NULL) {
        fprintf(stderr, "No memory");
        return;
    }

    /* Start building the AST */
    program = AST_ParseProgram(parser);
    AST_PrintProgram(program);
    AST_ProcessProgram(program);
    AST_FreeProgram(program);
}

void Shell_ParseLine(void) 
{
    Parser_t parser;
    memset(&parser, 0, sizeof(parser));
    char *prompt = ">";

    printf("%s ", prompt);
    while (fgets(parser.line, sizeof(parser.line), stdin)) {
        parser.linenum  = 1;
        parser.colnum   = 1;

        Shell_ParseInput(&parser);
        printf("%s ", prompt);
    }
}

void Shell_ParseFile(char *file) 
{
    FILE *f;
    Parser_t parser;

    f = fopen(file, "r");
    if (f == NULL) {
        return;
    }

    parser.linenum  = 1;
    parser.colnum   = 1;

    while (fgets(parser.line, sizeof(parser.line), f)) {
        printf("line = %s\n", parser.line);
        Shell_ParseInput(&parser);
    }
}

int main(int argc, char *argv[]) 
{
    if (argc == 1) {
        Shell_ParseLine();
    } else {
        Shell_ParseFile(argv[1]);
    }
	return 0;
}

//------------------------------------------------------------------------------

