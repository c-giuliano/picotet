/*
 * picotet
 * made by c 2023
 */

#include <ncurses.h>
#include <stdlib.h>
#include <time.h>

typedef uint16_t tet_t;
typedef uint32_t score_t;
typedef uint8_t coord_t;
typedef uint8_t rot_t;

/* Config macros. */
#define TET_CH '@'
#define BORDER_CH '|'
#define BLANK_CH ' '
#define BOARD_WIDTH 20
#define BOARD_HEIGHT 20
#define BORDER_WIDTH 2
#define HUD_OFFSET 2
#define SCORE_INC 347

/* Helper macros. */
#define LEN(ar) (sizeof((ar))/sizeof((ar)[0]))
#define LAST_OF(ar) (ar)[LEN((ar))-1]
#define BIT_SIZE(x) (sizeof(x) * 8)
#define TET_I(i) (i * 4)

#define X_START (BORDER_WIDTH + (BOARD_WIDTH / 2) - (TET_W / 2))
#define SCORE_X (BOARD_WIDTH + 2 * BORDER_WIDTH + HUD_OFFSET)
#define SCORE_Y BOARD_HEIGHT
#define TET_W 4
#define TET_H 4

typedef struct {
  coord_t drop_x;         /* Current tetromino horizontal position */
  coord_t drop_y;         /* Current tetromino vertical position */
  rot_t ru;             /* Rotation units (0ru to 3ru) */
  score_t score;
  int tets_queue[4];  /* Queue of upcoming tetrominos */
} State;

enum actions {
  ACTION_DROP,
  ACTION_INSTANT_DROP,
  ACTION_ROTATE,
  ACTION_MOVE_LEFT,
  ACTION_MOVE_RIGHT,
};

const tet_t tets[] = {
/*
  T tetromino sprites
*/
  0x2700, /* 0ru */
  0x2620, /* 1ru */
  0x720,  /* 2ru */
  0x2320, /* 3ru */

/*
  L tetromino sprites
*/
  0x1700, /* 0ru */
  0x6220, /* 1ru */
  0x740,  /* 2ru */
  0x2230, /* 3ru */

/*
  J tetromino sprites
*/
  0x4700, /* 0ru */
  0x2260, /* 1ru */
  0x710,  /* 2ru */
  0x3220, /* 3ru */

/*
  O tetromino sprites
*/
  0x6600, /* 0ru */
  0x6600, /* 1ru */
  0x6600, /* 2ru */
  0x6600, /* 3ru */

/*
  I tetromino sprites
*/
  0xf00,  /* 0ru */
  0x4444, /* 1ru */
  0xf00,  /* 2ru */
  0x4444, /* 3ru */

/*
  S tetromino sprites
*/
  0x2310, /* 0ru */
  0x3600, /* 1ru */
  0x2310, /* 2ru */
  0x3600, /* 3ru */

/*
  Z tetromino sprites
*/
  0x1320, /* 0ru */
  0x6300, /* 1ru */
  0x1320, /* 2ru */
  0x6300, /* 3ru */
};

void sleep(clock_t ticks) {
  clock_t stamp = clock();
  do {clock_t stamp = clock();}
  while (clock() - stamp < ticks);
}

bool tet_solid(tet_t t, coord_t x, coord_t y) {
  int bit_n = y * TET_W + x;
  bool in_tet = t & (1 << bit_n);
  return in_tet;
}

bool tet_collide_lat(tet_t t, coord_t x, coord_t y) {
  int ix, iy, board_x;

  /* Handle tetromino collisions. */
  for (int bit_n = 0; bit_n < BIT_SIZE(tet_t); ++bit_n) {
    ix = bit_n % TET_W;
    iy = bit_n / TET_W;

    bool solid = tet_solid(t, ix, iy);
    if (!solid) continue;

    board_x = ix + x - BORDER_WIDTH;

    if ((board_x >= BOARD_WIDTH) ||
        (board_x < 0)) return 1;

    bool colliding =
      (mvinch(iy + y,
              ix + x) & A_CHARTEXT) == TET_CH;

    if (colliding) return 1;
  }

  return 0;
}

