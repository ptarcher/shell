#!/bin/sh
me="hello"
./cmd arg1 arg2 &
./cmd arg1 arg2 > file_out < file_in
./cmd arg1 arg2 > file_out < file_in && cmd2
./cmd arg1 arg2 || cmd2 ${me}
./cmd arg1 arg2; cmd2 ${me}
cmd1; you=mine; cmd2 ${you}
