/*
 * matrix.c - animation inspired by The Matrix
 *
 * Written by Hampus Fridholm
 *
 * Last updated: 2025-02-15
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
  int       width;   // Width  of screen
  int       height;  // Height of screen
} screen_t;

#define INPUT_DELAY 500000

#define MIN_DELAY 10000
#define MAX_DELAY 100000

#define MIN_LENGTH 4
#define MAX_LENGTH 30

const int COLOR_COUNT = 7;
const int DEPTH_COUNT = 6;

const int COLOR_CODES[] = {
  15,  46, 40, 34, 28, 22, 16,
  255, 41, 35, 29, 23, 17, 16,
  251, 36, 30, 24, 18, 17, 16,
  247, 31, 25, 19, 18, 17, 16,
  243, 26, 20, 19, 18, 17, 16,
  239, 21, 20, 19, 18, 17, 16
};

screen_t* screen;

bool is_running;


static char doc[] = "matrix - animation inspired by The Matrix";

static char args_doc[] = "";

static struct argp_option options[] =
{
  { "speed",  's', "NUMBER", 0, "Speed of scrolling     (1-10)" },
  { "depth",  'd', "NUMBER", 0, "Depth of environment   (0-5)" },
  { "length", 'l', "NUMBER", 0, "General length of line (1-10)" },
  { "air",    'a', "NUMBER", 0, "Air between strings    (1-10)" },
  { "typing", 't', 0,        0, "Don't exit on keypress"        },
  { 0 }
};

struct args
{
  int  speed;
  int  depth;
  int  length;
  int  air;
  bool typing;
};

struct args args =
{
  .speed  = 5,
  .depth  = 0,
  .length = 5,
  .air    = 5,
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

    case 'a':
      if(!arg) argp_usage(state);

      number = atoi(arg);

      if(number >= 1 && number <= 10)
      {
        args->air = number;
      }
      else argp_usage(state);

      break;

    case 'd':
      if(!arg) argp_usage(state);

      number = atoi(arg);

      if(number >= 0 && number < DEPTH_COUNT)
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

/*
 * Create the screen
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
 * Free a string (symbol array)
 */
static void string_free(string_t* string)
{
  free(string->symbols);
}

/*
 * Free a column (and every string inside)
 */
static void column_free(column_t* column)
{
  for(int index = 0; index < column->count; index++)
  {
    string_t* string = &column->strings[index];

    string_free(string);
  }

  free(column->strings);
}

/*
 * Free screen (and every column inside)
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

/*
 * Get a new random symbol for a string
 *
 * The symbol can be between ASCII 33 and 126
 */
static char symbol_get(void)
{
  return RANDOM_VALUE_GET(33, 126);
}

/*
 * Generate a random length for a string
 *
 * The length is dependent on:
 * 1. the average length (args.length)
 * 2. the depth
 *
 * Strings in deeper depths (5, 4, ...) are shorter
 *
 * That should simulate them being further away
 */
static int length_gen(int depth)
{
  float length_ratio = (float) args.length / 10.f;

  float depth_ratio = (float) (DEPTH_COUNT - depth) / (float) DEPTH_COUNT;

  float ratio = length_ratio * depth_ratio;


  int max_length = RATIO_VALUE_GET(MIN_LENGTH, MAX_LENGTH, ratio);

  return RANDOM_VALUE_GET(MIN_LENGTH, max_length);
}

/*
 * Generate a random y position for a string
 *
 * The y position is dependent on:
 * 1. the air constant (how much air between strings)
 * 2. the depth
 *
 * Strings in deeper depths (5, 4, ...) have less distance between them
 *
 * The thought is that the distance between 
 * strings are the same on every depth,
 * but perceived as smaller further away
 */
static int y_gen(int depth)
{
  float air_ratio = (float) args.air / 10.f;

  float depth_ratio = (float) (DEPTH_COUNT - depth) / (float) DEPTH_COUNT;

  float ratio = air_ratio * depth_ratio;


  int max_y = RATIO_VALUE_GET(MAX_LENGTH, MAX_LENGTH * 6, ratio);
  
  return -(RANDOM_VALUE_GET(0, max_y));
}

