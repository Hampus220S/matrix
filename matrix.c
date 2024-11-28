/*
 * matrix.c
 *
 * Written by Hampus Fridholm
 *
 * Last updated: 2024-11-28
 */

#include <stdio.h>
#include <stdlib.h>
#include <argp.h>
#include <time.h>
#include <stdbool.h>
#include <ncurses.h>
#include <unistd.h>
#include <pthread.h>

static char doc[] = "matrix - animation inspired by The Matrix";

static char args_doc[] = "";

static struct argp_option options[] =
{
  { "async",  'a', 0,        0, "Asyncronous scroll" },
  { "old",    'o', 0,        0, "Use old style scrolling" },
  { "speed",  's', "NUMBER", 0, "Speed of scrolling" },
  { "typing", 't', 0,        0, "Don't exit on keypress" },
  { "depth",  'd', "NUMBER", 0, "Depth of environment" },
  { 0 }
};

struct args
{
  bool async;
  bool old;
  bool typing;
  int  speed;
  int  depth;
};

struct args args =
{
  .async  = false,
  .old    = false,
  .typing = false,
  .speed  = 4,
  .depth  = 1
};

/*
 * This is the option parsing function used by argp
 */
static error_t opt_parse(int key, char* arg, struct argp_state* state)
{
  struct args* args = state->input;

  int number;

  switch(key)
  {
    case 'a':
      args->async = true;
      break;

    case 'o':
      args->old = true;
      break;

    case 't':
      args->typing = true;
      break;

    case 's':
      if(!arg) argp_usage(state);

      number = atoi(arg);

      if(number >= 1 && number <= 10)
      {
        args->speed = number;
      }
      else argp_usage(state);

      break;

    case 'd':
      if(!arg) argp_usage(state);

      number = atoi(arg);

      if(number >= 1 && number <= 10)
      {
        args->depth = number;
      }
      else argp_usage(state);

      break;

    case ARGP_KEY_ARG:
      break;

    case ARGP_KEY_END:
      break;

    default:
      return ARGP_ERR_UNKNOWN;
  }

  return 0;
}


typedef struct
{
  char* symbols; // Symbols in string
  int   length;  // Length of string
  int   depth;   // Depth in background
  int   speed;   // Speed of falling down
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

/*
 *
 */
static screen_t* screen_create(void)
{
  screen_t* screen = malloc(sizeof(screen_t));

  initscr();
  noecho();
  raw();
  keypad(stdscr, TRUE);

  if(start_color() == ERR || !has_colors() || !can_change_color())
  {
    endwin();

    return NULL;
  }
  
  return screen;
}

/*
 *
 */
static void screen_free(screen_t** screen)
{
  if(!screen) return;

  free(*screen);

  endwin();

  *screen = NULL;
}

screen_t* screen;
bool running;

/*
 *
 */
static void* print_routine(void* arg)
{
  while(running)
  {
    clear();

    int count = rand() % 0xFF;

    mvprintw(2, 2, "count: %d", count);

    curs_set(0);

    refresh();

    usleep(100000);
  }
}

static struct argp argp = { options, opt_parse, args_doc, doc };

/*
 * This is the main function
 */
int main(int argc, char* argv[])
{
  argp_parse(&argp, argc, argv, 0, 0, &args);

  srand(time(NULL));


  screen = screen_create();

  running = true;

  pthread_t thread;

  if(pthread_create(&thread, NULL, print_routine, NULL) != 0)
  {
    perror("Failed to create second thread");

    return 1;
  }

  int key;
  while((key = getch()))
  {
    if(!args.typing) break;
      
    if(key == 'q') break;

    if(key == KEY_RESIZE)
    {
      screen->height = getmaxy(stdscr);
      screen->width  = getmaxx(stdscr);
    }
  }


  running = false;

  // pthread_cancel(thread);

  // Wait for the second thread to finish
  if(pthread_join(thread, NULL) != 0)
  {
    perror("Failed to join second thread");
    return 1;
  }

  screen_free(&screen);

  return 0;
}