coord_t tet_collide_lon(tet_t t, coord_t x, coord_t y) {
  int ix, iy;
  coord_t line_y;

  /* Handle tetromino collisions. */
  /* This goes from the bottom up because we prioritize lower
  collisions for when we need to clear lines bottom to top. */
  for (int bit_n = BIT_SIZE(tet_t); bit_n >= 0; --bit_n) {
    ix = bit_n % TET_W;
    iy = bit_n / TET_W;

    bool solid = tet_solid(t, ix, iy);
    if (!solid) continue;

    line_y = iy + y;

    if (line_y > BOARD_HEIGHT - 1) return line_y + 1;

    bool colliding =
      (mvinch(line_y,
              ix + x) & A_CHARTEXT) == TET_CH;

    if (colliding) return line_y + 1;
  }

  return 0;
}

tet_t state_get_tet(State *g_state) {
  return tets[g_state->tets_queue[0] + g_state->ru];
}

void draw_tet(tet_t t, coord_t x, coord_t y, bool draw_flag) {
  int ix, iy;
  char curr_char;

  /* Iterate through bits of tetromino and draw if 1. */
  for (int bit_n = 0; bit_n < BIT_SIZE(tet_t); ++bit_n) {
    ix = bit_n % TET_W;
    iy = bit_n / TET_W;

    curr_char = mvinch(iy + y,
                       ix + x) & A_CHARTEXT;

    bool in_target = t & (1 << bit_n);

    if (in_target && curr_char != BORDER_CH) {
      if (draw_flag) {
        printw("%c", TET_CH);
        continue;
      }
      printw("%c", BLANK_CH);
    }
  }
}

void put_tet(State *g_state) {
  tet_t t = state_get_tet(g_state);

  /* Move focus to the top of the screen. */
  g_state->drop_x = X_START;
  g_state->drop_y = 0;

  /* Put cursor at the top of screen. */
  mvinch(g_state->drop_x,
         g_state->drop_y);

  /* Draw new tetromino. */
  draw_tet(t, g_state->drop_x,
              g_state->drop_y, 1);
}

coord_t drop_tet(State *g_state) {
  tet_t t = state_get_tet(g_state);

  /* Clear tetromino from current position. */
  draw_tet(t, g_state->drop_x,
              g_state->drop_y, 0);

  /* Check for collisions. */
  coord_t collision = tet_collide_lon(t, g_state->drop_x,
                                       g_state->drop_y + 1);

  /* If collision occurs, redraw original tetromino, and return. */
  if (collision) {
    draw_tet(t, g_state->drop_x,
                g_state->drop_y, 1);
    return collision;
  }

  /* Draw tetromino at the next position. */
  draw_tet(t, g_state->drop_x,
            ++g_state->drop_y, 1);

  return collision;
}

bool rotate_tet(State *g_state) {
  /* Calculate new rotation (0ru to 3ru) */
  rot_t new_rotation = (g_state->ru + 1) % 4;

  /* Clear old rotation tetromino. */
  draw_tet(state_get_tet(g_state),
           g_state->drop_x,
           g_state->drop_y, 0);

  bool lat_collision = tet_collide_lat(
      tets[g_state->tets_queue[0] + new_rotation],
      g_state->drop_x,
      g_state->drop_y
  );

  bool lon_collision = tet_collide_lon(
      tets[g_state->tets_queue[0] + new_rotation],
      g_state->drop_x,
      g_state->drop_y
  );

  bool collision = (lat_collision || lon_collision);

  /* If collision occurs, redraw original tetromino, and return. */
  if (collision) {
    draw_tet(state_get_tet(g_state),
             g_state->drop_x,
             g_state->drop_y, 1);
    return 1;
  }

  /* Apply rotation since it doesn't cause collision. */
  g_state->ru = new_rotation;

  /* Draw new rotated tetromino. */
  draw_tet(state_get_tet(g_state),
           g_state->drop_x,
           g_state->drop_y, 1);

  /* Return 1 if collision has occured for handling. */
  return collision;
}

bool move_lat(State *g_state, int direction) {
  /* Calculate new position (0ru to 3ru) */
  int new_position = g_state->drop_x + direction;

  /* Clear old tetromino. */
  draw_tet(state_get_tet(g_state),
           g_state->drop_x,
           g_state->drop_y, 0);

  /* Check for collisions. */
  bool collision = tet_collide_lat(
    state_get_tet(g_state),
    new_position,
    g_state->drop_y
  );

  /* Only apply left translation if it doesn't cause collision. */
  if (!collision) {
    g_state->drop_x = new_position;
  }

  /* Draw left translated tetromino. */
  draw_tet(state_get_tet(g_state),
           g_state->drop_x,
           g_state->drop_y, 1);

  /* Return 1 if collision has occured for handling. */
  return collision;
}