/*
 * Generate depth for string
 *
 * The deeper values are less likely
 *
 * Cumulative Weight Search
 *
 * depth 0 is 6 times more likely to be picked than depth 5
 *
 * depth 1 is 5 times ... than depth 5
 *
 * depth 4 is 2 times ... than depth 5
 */
static int depth_gen(void)
{
  // 1. Calculate the total sum of weights
  int total_weight = 0;

  for(int depth = 0; depth <= args.depth; depth++)
  {
    total_weight += (args.depth - depth);
  }

  // 2. Pick a random weight
  int rand_weight = RANDOM_VALUE_GET(0, total_weight);


  // 3. Find the index based on the random weight
  int cumulative_weight = 0;

  for(int depth = 0; depth < args.depth; depth++)
  {
    cumulative_weight += (args.depth - depth);

    if(cumulative_weight > rand_weight) return depth;
  }

  return args.depth;
}

/*
 * Create a string, with:
 * - random depth
 * - random length
 * - random start height (y position)
 *
 * Generate random symbols for the string
 */
static int string_init(string_t* string)
{
  string->depth  = depth_gen();

  string->length = length_gen(string->depth);

  string->y      = y_gen(string->depth);
  
  string->clock  = 0;


  string->symbols = malloc(sizeof(char) * string->length);

  if(!string->symbols)
  {
    perror("malloc symbols");

    return 1;
  }

  // Initialize random symbols
  for(int index = 0; index < string->length; index++)
  {
    string->symbols[index] = symbol_get();
  }

  return 0;
}

/*
 * Update the string, when the clock cycle is right
 *
 * 1. Scroll the string down one step
 * 2. Cycle the symbols up one step
 *
 * That way, the symbols look like they are fixed in place
 */
static void string_update(string_t* string)
{
  string->clock = (string->clock + 1) % (string->depth + 1);

  if(string->clock != 0) return; 
  

  string->y++;

  for(int index = string->length; index-- > 1;)
  {
    string->symbols[index] = string->symbols[index - 1];
  }

  string->symbols[0] = symbol_get();
}

/*
 * Append string to column
 *
 * Allocate more memory and add string to array
 */
static int string_append(column_t* column)
{
  string_t new_string;

  if(string_init(&new_string) != 0)
  {
    perror("string init");

    return 1;
  }

  string_t* temp_strings = realloc(column->strings, sizeof(string_t) * (column->count + 1));

  if(!temp_strings)
  {
    perror("realloc strings");

    return 2;
  }

  column->strings = temp_strings;

  column->strings[column->count] = new_string;

  column->count++;

  return 0;
}

/*
 * Remove string from column
 *
 * Shift the other strings to fill the left over space
 *
 * Reallocate with less memory (free the previous memory)
 */
static int string_remove(column_t* column)
{
  if(column->count <= 0) return 1;

  // 1. Free the first (and oldest) string
  string_free(&column->strings[0]);

  // 2. Shift the rest of the strings to take its place
  for(int index = 0; index < (column->count - 1); index++)
  {
    column->strings[index] = column->strings[index + 1];
  }

  // 3. Free the old memory and shrink the array
  string_t* temp_strings = realloc(column->strings, sizeof(string_t) * (column->count - 1));

  if(!temp_strings)
  {
    perror("realloc strings");

    return 2;
  }

  column->strings = temp_strings;

  column->count--;

  return 0;
}

/*
 * Update the column (every string in column)
 *
 * - Add string if no strings exists
 * - Add string if previous string is fully visable
 * - Remove string if it is not visable
 */
static int column_update(column_t* column, int height)
{
  // 1. Update each string (scroll)
  for(int index = 0; index < column->count; index++)
  {
    string_t* string = &column->strings[index];

    string_update(string);
  }

  // 2. If strings exists
  if(column->count > 0)
  {
    // Append new string, if last string is fully visable
    string_t* last_string = &column->strings[column->count - 1];

    if(last_string->y - last_string->length > 0)
    {
      if(string_append(column) != 0)
      {
        return 1;
      }
    }

    // Remove oldest string, if it is not visable
    string_t* first_string = &column->strings[0];

    if(first_string->y - first_string->length >= height)
    {
      if(string_remove(column) != 0)
      {
        return 2;
      }
    }
  }
  else // If no strings exists, append one
  {
    if(string_append(column) != 0)
    {
      return 1;
    }
  }

  return 0;
}

