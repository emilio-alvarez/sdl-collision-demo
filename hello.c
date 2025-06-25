/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static int mouse_x = 0;
static int mouse_y = 0;
static int game_over = 0;  /* Flag to track if game is over */
static int w = 0, h = 0;

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    /* Create the window */
    if (!SDL_CreateWindowAndRenderer("Hello World", 800, 600, SDL_WINDOW_FULLSCREEN, &window, &renderer)) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GetRenderOutputSize(renderer, &w, &h);

    /* Set initial mouse position inside yellow square but away from edges */
    /* Yellow square: x=50 to 350 (width=300), y=50 to 250 (height=200) */
    /* Circle radius: 50, so need at least 50 pixels from each edge */
    mouse_x = w / 2;
    mouse_y = h / 2;
    SDL_WarpMouseInWindow(window, mouse_x, mouse_y);

    return SDL_APP_CONTINUE;
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    switch (event->type) {
    case SDL_EVENT_KEY_DOWN:
        if (event->key.key == SDLK_ESCAPE) {
            return SDL_APP_SUCCESS; /* end the program, reporting success to the OS. */
        }
        break;
    case SDL_EVENT_MOUSE_MOTION: {
        /* Constrain mouse position to keep circle within yellow square bounds */
        mouse_x = event->motion.x;
        mouse_y = event->motion.y;
        break;
    }
    }
    return SDL_APP_CONTINUE;
}

static void check_collision(SDL_FRect player_square, SDL_FRect obstacle_square) {
  int left_edge = player_square.x;
  int right_edge = player_square.x + player_square.w;
  int top_edge = player_square.y;
  int bottom_edge = player_square.y + player_square.h;

  int square_left_edge = obstacle_square.x;
  int square_right_edge = obstacle_square.x + obstacle_square.w;
  int square_top_edge = obstacle_square.y;
  int square_bottom_edge = obstacle_square.y + obstacle_square.h;

  if (bottom_edge <= square_top_edge) {
    return;
  }

  if (top_edge >= square_bottom_edge) {
    return;
  }

  if (left_edge >= square_right_edge) {
    return;
  }

  if (right_edge <= square_left_edge) {
    return;
  }

  game_over = 1;
}


static void draw_circle(SDL_Renderer *renderer, int center_x, int center_y, int radius)
{
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                SDL_RenderPoint(renderer, center_x + x, center_y + y);
            }
        }
    }
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    /* Draw the background */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    /* Draw the path */
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);  /* Yellow color */

    SDL_FRect path_squares[] = {
      {0, 0, 500, 200},
      {500, 0, 200, 500},
      {0, 700, 500, 200},
    };
    for (int i = 0; i < sizeof(path_squares) / sizeof(path_squares[0]); i++) {
      SDL_RenderFillRect(renderer, &path_squares[i]);
    }

    /* Draw a red circle in the center of the screen */
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_FRect player_square = {mouse_x - 25 , mouse_y - 25, 50, 50};
    SDL_RenderFillRect(renderer, &player_square);

    for (int i = 0; i < sizeof(path_squares) / sizeof(path_squares[0]); i++) {
      check_collision(player_square, path_squares[i]);
    }

    /* Draw game over text if game is over */
    if (game_over) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);  /* White color */
        const char *game_over_text = "GAME OVER";
        const float text_scale = 3.0f;  /* Make text 3x bigger */

        /* Save current scale */
        float current_scale_x, current_scale_y;
        SDL_GetRenderScale(renderer, &current_scale_x, &current_scale_y);

        /* Set scale for bigger text */
        SDL_SetRenderScale(renderer, text_scale, text_scale);

        /* Calculate centered position with scale */
        int text_x = ((w / text_scale) - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * SDL_strlen(game_over_text)) / 2;
        int text_y = (h / text_scale) / 2;
        SDL_RenderDebugText(renderer, text_x, text_y, game_over_text);

        /* Restore original scale */
        SDL_SetRenderScale(renderer, current_scale_x, current_scale_y);
    }

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
}
