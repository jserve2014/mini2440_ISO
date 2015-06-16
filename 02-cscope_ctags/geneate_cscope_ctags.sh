#!/bin/sh

find . -name "*.h" -o -name "*.c" -o -name "*.cpp" > cscope.out
cscope -bkq -i cscope.out
ctags -R