/*
 * Update screen (every column in screen)
 */
static int screen_update(screen_t* screen)
{
  for(int index = 0; index < screen->width; index++)
  {
    if(column_update(&screen->columns[index], screen->height) != 0)
    {
      return 1;
    }
  }

  return 0;
}

/*
 * Get the color for a symbol in string
 *
 * Strings on a deeper level is more blue than
 * the green strings closer to the screen
 *
 * The first symbol of a string is bright white
 */
static int color_get(int depth, int index, int length)
{
  if(index == 0) return 1 + depth * COLOR_COUNT;


  float ratio = (float) index / (float) length;

  int depth_index = RATIO_VALUE_GET(1, COLOR_COUNT - 2, ratio);

  return (1 + depth * COLOR_COUNT + depth_index);
}

/*
 * Print a string (every symbol in string)
 */
static void string_print(string_t* string, int height, int x)
{
  for(int index = 0; index < string->length; index++)
  {
    int y = string->y - index;

    if(!(y >= 0 && y < height)) continue;


    char symbol = string->symbols[index];

    int color = color_get(string->depth, index, string->length);


    attron(COLOR_PAIR(color));

    mvprintw(y, x, "%c", symbol);

    attroff(COLOR_PAIR(color));
  }
}

/*
 * Print a column (every string in column)
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
 * Print screen (every column in screen)
 */
static void screen_print(screen_t* screen)
{
  for(int x = 0; x < screen->width; x++)
  {
    column_t* column = &screen->columns[x];

    column_print(column, screen->height, x);
  }
}

/*
 * Get base delay of animation
 *
 * The base delay can be thought of as one "tick"
 *
 * Strings in depth=0 update every tick
 *
 * Strings in depth=5 update every 6 ticks
 *
 * The internal clock in each string keeps track of the ticks
 */
static unsigned int delay_get(void)
{
  float ratio = (float) (10 - args.speed) / 10.f;

  return RATIO_VALUE_GET(MIN_DELAY, MAX_DELAY, ratio);
}

/*
 * The print routine (output routine)
 *
 * The matrix animation takes place simultaneously 
 * as the user can input keystrokes
 *
 * Every iteration is a tick of the animation,
 * where the strings are updated and printed
 */
static void* print_routine(void* arg)
{
  unsigned int delay = delay_get();

  while(is_running)
  {
    erase();

    if(screen_update(screen) != 0)
    {
      perror("screen update");

      break;
    }

    screen_print(screen);

    refresh();

    usleep(delay);
  }

  return NULL;
}

/*
 * Init the colors for the strings in the different depths 
 *
 * Strings in depths 1, 2 are more green
 *
 * Strings in deeper depths are more blue
 *
 * Blue is ment to symbolize that the strings is further away,
 * like mountains look blue in the sky
 */
static void colors_init(void)
{
  for(int index = 0; index < DEPTH_COUNT * COLOR_COUNT; index++)
  {
    int color = COLOR_CODES[index];

    init_pair(index + 1, color, -1);
  }
}

/*
 * Init the ncurses library and screen
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

  colors_init();

  clear();
  refresh();

  return 0;
}

/*
 * Close the ncurses library and restore the terminal
 */
static void curses_free(void)
{
  clear();

  refresh();

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

  is_running = true;


  pthread_t thread;

  if(pthread_create(&thread, NULL, print_routine, NULL) != 0)
  {
    curses_free();

    screen_free(&screen);

    perror("Failed to create second thread");

    return 2;
  }

  int key;
  while((key = getch()) != ERR)
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

    usleep(INPUT_DELAY);

    flushinp(); // Flush input buffer
  }

  is_running = false;

  // Wait for the second thread to finish
  // pthread_cancel(thread);

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
