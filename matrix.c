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
#include <signal.h>

#define RANDOM_VALUE_GET(MIN, MAX) (rand() % ((MAX) - (MIN)) + (MIN))

#define RATIO_VALUE_GET(MIN, MAX, RATIO) ((RATIO) * ((MAX) - (MIN)) + (MIN))


static char doc[] = "matrix - animation inspired by The Matrix";

static char args_doc[] = "";

static struct argp_option options[] =
{
  { "speed",  's', "NUMBER", 0, "Speed of scrolling" },
  { "depth",  'd', "NUMBER", 0, "Depth of environment" },
  { "length", 'l', "NUMBER", 0, "General length of line" },
  { "async",  'a', 0,        0, "Asyncronous scroll" },
  { "old",    'o', 0,        0, "Use old style scrolling" },
  { "typing", 't', 0,        0, "Don't exit on keypress" },
  { 0 }
};

struct args
{
  int  speed;
  int  depth;
  int  length;
  bool async;
  bool old;
  bool typing;
};

struct args args =
{
  .speed  = 10,
  .depth  = 1,
  .length = 10,
  .async  = false,
  .old    = false,
  .typing = false
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

    case 'l':
      if(!arg) argp_usage(state);

      number = atoi(arg);

      if(number >= 1 && number <= 10)
      {
        args->length = number;
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

/*
 *
 */
static screen_t* screen_create(int width, int height)
{
  screen_t* screen = malloc(sizeof(screen_t));

  if(!screen)
  {
    perror("malloc screen");

    return NULL;
  }

  screen->width  = width;
  screen->height = height;

  screen->columns = malloc(sizeof(column_t) * screen->width);

  if(!screen->columns)
  {
    free(screen);

    perror("malloc columns");

    return NULL;
  }

  for(int index = 0; index < screen->width; index++)
  {
    column_t* column = &screen->columns[index];

    column->strings = NULL;
    column->count   = 0;
  }

  return screen;
}

/*
 *
 */
static void column_free(column_t* column)
{
  for(int index = 0; index < column->count; index++)
  {
    string_t* string = &column->strings[index];

    free(string->symbols);
  }

  free(column->strings);
}

/*
 *
 */
static void screen_free(screen_t** screen)
{
  if(!screen) return;

  for(int index = 0; index < (*screen)->width; index++)
  {
    column_t* column = &(*screen)->columns[index];

    column_free(column);
  }

  free((*screen)->columns);

  free(*screen);

  *screen = NULL;
}

static char symbol_get(void)
{
  return RANDOM_VALUE_GET(33, 126);
  // return RANDOM_VALUE_GET(65, 90);
}

/*
 *
 */
static int string_length_get(int height)
{
  float length_ratio = ((float) args.length / 10.f);

  int max_length = RATIO_VALUE_GET(height / 10, height, length_ratio);

  return RANDOM_VALUE_GET(4, max_length);
}

/*
 *
 */
static string_t string_create(int height)
{
  string_t string;

  string.length = string_length_get(height);


  string.symbols = malloc(sizeof(char) * string.length);

  for(int index = 0; index < string.length; index++)
  {
    string.symbols[index] = symbol_get();
  }


  string.y = -(RANDOM_VALUE_GET(0, height));

  return string;
}

/*
 *
 */
static void string_update(string_t* string)
{
  string->y++;

  // Bubble up symbols
  for(int index = string->length; index-- > 0;)
  {
    string->symbols[index] = string->symbols[index - 1];
  }

  string->symbols[0] = symbol_get();
}

/*
 *
 */
static int string_append(column_t* column, int height)
{
  column->strings = realloc(column->strings, sizeof(string_t) * (column->count + 1));

  if(!column->strings)
  {
    perror("realloc strings\n");

    return 1;
  }

  column->strings[column->count] = string_create(height);

  column->count++;

  return 0;
}

/*
 *
 */
static void column_update(column_t* column, int height)
{
  for(int index = 0; index < column->count; index++)
  {
    string_t* string = &column->strings[index];

    string_update(string);
  }

  if(column->count > 0)
  {
    string_t* string = &column->strings[column->count - 1];

    if(string->y - string->length > 0)
    {
      string_append(column, height);
    }
  }
  else string_append(column, height);
}

/*
 *
 */
static void screen_update(screen_t* screen)
{
  for(int index = 0; index < screen->width; index++)
  {
    column_update(&screen->columns[index], screen->height);
  }
}

/*
 *
 */
static void string_print(string_t* string, int height, int x)
{
  for(int index = 0; index < string->length; index++)
  {
    int y = string->y - index;

    if(!(y >= 0 && y < height)) continue;


    char symbol = string->symbols[index];

    int shade = 2 + ((float) index / (float) string->length) * 6;

    if(index == 0)
    {
      attron(COLOR_PAIR(1));

      mvprintw(y, x, "%c", symbol);

      attroff(COLOR_PAIR(1));
    }
    else
    {
      attron(COLOR_PAIR(shade));

      mvprintw(y, x, "%c", symbol);

      attroff(COLOR_PAIR(shade));
    }

  }
}

/*
 *
 */
static void column_print(column_t* column, int height, int x)
{
  for(int index = 0; index < column->count; index++)
  {
    string_t* string = &column->strings[index];

    string_print(string, height, x);
  }
}

/*
 *
 */
static void screen_print(screen_t* screen)
{
  for(int x = 0; x < screen->width; x++)
  {
    column_t* column = &screen->columns[x];

    column_print(column, screen->height, x);
  }

  attron(COLOR_PAIR(1));

  mvprintw(1, 1, "COUNT: %d", screen->columns[0].count);

  attroff(COLOR_PAIR(1));
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
    erase();

    screen_update(screen);

    screen_print(screen);

    refresh();

    usleep(100000);
  }
}

/*
 *
 */
static int curses_init(void)
{
  initscr();
  noecho();
  curs_set(0);

  if(start_color() == ERR || !has_colors())
  {
    endwin();

    perror("ncurses colors");

    return 1;
  }

  use_default_colors();

  clear();
  refresh();

  init_pair(1, COLOR_WHITE, COLOR_BLACK);

  int shades[6] = {46, 40, 34, 28, 22, 16};

  for(int i = 0; i < 6; i++)
  {
    init_pair(1 + i + 1, shades[i], COLOR_BLACK); 
  }

  return 0;
}

/*
 *
 */
static void curses_free(void)
{
  endwin();
}

static struct argp argp = { options, opt_parse, args_doc, doc };

/*
 * This is the main function
 */
int main(int argc, char* argv[])
{
  argp_parse(&argp, argc, argv, 0, 0, &args);

  srand(time(NULL));


  if(curses_init() != 0)
  {
    perror("init curses");

    return 1;
  }


  int height = getmaxy(stdscr);
  int width  = getmaxx(stdscr);

  screen = screen_create(width, height);

  running = true;

  pthread_t thread;

  if(pthread_create(&thread, NULL, print_routine, NULL) != 0)
  {
    curses_free();

    screen_free(&screen);

    perror("Failed to create second thread");

    return 2;
  }

  int key;
  while((key = getch()))
  {
    if(key == KEY_RESIZE)
    {
      height = getmaxy(stdscr);
      width  = getmaxx(stdscr);

      screen_free(&screen);

      screen = screen_create(width, height);
    }
    else
    {
      if(!args.typing) break;
        
      if(key == 'q') break;
    }
  }


  running = false;

  // pthread_cancel(thread);

  // Wait for the second thread to finish
  if(pthread_join(thread, NULL) != 0)
  {
    curses_free();

    screen_free(&screen);

    perror("Failed to join second thread");

    return 3;
  }

  screen_free(&screen);

  curses_free();

  return 0;
}
