/*
 * =====================================================================================
 *
 *       Filename:  c.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  25/02/21 15:05:29
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Ali (mn), 
 *        Company:  FH Südwestfalen, Iserlohn
 *
 * =====================================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define dprint(expr) printf(#expr " = %d", expr);


int main(int argc, const char **argv){
    char *test = "does it work?";
    char into[10];
    int i = 0;
    while(test[++i] != ' ');
    strncpy(into, test, i);
    into[i] = '\0';
    printf("is it? %s|\n", into);
    test+=i;
    i = 0;
    while(test[++i] != ' ');
    strncpy(into, test, i);
    printf("is it? %s|", into);

}

