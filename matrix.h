/*
 *
 */

#ifndef MATRIX_H
#define MATRIX_H

#include <stdio.h>
#include <stdlib.h>
#include <argp.h>
#include <time.h>
#include <stdbool.h>
#include <ncurses.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#define RANDOM_VALUE_GET(MIN, MAX) (rand() % (1 + (MAX) - (MIN)) + (MIN))

#define RATIO_VALUE_GET(MIN, MAX, RATIO) ((RATIO) * ((MAX) - (MIN)) + (MIN))

typedef struct
{
  char* symbols; // Symbols in string
  int   length;  // Length of string
  int   depth;   // Depth in background
  int   clock;   // Internal clock
  int   y;
} string_t;

typedef struct
{
  string_t* strings; // Strings in column
  int       count;   // Number of strings
} column_t;

typedef struct
{
  column_t* columns; // Columns
  int       width;   // Width of screen
  int       height;  // Height of screen
} screen_t;

#endif // MATRIX_H
