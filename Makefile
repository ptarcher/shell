.PHONY: default
default: shell.c parser_ast.c parser_scanner.c parser.h
	gcc -Wall -O -g shell.c parser_ast.c parser_scanner.c -o shell

.PHONY: tags
tags: 
	@ ctags -R
