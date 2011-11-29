//
//  Filename:       shell_parser.c  
//  Description:    
//  
//  Author:         Paul Archer
//  Creation Date:  November, 2011
//

#define USE_DTRACE 0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>
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
void env_cleanup(void)
{
    size_t i;
    for (i = 0; i < MAX_ENVS; i++) {
        if (env[i].name) {
            free(env[i].name);
            free(env[i].value);
            env[i].name  = NULL;
            env[i].value = NULL;
        }
    }
}

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
            break;
        }
    }

    return 0;
}

char *my_getenv(char *name)
{
    size_t i;

    for (i = 0; i < MAX_ENVS; i++) {
        if (env[i].name && (strcmp(env[i].name, name) == 0)) {
            return env[i].value;
        }
    }

    DTRACE("Unable to find %s\n", name);

    return NULL;
}

int Command_Test(int argc, char *argv[])
{
    if (argc > 3 && (strcmp(argv[1], "-n") == 0)) {
        /* Check for empty string */
        return (argv[2][0] != 0);
    } else if (argc == 2) {
        /* Check for empty string */
        return (argv[1][0] != 0);
    }

    return 0;
}

int Command_Echo(int argc, char *argv[])
{
    int i;

    for (i = 1; i < argc; i++) {
        printf("%s%s", argv[i], (i + 1 < argc) ? " " : "");
    }
    printf("\n");

    return 0;
}

int Command_Seq(int argc, char *argv[])
{

    if (argc == 3) {
        int low, high;
        int i;

        low  = strtol(argv[1], NULL, 0);
        high = strtol(argv[2], NULL, 0);

        for (i = low; i <= high; i++) {
            printf("%d\n", i);
        }

        printf("\n");
    }

    return 0;
}

int Command_True(int argc, char *argv[])
{
    return 0;
}

int Command_False(int argc, char *argv[])
{
    return 1;
}


int Shell_RunCommand(int argc, char *argv[], bool background)
{
    int i;
    int r;

    DTRACE("Running command ");
    for (i = 0; i < argc; i++) {
        DTRACE("'%s' ", argv[i]);
    }
    DTRACE("%s\n", background ? "in the background" : "");
    DTRACE("\n");

    if (strcmp(argv[0], "[") == 0) {
        r = Command_Test(argc, argv);
    } else if (strcmp(argv[0], "echo") == 0) {
        r = Command_Echo(argc, argv);
    } else if (strcmp(argv[0], "seq") == 0) {
        r = Command_Seq(argc, argv);
    } else if (strcmp(argv[0], "true") == 0) {
        r = Command_True(argc, argv);
    } else if (strcmp(argv[0], "false") == 0) {
        r = Command_False(argc, argv);
    } else {
        r = 0;
    }

    /* Clean up */
    for (i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);

    return r;
}

void Shell_ParseInput(Parser_t *parser)
{
    AST_Program_t *program;

    /* Reset the line */
    parser->c        = parser->getchar(parser, 1000);

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

FILE *f;
int Shell_FileGetChar(struct Parser *parser, int timeout)
{
    (void) timeout;
    return fgetc(f);
}

void Shell_ParseFile(char *file) 
{
    Parser_t parser;
    memset(&parser, 0, sizeof(parser));

    f = fopen(file, "r");
    if (f == NULL) {
        return;
    }

    parser.linenum  = 1;
    parser.colnum   = 1;
    parser.getchar  = Shell_FileGetChar;

    //while (fgets(parser.line, sizeof(parser.line), f)) {
    //    DTRACE("line = %s\n", parser.line);
        Shell_ParseInput(&parser);
    //}

    fclose(f);
}

int main(int argc, char *argv[]) 
{
    if (argc == 1) {
        Shell_ParseLine();
    } else {
        Shell_ParseFile(argv[1]);
    }
    env_cleanup();

	return 0;
}

//------------------------------------------------------------------------------