void process_queue(State *g_state) {
  int q_len = LEN(g_state->tets_queue);

  /* Shift all elements left in array by one element. */
  for (int nxt = 1; nxt < q_len; ++nxt) {
    g_state->tets_queue[nxt - 1] = g_state->tets_queue[nxt];
  }

  /* Push a new "random" tetromino at rotation of 0ru. */
  LAST_OF(g_state->tets_queue) = (rand() % (LEN(tets) / 4)) * 4;
}

bool row_full(coord_t row) {
  for (coord_t x = 1; x < BOARD_WIDTH; ++x) {
    char ch = mvinch(row, BORDER_WIDTH + x) & A_CHARTEXT;
    if (ch != TET_CH) return 0;
  }
  return 1;
}

void fill_row(coord_t row, char ch) {
  for (coord_t x = 0; x < BOARD_WIDTH; ++x) {
    mvinch(row, BORDER_WIDTH + x);
    printw("%c", ch);
  }
}

void shift_rows(coord_t start_row) {
  for (coord_t row = start_row; row > 1; --row) {
    for (coord_t x = 0; x < BOARD_WIDTH; ++x) {
      char above = mvinch(row - 1, BORDER_WIDTH + x) & A_CHARTEXT;
      printw("%c", BLANK_CH);
      mvinch(row, BORDER_WIDTH + x);
      printw("%c", above);
    }
  }
}

bool clear_check_from(State *g_state, int line_y) {
  /* Start at row we know is full. */
  coord_t y = line_y;

  /* Keep track of first y value. */
  coord_t start_y = line_y;

  /* The furthest we could possibly go is TET_H. */
  coord_t end_y = start_y - TET_H;

  bool did_clear = 0;

/*
  []   <- check up to here
  []
  [][] <- start here
============
*/

  /* From collision point to top of tetromino.
  y > 0 keeps logic working even if end_y overflows. */
  while (y > 0 && y > end_y) {
    if (row_full(y)) {
      /* Make row flash. */
      fill_row(y, '*');
      refresh();
      sleep(40000);

      /* Clear row. */
      fill_row(y, BLANK_CH);
      refresh();
      sleep(200000);

      /* Make gravity happen. */
      shift_rows(y);

      /* Increment score and set clear flag. */
      g_state->score += SCORE_INC;
      did_clear = 1;
    } else {
      --y;
    }
  }
  return did_clear;
}

void draw_queue(State *g_state) {
  int draw_x = BOARD_WIDTH + 2 * BORDER_WIDTH + HUD_OFFSET;

  for (int slot_i = 0; slot_i < LEN(g_state->tets_queue) - 1; ++slot_i) {
    /* Clear previous slot entry. */
    draw_tet(tets[g_state->tets_queue[slot_i]], draw_x, slot_i * TET_H, 0);

    /* Draw current slot entry. */
    draw_tet(tets[g_state->tets_queue[slot_i + 1]], draw_x, slot_i * TET_H, 1);
  }
}

void draw_score(score_t s, coord_t x, coord_t y) {
  mvinch(y, x);
  printw("Score: %u", s);
}

bool alive_loop(State *g_state, int action) {
  coord_t collision = 0;

  /* Call respective function for each action. */
  /* Each function handles drawing as well as updating. */
  switch (action) {

    case ACTION_ROTATE:
      /* Clear tet from last position with old rotation. */
      rotate_tet(g_state);
      break;

    case ACTION_DROP:
      /* Move the tetromino downwards. */
      collision = drop_tet(g_state);
      break;

    case ACTION_INSTANT_DROP:
      /* Drop the tetromino until there's collision. */
      do collision = drop_tet(g_state);
      while (!collision);

      break;

    case ACTION_MOVE_LEFT:
      move_lat(g_state, -1);
      break;

    case ACTION_MOVE_RIGHT:
      move_lat(g_state, 1);
      break;

    default:
      return 0;
  }

  tet_t curr_tet = state_get_tet(g_state);

  /* Handle collisions. */
  if (collision) {
    /* If the collision happens too high on the screen, game over. */
    if (collision - 1 <= TET_H) return 0;

    bool did_clear = clear_check_from(g_state, collision - 1);

    /* If any lines were cleared, draw the updated score. */
    if (did_clear) {
      draw_score(g_state->score, SCORE_X, SCORE_Y);
    }

    /* Shift queue left one. */
    process_queue(g_state);

    /* Draw the updated queue. */
    draw_queue(g_state);

    /* Set rotation to 0 for new tetromino. */
    g_state->ru = 0;

    /* Place new tetromino. */
    put_tet(g_state);
  }
  return 1;
}

