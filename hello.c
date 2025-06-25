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
static int mouse_moved = 0;  /* Flag to track if mouse has been moved */

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    /* Create the window */
    if (!SDL_CreateWindowAndRenderer("Hello World", 800, 600, SDL_WINDOW_FULLSCREEN, &window, &renderer)) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    
    /* Set initial mouse position inside yellow square but away from edges */
    /* Yellow square: x=50 to 350 (width=300), y=50 to 250 (height=200) */
    /* Circle radius: 50, so need at least 50 pixels from each edge */
    mouse_x = 50 + 50 + 100;  /* 50 (left edge) + 50 (radius) + 100 (safe distance) = 200 */
    mouse_y = 50 + 50;   /* 50 (top edge) + 50 (radius) + 50 (safe distance) = 150 */
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
        int new_x = event->motion.x;
        int new_y = event->motion.y;
        int radius = 50;  /* Circle radius */
        
        /* Only check for collision after mouse has been moved */
        if (mouse_moved) {
            /* Check if circle would touch the boundary */
            if (new_x - radius <= 50 || new_x + radius >= 350 || 
                new_y - radius <= 50 || new_y + radius >= 250) {
                if (!game_over) {
                    game_over = 1;
                }
            }
        } else {
            /* First mouse movement - set the flag */
            mouse_moved = 1;
        }
        
        /* Yellow square bounds: x=50 to 350, y=50 to 250 */
        if (new_x - radius < 50) new_x = 50 + radius;
        if (new_x + radius > 350) new_x = 350 - radius;
        if (new_y - radius < 50) new_y = 50 + radius;
        if (new_y + radius > 250) new_y = 250 - radius;
        
        mouse_x = new_x;
        mouse_y = new_y;
        break;
    }
    }
    return SDL_APP_CONTINUE;
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
    int w = 0, h = 0;

    SDL_GetRenderOutputSize(renderer, &w, &h);

    /* Draw the background */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    
    /* Draw a yellow square as background */
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);  /* Yellow color */
    SDL_FRect background_square = {50, 50, 300, 200};  /* x=50, y=50, width=300, height=200 */
    SDL_RenderFillRect(renderer, &background_square);

    /* Draw a red circle in the center of the screen */
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    draw_circle(renderer, mouse_x, mouse_y, 50);
    
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
