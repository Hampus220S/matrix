/*
 * matrix.c
 *
 * Written by Hampus Fridholm
 *
 * Last updated: 2024-11-30
 */

#include "matrix.h"

#define INPUT_DELAY 500000

#define MIN_DELAY 10000
#define MAX_DELAY 100000

#define MIN_LENGTH 4
#define MAX_LENGTH 30

const int COLOR_COUNT = 7;
const int DEPTH_COUNT = 6;

const int COLOR_CODES[] = {
  15, 46, 40, 34, 28, 22, 16,
  15, 41, 35, 29, 23, 17, 16,
  15, 36, 30, 24, 18, 17, 16,
  15, 31, 25, 19, 18, 17, 16,
  15, 26, 20, 19, 18, 17, 16,
  15, 21, 20, 19, 18, 17, 16
};

screen_t* screen;

bool running;


static char doc[] = "matrix - animation inspired by The Matrix";

static char args_doc[] = "";

static struct argp_option options[] =
{
  { "speed",  's', "NUMBER", 0, "Speed of scrolling" },
  { "depth",  'd', "NUMBER", 0, "Depth of environment" },
  { "length", 'l', "NUMBER", 0, "General length of line" },
  { "air",    'a', "NUMBER", 0, "Air between strings" },
  { "typing", 't', 0,        0, "Don't exit on keypress" },
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
}

/*
 * Maybe: Remove height and have constant max_height, independent of height
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
 *
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
 *
 */
static int depth_gen(void)
{
  int weights[args.depth + 1];

  for (int i = 0; i <= args.depth; i++)
  {
    weights[i] = args.depth - i;
  }

  // Calculate the total sum of weights
  int total_weight = 0;

  for (int i = 0; i <= args.depth; i++)
  {
    total_weight += weights[i];
  }

  // Generate a random number between 0 and total_weight - 1
  int rand_weight = rand() % total_weight;

  // Find the index based on the random weight
  int cumulative_weight = 0;

  for (int i = 0; i <= args.depth; i++)
  {
    cumulative_weight += weights[i];

    if (rand_weight < cumulative_weight)
    {
      return i;
    }
  }

  // Fallback (though this should never be reached)
  return 0;
}

/*
 *
 */
static string_t string_create(void)
{
  string_t string = { 0 };

  string.depth = depth_gen();

  string.length = length_gen(string.depth);

  string.y = y_gen(string.depth);


  string.symbols = malloc(sizeof(char) * string.length);

  for(int index = 0; index < string.length; index++)
  {
    string.symbols[index] = symbol_get();
  }

  return string;
}

/*
 *
 */
static void string_update(string_t* string)
{
  string->clock = (string->clock + 1) % (string->depth + 1);

  if(string->clock != 0) return; 
  

  string->y++;

  for(int index = string->length; index-- > 0;)
  {
    string->symbols[index] = string->symbols[index - 1];
  }

  string->symbols[0] = symbol_get();
}

/*
 *
 */
static int string_append(column_t* column)
{
  column->strings = realloc(column->strings, sizeof(string_t) * (column->count + 1));

  if(!column->strings)
  {
    perror("realloc strings\n");

    return 1;
  }

  column->strings[column->count] = string_create();

  column->count++;

  return 0;
}

/*
 *
 */
static int string_remove(column_t* column)
{
  if(column->count <= 0) return 1;

  for(int index = 0; index < (column->count - 1); index++)
  {
    column->strings[index] = column->strings[index + 1];
  }

  column->strings = realloc(column->strings, sizeof(string_t) * (column->count - 1));

  if(!column->strings)
  {
    perror("realloc strings\n");

    return 2;
  }

  column->count--;

  return 0;
}

/*
 *
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
 *
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
 *
 */
static int color_get(int depth, int index, int length)
{
  if(index == 0) return 1 + depth * COLOR_COUNT;


  float ratio = (float) index / (float) length;

  int depth_index = RATIO_VALUE_GET(1, COLOR_COUNT - 2, ratio);

  return (1 + depth * COLOR_COUNT + depth_index);
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

    int color = color_get(string->depth, index, string->length);


    attron(COLOR_PAIR(color));

    mvprintw(y, x, "%c", symbol);

    attroff(COLOR_PAIR(color));
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
}

/*
 *
 */
static unsigned int delay_get(void)
{
  float ratio = (float) (10 - args.speed) / 10.f;

  return RATIO_VALUE_GET(MIN_DELAY, MAX_DELAY, ratio);
}

/*
 *
 */
static void* print_routine(void* arg)
{
  unsigned int delay = delay_get();

  while(running)
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
 *
 */
static void colors_init(void)
{
  for(int index = 0; index < DEPTH_COUNT * COLOR_COUNT; index++)
  {
    int color = COLOR_CODES[index];

    init_pair(index + 1, color, COLOR_BLACK);
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

  colors_init();

  clear();
  refresh();

  return 0;
}

/*
 *
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