void game_over_screen(score_t s) {
  clear();
  mvinch(0, 0);
  printw("GAME OVER\nSCORE %u", s);
  mvinch(2, 0);
  printw("Press R to restart or any other key to exit.");
}

void draw_board() {
  for (coord_t y = 0; y <= BOARD_HEIGHT; ++y) {
    for (coord_t x = 1; x < BORDER_WIDTH; ++x) {
      printw("%c", BORDER_CH);
    }
    printw("|");
    for (coord_t x = 0; x < BOARD_WIDTH; ++x) {
      if (y == 0 || y == BOARD_HEIGHT) {
        printw("%c", BORDER_CH);
      } else {
        printw("%c", BLANK_CH);
      }
    }
    printw("|");
    for (coord_t x = 1; x < BORDER_WIDTH; ++x) {
      printw("%c", BORDER_CH);
    }
    mvinch(y + 1, 0);
  }
}

int main() {
  /* ncurses setup */
  initscr();   /* Initialize ncurses. */
  curs_set(0); /* Make the cursor invisible. */
  noecho();    /* Disable drawing of input characters. */
  raw();       /* Receive raw input. */

  /* State object to store game state on stack. */
  State *g_state;

lbl_restart:
  clear();

  /* Compound literal to init state. */
  g_state = &(State) {
    .drop_x = 0,
    .drop_y = 0,
    .ru = 0,
    .score = 0,
    .tets_queue = {
      TET_I(0),
      TET_I(2),
      TET_I(4),
      TET_I(6),
    }
  };

  /* Draw the board background. */
  draw_board();

  /* Draw the score display. */
  draw_score(0, SCORE_X, SCORE_Y);

  /* Draw the queue. */
  draw_queue(g_state);

  /* Place the inital tetromino. */
  put_tet(g_state);

  bool GAME_ONGOING = 1;

  /* Loop the game loop if the game is game. */
  while (GAME_ONGOING) {
    char user_input;

  lbl_cancel_poll:
    user_input = getch() & A_CHARTEXT;

    switch (user_input) {
      /* Process movement */
      case 'h':
      case 'a':
        goto lbl_move_left;

      case 'l':
      case 'd':
        goto lbl_move_right;

      case 'j':
      case 's':
        goto lbl_drop;

      case 'k':
      case 'w':
        goto lbl_instant_drop;

      case ' ':
      case 'f':
        goto lbl_rotate;

      case 'r':
        goto lbl_restart;

      /* Quit game */
      case 'c':
      case 'q':
        goto lbl_cleanup_and_exit;

      /* Non-game inputs */
      default:
        goto lbl_cancel_poll;
    }

  lbl_drop:
    GAME_ONGOING = alive_loop(g_state, ACTION_DROP);
    continue;

  lbl_instant_drop:
    GAME_ONGOING = alive_loop(g_state, ACTION_INSTANT_DROP);
    continue;

  lbl_rotate:
    GAME_ONGOING = alive_loop(g_state, ACTION_ROTATE);
    continue;

  lbl_move_left:
    GAME_ONGOING = alive_loop(g_state, ACTION_MOVE_LEFT);
    continue;

  lbl_move_right:
    GAME_ONGOING = alive_loop(g_state, ACTION_MOVE_RIGHT);
  }

  /* Show game over screen. */
  game_over_screen(g_state->score);

  char game_over_input = getch() & A_CHARTEXT;

  /* Allow the user to restart or exit based on which key they press. */
  if (game_over_input == 'r' ||
      game_over_input == 'R')
  {
    goto lbl_restart;
  }

lbl_cleanup_and_exit:

  /* Give the user their cursor back. */
  curs_set(1);

  /* End ncurses. */
  endwin();

  return 0;
}
