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

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    /* Create the window */
    if (!SDL_CreateWindowAndRenderer("Hello World", 800, 600, SDL_WINDOW_FULLSCREEN, &window, &renderer)) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
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
    case SDL_EVENT_MOUSE_MOTION:
        mouse_x = event->motion.x;
        mouse_y = event->motion.y;
        break;
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

    /* Draw a red circle in the center of the screen */
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    draw_circle(renderer, mouse_x, mouse_y, 50);
    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
}
