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
static int w = 0, h = 0;
static int game_over = 0;

static SDL_FRect player;
static float player_vy = 0; // Vertical velocity
static int is_on_ground = 0;
static float player_rotation = 0.0f; // Rotation angle in degrees

// Physics and movement constants
const float GRAVITY = 0.5f;
const float JUMP_STRENGTH = -12.0f;
const float MOVE_SPEED = 2.0f;
const float SPIN_SPEED = 5.0f; // Degrees per frame when spinning

static const SDL_FRect path_squares[] = {
    { 0, 200, 500, 50 },
    { 450, 200, 50, 400 },
    { 200, 550, 500, 50 },
};
static const int num_path_squares = sizeof(path_squares) / sizeof(path_squares[0]);

static const SDL_FRect lava_squares[] = {
    { 700, 550, 500, 50 },
};
static const int num_lava_squares = sizeof(lava_squares) / sizeof(lava_squares[0]);

static int is_colliding(SDL_FRect a, SDL_FRect b)
{
    // Axis-Aligned Bounding Box (AABB) collision check
    if (a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y) {
        return 1; // Collision
    }
    return 0; // No collision
}

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    /* Create the window */
    if (!SDL_CreateWindowAndRenderer("Gravity Demo", 800, 600, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GetRenderOutputSize(renderer, &w, &h);

    // Start player just above the first platform
    player.w = 50;
    player.h = 50;
    player.x = 100;
    player.y = 100;

    return SDL_APP_CONTINUE;
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    switch (event->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_KEY_DOWN:
        switch (event->key.key) {
        case SDLK_ESCAPE:
            return SDL_APP_SUCCESS;
        case SDLK_UP:
            if (is_on_ground) {
                player_vy = JUMP_STRENGTH;
                is_on_ground = 0;
            }
            break;
        }
        break;
    }
    return SDL_APP_CONTINUE;
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    if (!game_over) {
        // --- Handle horizontal movement ---
        const Uint8 *keystate = SDL_GetKeyboardState(NULL);
        float old_x = player.x;
        if (keystate[SDL_SCANCODE_LEFT]) {
            player.x -= MOVE_SPEED;
        }
        if (keystate[SDL_SCANCODE_RIGHT]) {
            player.x += MOVE_SPEED;
        }

        // --- Horizontal collision with path ---
        for (int i = 0; i < num_path_squares; i++) {
            if (is_colliding(player, path_squares[i])) {
                player.x = old_x; // Revert move
                break;
            }
        }

        // --- Handle vertical movement (gravity) ---
        player_vy += GRAVITY;
        player.y += player_vy;

        // --- Vertical collision with path ---
        is_on_ground = 0;
        for (int i = 0; i < num_path_squares; i++) {
            if (is_colliding(player, path_squares[i])) {
                // Check if landing on top
                if (player_vy > 0) {
                    player.y = path_squares[i].y - player.h;
                    player_vy = 0;
                    is_on_ground = 1;
                }
                // Check if hitting bottom
                else if (player_vy < 0) {
                    player.y = path_squares[i].y + path_squares[i].h;
                    player_vy = 0;
                }
                break; // Assume one collision is enough
            }
        }

        // Check for collision with lava
        for (int i = 0; i < num_lava_squares; i++) {
            if (is_colliding(player, lava_squares[i])) {
                game_over = 1;
                break;
            }
        }
    } else {
        // When game is over, spin the player
        player_rotation += SPIN_SPEED;
        if (player_rotation >= 360.0f) {
            player_rotation -= 360.0f; // Keep angle in 0-360 range
        }
    }


    /* Rendering */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    /* Draw the path */
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255); /* Yellow color */
    SDL_RenderFillRects(renderer, path_squares, num_path_squares);

    /* Draw the lava squares */
    SDL_SetRenderDrawColor(renderer, 255, 100, 0, 255); /* Orange/Red for lava */
    SDL_RenderFillRects(renderer, lava_squares, num_lava_squares);


    /* Draw the player square */
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

    if (game_over && player_rotation != 0.0f) {
        // Draw rotated square using SDL_RenderGeometry
        float center_x = player.x + player.w / 2.0f;
        float center_y = player.y + player.h / 2.0f;
        float half_size = player.w / 2.0f;

        // Convert rotation to radians
        float angle_rad = player_rotation * (3.14159f / 180.0f);
        float cos_a = SDL_cosf(angle_rad);
        float sin_a = SDL_sinf(angle_rad);

        // Calculate rotated corners
        SDL_Vertex vertices[4];

        // Top-left corner
        float x1 = -half_size, y1 = -half_size;
        vertices[0].position.x = center_x + (x1 * cos_a - y1 * sin_a);
        vertices[0].position.y = center_y + (x1 * sin_a + y1 * cos_a);
        vertices[0].color.r = 255; vertices[0].color.g = 0; vertices[0].color.b = 0; vertices[0].color.a = 255;

        // Top-right corner
        float x2 = half_size, y2 = -half_size;
        vertices[1].position.x = center_x + (x2 * cos_a - y2 * sin_a);
        vertices[1].position.y = center_y + (x2 * sin_a + y2 * cos_a);
        vertices[1].color.r = 255; vertices[1].color.g = 0; vertices[1].color.b = 0; vertices[1].color.a = 255;

        // Bottom-right corner
        float x3 = half_size, y3 = half_size;
        vertices[2].position.x = center_x + (x3 * cos_a - y3 * sin_a);
        vertices[2].position.y = center_y + (x3 * sin_a + y3 * cos_a);
        vertices[2].color.r = 255; vertices[2].color.g = 0; vertices[2].color.b = 0; vertices[2].color.a = 255;

        // Bottom-left corner
        float x4 = -half_size, y4 = half_size;
        vertices[3].position.x = center_x + (x4 * cos_a - y4 * sin_a);
        vertices[3].position.y = center_y + (x4 * sin_a + y4 * cos_a);
        vertices[3].color.r = 255; vertices[3].color.g = 0; vertices[3].color.b = 0; vertices[3].color.a = 255;

        // Define triangles (two triangles make a square)
        int indices[6] = { 0, 1, 2, 0, 2, 3 };

        SDL_RenderGeometry(renderer, NULL, vertices, 4, indices, 6);
    } else {
        // Draw normal square
        SDL_RenderFillRect(renderer, &player);
    }

    /* Draw game over text if game is over */
    if (game_over) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); /* White color */
        const char *game_over_text = "GAME OVER";
        const float text_scale = 3.0f; /* Make text 3x bigger */
        float current_scale_x, current_scale_y;
        SDL_GetRenderScale(renderer, &current_scale_x, &current_scale_y);
        SDL_SetRenderScale(renderer, text_scale, text_scale);
        float text_x = ((w / text_scale) - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * SDL_strlen(game_over_text)) / 2;
        float text_y = ((h / text_scale) - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) / 2;
        SDL_RenderDebugText(renderer, text_x, text_y, game_over_text);
        SDL_SetRenderScale(renderer, current_scale_x, current_scale_y);
    }

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
}
