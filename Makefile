.PHONY: default
default: src/shell.c src/parser_ast.c src/parser_scanner.c include/libraries/parser.h
	gcc -Wall -O -g -I include src/shell.c src/parser_ast.c src/parser_scanner.c -o shell

.PHONY: tags
tags: 
	@ ctags -R
