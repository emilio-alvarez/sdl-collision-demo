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
#include <math.h>
#include <stdlib.h>

// Game constants
const int SCREEN_WIDTH = 1200;
const int SCREEN_HEIGHT = 800;
const int MAX_PARTICLES = 100;
const int MAX_COLLECTIBLES = 10;
const int MAX_LEVELS = 6;

// Physics constants
const float GRAVITY = 0.2f;
const float JUMP_STRENGTH = -8.0f;
const float MOVE_SPEED = 5.0f;
const float MAX_FALL_SPEED = 18.0f;
const float SPIN_SPEED = 8.0f;

const int COYOTE_TIME = 6; // frames
const int JUMP_BUFFER_TIME = 8; // frames

// Game state
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static int w = 0, h = 0;
static int game_over = 0;
static int game_won = 0;
static int current_level = 0;
static int score = 0;
static int lives = 3;

// Timer system
static float level_timer = 0.0f;
static float level_time_limit = 60.0f; // Default 60 seconds

// Frame rate control
static Uint64 last_frame_time = 0;
static const Uint64 TARGET_FRAME_TIME = 16666667; // 60 FPS in nanoseconds

// Player state
static SDL_FRect player;
static float player_vy = 0;
static int is_on_ground = 0;
static int coyote_timer = 0;
static int jump_buffer = 0;
static int jump_held = 0;
static float player_rotation = 0.0f;
static int has_double_jump = 0;
static int double_jump_used = 0;
static float invincibility_timer = 0;

// Particle system
typedef struct {
    float x, y, vx, vy;
    float life, max_life;
    Uint8 r, g, b, a;
    int active;
} Particle;

static Particle particles[MAX_PARTICLES];

// Collectibles
typedef struct {
    SDL_FRect rect;
    int collected;
    float bob_offset;
} Collectible;

static Collectible collectibles[MAX_COLLECTIBLES];
static int total_collectibles = 0;
static int collected_count = 0;

// Moving platforms
typedef struct {
    SDL_FRect rect;
    float vx, vy;
    float start_x, end_x, start_y, end_y;
    int direction;
} MovingPlatform;

static MovingPlatform moving_platforms[5];
static int num_moving_platforms = 0;

// Level data
typedef struct {
    SDL_FRect platforms[20];
    int num_platforms;
    SDL_FRect lava_squares[5];
    int num_lava;
    SDL_FRect start_pos;
    SDL_FRect goal;
    Collectible level_collectibles[MAX_COLLECTIBLES];
    int num_collectibles;
    MovingPlatform level_moving_platforms[5];
    int num_moving;
    float time_limit; // Time limit for this level in seconds
} Level;

static Level levels[MAX_LEVELS];

// Function declarations
void init_particles(void);
void add_particle(float x, float y, float vx, float vy, Uint8 r, Uint8 g, Uint8 b, float life);
void update_particles(void);
void render_particles(void);
void init_levels(void);
void load_level(int level_num);
void reset_player(void);
void update_collectibles(void);
void render_collectibles(void);
void update_moving_platforms(void);
void render_moving_platforms(void);
int is_colliding(SDL_FRect a, SDL_FRect b);
void render_hud(void);
void render_background(void);

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    if (!SDL_CreateWindowAndRenderer("Enhanced Platformer", SCREEN_WIDTH, SCREEN_HEIGHT, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GetRenderOutputSize(renderer, &w, &h);

    init_particles();
    init_levels();
    load_level(0);
    reset_player();

    // Initialize frame timing
    last_frame_time = SDL_GetTicksNS();

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
        case SDLK_R:
            if (game_over || game_won) {
                // Restart current level
                game_over = 0;
                game_won = 0;
                lives = 3;
                score = 0;
                load_level(current_level);
                reset_player();
            }
            break;
        case SDLK_N:
            if (game_won && current_level < MAX_LEVELS - 1) {
                // Next level
                current_level++;
                game_won = 0;
                load_level(current_level);
                reset_player();
            }
            break;
        case SDLK_SPACE:
        case SDLK_UP:
            if (!game_over && !game_won) {
                jump_buffer = JUMP_BUFFER_TIME;
                jump_held = 1;
            }
            break;
        }
        break;
    case SDL_EVENT_KEY_UP:
        switch (event->key.key) {
        case SDLK_SPACE:
        case SDLK_UP:
            jump_held = 0;
            break;
        }
        break;
    }
    return SDL_APP_CONTINUE;
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    if (!game_over && !game_won) {
        // Update timer
        level_timer -= 1.0f / 60.0f; // Assuming 60 FPS
        if (level_timer <= 0) {
            game_over = 1;
        }

        // Handle input
        const Uint8 *keystate = SDL_GetKeyboardState(NULL);
        float old_x = player.x;

        // Horizontal movement
        if (keystate[SDL_SCANCODE_LEFT] || keystate[SDL_SCANCODE_A]) {
            player.x -= MOVE_SPEED;
            // Add dust particles when moving
            if (is_on_ground && SDL_GetTicks() % 3 == 0) {
                add_particle(player.x + player.w/2, player.y + player.h,
                           (float)(rand() % 20 - 10) / 10.0f, -1.0f,
                           139, 69, 19, 30.0f);
            }
        }
        if (keystate[SDL_SCANCODE_RIGHT] || keystate[SDL_SCANCODE_D]) {
            player.x += MOVE_SPEED;
            if (is_on_ground && SDL_GetTicks() % 3 == 0) {
                add_particle(player.x + player.w/2, player.y + player.h,
                           (float)(rand() % 20 - 10) / 10.0f, -1.0f,
                           139, 69, 19, 30.0f);
            }
        }

        // Screen boundaries
        if (player.x < 0) player.x = 0;
        if (player.x + player.w > w) player.x = w - player.w;

                // Horizontal collision with platforms
        Level *level = &levels[current_level];
        for (int i = 0; i < level->num_platforms; i++) {
            if (is_colliding(player, level->platforms[i])) {
                player.x = old_x;
                break;
            }
        }

        // Check moving platform collisions
        for (int i = 0; i < num_moving_platforms; i++) {
            if (is_colliding(player, moving_platforms[i].rect)) {
                player.x = old_x;
                break;
            }
        }

        // Coyote time
        if (is_on_ground) {
            coyote_timer = COYOTE_TIME;
        } else if (coyote_timer > 0) {
            coyote_timer--;
        }

        // Jump buffering and variable jump height
        if (jump_buffer > 0) {
            jump_buffer--;
            if (coyote_timer > 0 || (has_double_jump && !double_jump_used)) {
                if (coyote_timer > 0) {
                    player_vy = JUMP_STRENGTH;
                    coyote_timer = 0;
                } else {
                    player_vy = JUMP_STRENGTH * 0.8f; // Double jump is slightly weaker
                    double_jump_used = 1;
                }
                jump_buffer = 0;

                // Jump particles
                for (int i = 0; i < 8; i++) {
                    add_particle(player.x + player.w/2, player.y + player.h,
                               (float)(rand() % 40 - 20) / 10.0f,
                               (float)(rand() % 10 + 5) / 10.0f,
                               200, 200, 255, 40.0f);
                }
            }
        }

        // Variable jump height
        if (!jump_held && player_vy < -2.0f) {
            player_vy *= 0.5f; // Cut jump short
        }

        // Gravity and vertical movement
        player_vy += GRAVITY;
        if (player_vy > MAX_FALL_SPEED) player_vy = MAX_FALL_SPEED;
        player.y += player_vy;



                 // Vertical collision
         is_on_ground = 0;

         // Platform collisions
         for (int i = 0; i < level->num_platforms; i++) {
             if (is_colliding(player, level->platforms[i])) {
                 if (player_vy > 0) {
                     player.y = level->platforms[i].y - player.h;
                     player_vy = 0;
                     is_on_ground = 1;
                     double_jump_used = 0;

                     // Landing particles
                     for (int j = 0; j < 5; j++) {
                         add_particle(player.x + (float)(rand() % (int)player.w),
                                    player.y + player.h,
                                    (float)(rand() % 20 - 10) / 5.0f, -2.0f,
                                    139, 69, 19, 25.0f);
                     }
                 } else if (player_vy < 0) {
                     player.y = level->platforms[i].y + level->platforms[i].h;
                     player_vy = 0;
                 }
                 break;
             }
         }

         // Moving platform collisions
         for (int i = 0; i < num_moving_platforms; i++) {
             if (is_colliding(player, moving_platforms[i].rect)) {
                 if (player_vy > 0) {
                     player.y = moving_platforms[i].rect.y - player.h;
                     player_vy = 0;
                     is_on_ground = 1;
                     double_jump_used = 0;
                     // Move with platform
                     player.x += moving_platforms[i].vx;
                 } else if (player_vy < 0) {
                     player.y = moving_platforms[i].rect.y + moving_platforms[i].rect.h;
                     player_vy = 0;
                 }
                 break;
             }
         }

        // Update invincibility
        if (invincibility_timer > 0) {
            invincibility_timer--;
        }

                 // Lava collision
         if (invincibility_timer <= 0) {
             for (int i = 0; i < level->num_lava; i++) {
                 if (is_colliding(player, level->lava_squares[i])) {
                    lives--;
                    if (lives <= 0) {
                        game_over = 1;
                    } else {
                        // Respawn with invincibility
                        player.x = level->start_pos.x;
                        player.y = level->start_pos.y;
                        player_vy = 0;
                        invincibility_timer = 120; // 2 seconds at 60fps
                    }

                    // Death particles
                    for (int j = 0; j < 15; j++) {
                        add_particle(player.x + player.w/2, player.y + player.h/2,
                                   (float)(rand() % 40 - 20) / 5.0f,
                                   (float)(rand() % 40 - 20) / 5.0f,
                                   255, 100, 0, 60.0f);
                    }
                    break;
                }
            }
        }

        // Fall off screen
        if (player.y > h + 100) {
            lives--;
            if (lives <= 0) {
                game_over = 1;
            } else {
                player.x = level->start_pos.x;
                player.y = level->start_pos.y;
                player_vy = 0;
                invincibility_timer = 60;
            }
        }

        // Goal collision
        if (is_colliding(player, level->goal)) {
            if (collected_count >= total_collectibles) {
                game_won = 1;
                score += 1000 + (lives * 500);
            }
        }

        update_collectibles();
        update_moving_platforms();
    } else if (game_over) {
        // Spin when dead
        player_rotation += SPIN_SPEED;
        if (player_rotation >= 360.0f) {
            player_rotation -= 360.0f;
        }
    }

    update_particles();

    // Rendering
    render_background();

    Level *level = &levels[current_level];

    // Draw platforms
    SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255);
    SDL_RenderFillRects(renderer, level->platforms, level->num_platforms);

    // Draw lava with animated effect
    for (int i = 0; i < level->num_lava; i++) {
        // Animated lava color
        Uint8 red = 255;
        Uint8 green = 50 + (Uint8)(50 * SDL_sin(SDL_GetTicks() * 0.01f + i));
        SDL_SetRenderDrawColor(renderer, red, green, 0, 255);
        SDL_RenderFillRect(renderer, &level->lava_squares[i]);

        // Lava particles
        if (SDL_GetTicks() % 5 == 0) {
            add_particle(level->lava_squares[i].x + (float)(rand() % (int)level->lava_squares[i].w),
                       level->lava_squares[i].y,
                       (float)(rand() % 10 - 5) / 10.0f, -2.0f,
                       255, 100, 0, 80.0f);
        }
    }

    render_moving_platforms();
    render_collectibles();

    // Draw goal
    SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255); // Gold
    SDL_RenderFillRect(renderer, &level->goal);

    // Goal glow effect
    if (collected_count >= total_collectibles) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 100);
        SDL_FRect glow = {level->goal.x - 5, level->goal.y - 5, level->goal.w + 10, level->goal.h + 10};
        SDL_RenderFillRect(renderer, &glow);
    }

    // Draw player
    if (invincibility_timer > 0 && ((int)invincibility_timer / 5) % 2) {
        // Flashing effect during invincibility
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);

        if (game_over && player_rotation != 0.0f) {
            // Rotated square for death animation
            float center_x = player.x + player.w / 2.0f;
            float center_y = player.y + player.h / 2.0f;
            float half_size = player.w / 2.0f;

            float angle_rad = player_rotation * (3.14159f / 180.0f);
            float cos_a = SDL_cosf(angle_rad);
            float sin_a = SDL_sinf(angle_rad);

            SDL_Vertex vertices[4];
            float corners[4][2] = {{-half_size, -half_size}, {half_size, -half_size},
                                 {half_size, half_size}, {-half_size, half_size}};

                         for (int i = 0; i < 4; i++) {
                 vertices[i].position.x = center_x + (corners[i][0] * cos_a - corners[i][1] * sin_a);
                 vertices[i].position.y = center_y + (corners[i][0] * sin_a + corners[i][1] * cos_a);
                 vertices[i].color = (SDL_FColor){1.0f, 0.2f, 0.2f, 1.0f};
             }

            int indices[6] = {0, 1, 2, 0, 2, 3};
            SDL_RenderGeometry(renderer, NULL, vertices, 4, indices, 6);
                 } else {
             // Normal square
             SDL_RenderFillRect(renderer, &player);
         }
    }

    render_particles();
    render_hud();

    SDL_RenderPresent(renderer);

    // Frame rate limiting
    Uint64 current_time = SDL_GetTicksNS();
    Uint64 frame_time = current_time - last_frame_time;

    if (frame_time < TARGET_FRAME_TIME) {
        Uint64 delay_ns = TARGET_FRAME_TIME - frame_time;
        SDL_DelayNS(delay_ns);
    }

    last_frame_time = SDL_GetTicksNS();

    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
}

// Implementation of helper functions
void init_particles(void)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        particles[i].active = 0;
    }
}

void add_particle(float x, float y, float vx, float vy, Uint8 r, Uint8 g, Uint8 b, float life)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) {
            particles[i].x = x;
            particles[i].y = y;
            particles[i].vx = vx;
            particles[i].vy = vy;
            particles[i].r = r;
            particles[i].g = g;
            particles[i].b = b;
            particles[i].life = life;
            particles[i].max_life = life;
            particles[i].active = 1;
            break;
        }
    }
}

void update_particles(void)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            particles[i].x += particles[i].vx;
            particles[i].y += particles[i].vy;
            particles[i].vy += 0.1f; // Gravity on particles
            particles[i].life--;

            if (particles[i].life <= 0) {
                particles[i].active = 0;
            }
        }
    }
}

void render_particles(void)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            float alpha = particles[i].life / particles[i].max_life;
            SDL_SetRenderDrawColor(renderer, particles[i].r, particles[i].g, particles[i].b,
                                 (Uint8)(255 * alpha));
            SDL_FRect rect = {particles[i].x - 1, particles[i].y - 1, 2, 2};
            SDL_RenderFillRect(renderer, &rect);
        }
    }
}

void init_levels(void)
{
    // Level 0 - Tutorial
    levels[0].num_platforms = 6;
    levels[0].platforms[0] = (SDL_FRect){0, h-50, 300, 50}; // Ground
    levels[0].platforms[1] = (SDL_FRect){400, h-150, 200, 30};
    levels[0].platforms[2] = (SDL_FRect){700, h-250, 200, 30};
    levels[0].platforms[3] = (SDL_FRect){1000, h-200, 200, 30};
    levels[0].platforms[4] = (SDL_FRect){350, h-350, 100, 30};
    levels[0].platforms[5] = (SDL_FRect){900, h-400, 100, 30};

    levels[0].num_lava = 2;
    levels[0].lava_squares[0] = (SDL_FRect){300, h-45, 100, 50};
    levels[0].lava_squares[1] = (SDL_FRect){600, h-45, 100, 50};

    levels[0].start_pos = (SDL_FRect){50, h-150, 50, 50};
    levels[0].goal = (SDL_FRect){w-100, h-300, 50, 50};

    levels[0].num_collectibles = 3;
    levels[0].level_collectibles[0] = (Collectible){{450, h-200, 20, 20}, 0, 0};
    levels[0].level_collectibles[1] = (Collectible){{750, h-300, 20, 20}, 0, 0};
    levels[0].level_collectibles[2] = (Collectible){{950, h-450, 20, 20}, 0, 0};

    levels[0].num_moving = 1;
    levels[0].level_moving_platforms[0] = (MovingPlatform){{500, h-300, 100, 20}, 1, 0, 500, 800, 0, 0, 1};

    // Level 1 - Intermediate
    levels[1].num_platforms = 8;
    levels[1].platforms[0] = (SDL_FRect){0, h-50, 200, 50};
    levels[1].platforms[1] = (SDL_FRect){300, h-150, 150, 30};
    levels[1].platforms[2] = (SDL_FRect){550, h-280, 100, 30};
    levels[1].platforms[3] = (SDL_FRect){750, h-200, 150, 30};
    levels[1].platforms[4] = (SDL_FRect){1000, h-350, 100, 30};
    levels[1].platforms[5] = (SDL_FRect){200, h-400, 100, 30};
    levels[1].platforms[6] = (SDL_FRect){400, h-500, 200, 30};
    levels[1].platforms[7] = (SDL_FRect){w-200, h-100, 200, 50};

    levels[1].num_lava = 3;
    levels[1].lava_squares[0] = (SDL_FRect){200, h-45, 100, 45};
    levels[1].lava_squares[1] = (SDL_FRect){450, h-45, 300, 45};
    levels[1].lava_squares[2] = (SDL_FRect){900, h-45, 100, 45};

    levels[1].start_pos = (SDL_FRect){50, h-150, 50, 50};
    levels[1].goal = (SDL_FRect){w-150, h-200, 50, 50};

    levels[1].num_collectibles = 4;
    levels[1].level_collectibles[0] = (Collectible){{350, h-200, 20, 20}, 0, 0};
    levels[1].level_collectibles[1] = (Collectible){{575, h-330, 20, 20}, 0, 0};
    levels[1].level_collectibles[2] = (Collectible){{1050, h-400, 20, 20}, 0, 0};
    levels[1].level_collectibles[3] = (Collectible){{500, h-550, 20, 20}, 0, 0};

    levels[1].num_moving = 2;
    levels[1].level_moving_platforms[0] = (MovingPlatform){{650, h-350, 80, 20}, 0, -1, 0, 0, h-450, h-250, 1};
    levels[1].level_moving_platforms[1] = (MovingPlatform){{800, h-400, 100, 20}, 1, 0, 800, 950, 0, 0, 1};

    // Level 2 - Advanced
    levels[2].num_platforms = 10;
    levels[2].platforms[0] = (SDL_FRect){0, h-50, 150, 50};
    levels[2].platforms[1] = (SDL_FRect){250, h-150, 100, 30};
    levels[2].platforms[2] = (SDL_FRect){450, h-250, 80, 30};
    levels[2].platforms[3] = (SDL_FRect){600, h-180, 100, 30};
    levels[2].platforms[4] = (SDL_FRect){800, h-320, 80, 30};
    levels[2].platforms[5] = (SDL_FRect){950, h-250, 100, 30};
    levels[2].platforms[6] = (SDL_FRect){200, h-450, 100, 30};
    levels[2].platforms[7] = (SDL_FRect){400, h-550, 150, 30};
    levels[2].platforms[8] = (SDL_FRect){700, h-480, 100, 30};
    levels[2].platforms[9] = (SDL_FRect){w-150, h-100, 150, 50};

    levels[2].num_lava = 4;
    levels[2].lava_squares[0] = (SDL_FRect){150, h-45, 100, 45};
    levels[2].lava_squares[1] = (SDL_FRect){350, h-45, 100, 45};
    levels[2].lava_squares[2] = (SDL_FRect){700, h-45, 250, 45};
    levels[2].lava_squares[3] = (SDL_FRect){550, h-245, 50, 70};

    levels[2].start_pos = (SDL_FRect){50, h-150, 50, 50};
    levels[2].goal = (SDL_FRect){w-100, h-200, 50, 50};

    levels[2].num_collectibles = 5;
    levels[2].level_collectibles[0] = (Collectible){{275, h-200, 20, 20}, 0, 0};
    levels[2].level_collectibles[1] = (Collectible){{475, h-300, 20, 20}, 0, 0};
    levels[2].level_collectibles[2] = (Collectible){{825, h-370, 20, 20}, 0, 0};
    levels[2].level_collectibles[3] = (Collectible){{475, h-600, 20, 20}, 0, 0};
    levels[2].level_collectibles[4] = (Collectible){{725, h-530, 20, 20}, 0, 0};

    levels[2].num_moving = 3;
    levels[2].level_moving_platforms[0] = (MovingPlatform){{300, h-350, 80, 20}, 1, 0, 300, 500, 0, 0, 1};
    levels[2].level_moving_platforms[1] = (MovingPlatform){{600, h-400, 80, 20}, 0, -1, 0, 0, h-500, h-300, 1};
    levels[2].level_moving_platforms[2] = (MovingPlatform){{850, h-150, 100, 20}, 1, 0, 850, 1000, 0, 0, 1};

    // Level 3 - Vertical Challenge
    levels[3].num_platforms = 12;
    levels[3].platforms[0] = (SDL_FRect){0, h-50, 100, 50}; // Small starting platform
    levels[3].platforms[1] = (SDL_FRect){200, h-150, 80, 20};
    levels[3].platforms[2] = (SDL_FRect){350, h-250, 80, 20};
    levels[3].platforms[3] = (SDL_FRect){150, h-350, 80, 20};
    levels[3].platforms[4] = (SDL_FRect){400, h-450, 80, 20};
    levels[3].platforms[5] = (SDL_FRect){250, h-550, 80, 20};
    levels[3].platforms[6] = (SDL_FRect){500, h-650, 80, 20};
    levels[3].platforms[7] = (SDL_FRect){700, h-600, 100, 20};
    levels[3].platforms[8] = (SDL_FRect){900, h-500, 80, 20};
    levels[3].platforms[9] = (SDL_FRect){1050, h-400, 80, 20};
    levels[3].platforms[10] = (SDL_FRect){850, h-300, 100, 20};
    levels[3].platforms[11] = (SDL_FRect){w-150, h-200, 150, 50};

    levels[3].num_lava = 5;
    levels[3].lava_squares[0] = (SDL_FRect){100, h-45, 100, 45};
    levels[3].lava_squares[1] = (SDL_FRect){300, h-45, 200, 45};
    levels[3].lava_squares[2] = (SDL_FRect){600, h-45, 300, 45};
    levels[3].lava_squares[3] = (SDL_FRect){450, h-345, 50, 95};
    levels[3].lava_squares[4] = (SDL_FRect){750, h-445, 50, 145};

    levels[3].start_pos = (SDL_FRect){25, h-150, 40, 40};
    levels[3].goal = (SDL_FRect){w-100, h-300, 50, 50};

    levels[3].num_collectibles = 6;
    levels[3].level_collectibles[0] = (Collectible){{225, h-200, 20, 20}, 0, 0};
    levels[3].level_collectibles[1] = (Collectible){{375, h-300, 20, 20}, 0, 0};
    levels[3].level_collectibles[2] = (Collectible){{275, h-600, 20, 20}, 0, 0};
    levels[3].level_collectibles[3] = (Collectible){{525, h-700, 20, 20}, 0, 0};
    levels[3].level_collectibles[4] = (Collectible){{925, h-550, 20, 20}, 0, 0};
    levels[3].level_collectibles[5] = (Collectible){{875, h-350, 20, 20}, 0, 0};

    levels[3].num_moving = 2;
    levels[3].level_moving_platforms[0] = (MovingPlatform){{600, h-350, 80, 20}, 0, -2, 0, 0, h-550, h-250, 1};
    levels[3].level_moving_platforms[1] = (MovingPlatform){{800, h-400, 80, 20}, 1, 0, 800, 950, 0, 0, 1};

    // Level 4 - Speed Run
    levels[4].num_platforms = 15;
    levels[4].platforms[0] = (SDL_FRect){0, h-50, 150, 50};
    levels[4].platforms[1] = (SDL_FRect){250, h-120, 60, 20};
    levels[4].platforms[2] = (SDL_FRect){400, h-180, 60, 20};
    levels[4].platforms[3] = (SDL_FRect){550, h-120, 60, 20};
    levels[4].platforms[4] = (SDL_FRect){700, h-200, 60, 20};
    levels[4].platforms[5] = (SDL_FRect){850, h-150, 60, 20};
    levels[4].platforms[6] = (SDL_FRect){1000, h-250, 60, 20};
    levels[4].platforms[7] = (SDL_FRect){900, h-350, 80, 20};
    levels[4].platforms[8] = (SDL_FRect){700, h-450, 80, 20};
    levels[4].platforms[9] = (SDL_FRect){500, h-350, 80, 20};
    levels[4].platforms[10] = (SDL_FRect){300, h-450, 80, 20};
    levels[4].platforms[11] = (SDL_FRect){100, h-350, 80, 20};
    levels[4].platforms[12] = (SDL_FRect){200, h-550, 100, 20};
    levels[4].platforms[13] = (SDL_FRect){400, h-650, 100, 20};
    levels[4].platforms[14] = (SDL_FRect){w-200, h-100, 200, 50};

    levels[4].num_lava = 4;
    levels[4].lava_squares[0] = (SDL_FRect){150, h-45, 100, 50};
    levels[4].lava_squares[1] = (SDL_FRect){310, h-45, 240, 50};
    levels[4].lava_squares[2] = (SDL_FRect){610, h-45, 240, 50};
    levels[4].lava_squares[3] = (SDL_FRect){380, h-345, 120, 100};

    levels[4].start_pos = (SDL_FRect){50, h-150, 40, 40};
    levels[4].goal = (SDL_FRect){w-150, h-200, 50, 50};

    levels[4].num_collectibles = 7;
    levels[4].level_collectibles[0] = (Collectible){{275, h-170, 20, 20}, 0, 0};
    levels[4].level_collectibles[1] = (Collectible){{575, h-170, 20, 20}, 0, 0};
    levels[4].level_collectibles[2] = (Collectible){{875, h-200, 20, 20}, 0, 0};
    levels[4].level_collectibles[3] = (Collectible){{925, h-400, 20, 20}, 0, 0};
    levels[4].level_collectibles[4] = (Collectible){{325, h-500, 20, 20}, 0, 0};
    levels[4].level_collectibles[5] = (Collectible){{225, h-600, 20, 20}, 0, 0};
    levels[4].level_collectibles[6] = (Collectible){{425, h-700, 20, 20}, 0, 0};

    levels[4].num_moving = 4;
    levels[4].level_moving_platforms[0] = (MovingPlatform){{600, h-300, 60, 20}, 1, 0, 600, 750, 0, 0, 1};
    levels[4].level_moving_platforms[1] = (MovingPlatform){{150, h-250, 60, 20}, 0, -2, 0, 0, h-400, h-200, 1};
    levels[4].level_moving_platforms[2] = (MovingPlatform){{750, h-350, 60, 20}, 1, 0, 750, 900, 0, 0, 1};
    levels[4].level_moving_platforms[3] = (MovingPlatform){{300, h-200, 60, 20}, 2, 0, 300, 500, 0, 0, 1};

    // Level 5 - The Gauntlet (Final Challenge)
    levels[5].num_platforms = 20;
    levels[5].platforms[0] = (SDL_FRect){0, h-50, 120, 50};
    levels[5].platforms[1] = (SDL_FRect){180, h-150, 60, 20};
    levels[5].platforms[2] = (SDL_FRect){300, h-200, 40, 20};
    levels[5].platforms[3] = (SDL_FRect){400, h-150, 40, 20};
    levels[5].platforms[4] = (SDL_FRect){500, h-250, 60, 20};
    levels[5].platforms[5] = (SDL_FRect){620, h-180, 40, 20};
    levels[5].platforms[6] = (SDL_FRect){720, h-300, 60, 20};
    levels[5].platforms[7] = (SDL_FRect){840, h-220, 40, 20};
    levels[5].platforms[8] = (SDL_FRect){940, h-350, 60, 20};
    levels[5].platforms[9] = (SDL_FRect){1050, h-280, 40, 20};
    levels[5].platforms[10] = (SDL_FRect){950, h-450, 80, 20};
    levels[5].platforms[11] = (SDL_FRect){800, h-550, 60, 20};
    levels[5].platforms[12] = (SDL_FRect){650, h-450, 60, 20};
    levels[5].platforms[13] = (SDL_FRect){500, h-550, 60, 20};
    levels[5].platforms[14] = (SDL_FRect){350, h-450, 60, 20};
    levels[5].platforms[15] = (SDL_FRect){200, h-550, 60, 20};
    levels[5].platforms[16] = (SDL_FRect){100, h-650, 80, 20};
    levels[5].platforms[17] = (SDL_FRect){300, h-700, 100, 20};
    levels[5].platforms[18] = (SDL_FRect){500, h-750, 80, 20};
    levels[5].platforms[19] = (SDL_FRect){w-150, h-150, 150, 50};

    levels[5].num_lava = 5;
    levels[5].lava_squares[0] = (SDL_FRect){120, h-45, 60, 50};
    levels[5].lava_squares[1] = (SDL_FRect){240, h-45, 160, 50};
    levels[5].lava_squares[2] = (SDL_FRect){440, h-45, 180, 50};
    levels[5].lava_squares[3] = (SDL_FRect){660, h-45, 280, 50};
    levels[5].lava_squares[4] = (SDL_FRect){780, h-395, 60, 150};

    levels[5].start_pos = (SDL_FRect){25, h-150, 40, 40};
    levels[5].goal = (SDL_FRect){w-100, h-250, 50, 50};

    levels[5].num_collectibles = 8;
    levels[5].level_collectibles[0] = (Collectible){{205, h-200, 20, 20}, 0, 0};
    levels[5].level_collectibles[1] = (Collectible){{525, h-300, 20, 20}, 0, 0};
    levels[5].level_collectibles[2] = (Collectible){{745, h-350, 20, 20}, 0, 0};
    levels[5].level_collectibles[3] = (Collectible){{975, h-500, 20, 20}, 0, 0};
    levels[5].level_collectibles[4] = (Collectible){{675, h-500, 20, 20}, 0, 0};
    levels[5].level_collectibles[5] = (Collectible){{225, h-600, 20, 20}, 0, 0};
    levels[5].level_collectibles[6] = (Collectible){{325, h-750, 20, 20}, 0, 0};
    levels[5].level_collectibles[7] = (Collectible){{525, h-800, 20, 20}, 0, 0};

    levels[5].num_moving = 5;
    levels[5].level_moving_platforms[0] = (MovingPlatform){{250, h-300, 50, 20}, 1, 0, 250, 350, 0, 0, 1};
    levels[5].level_moving_platforms[1] = (MovingPlatform){{450, h-350, 50, 20}, 0, -1, 0, 0, h-500, h-300, 1};
    levels[5].level_moving_platforms[2] = (MovingPlatform){{600, h-350, 50, 20}, 1, 0, 600, 700, 0, 0, 1};
    levels[5].level_moving_platforms[3] = (MovingPlatform){{400, h-600, 60, 20}, 2, 0, 400, 550, 0, 0, 1};
    levels[5].level_moving_platforms[4] = (MovingPlatform){{200, h-400, 50, 20}, 0, -2, 0, 0, h-600, h-350, 1};

    // Set time limits for each level (in seconds)
    levels[0].time_limit = 90.0f;  // Tutorial - generous time
    levels[1].time_limit = 75.0f;  // Intermediate
    levels[2].time_limit = 60.0f;  // Advanced
    levels[3].time_limit = 90.0f;  // Vertical Challenge - needs more time for climbing
    levels[4].time_limit = 45.0f;  // Speed Run - tight time limit!
    levels[5].time_limit = 120.0f; // The Gauntlet - final challenge, more time needed
}

void load_level(int level_num)
{
    if (level_num >= MAX_LEVELS) return;

    Level *level = &levels[level_num];

    // Copy collectibles
    total_collectibles = level->num_collectibles;
    collected_count = 0;
    for (int i = 0; i < level->num_collectibles; i++) {
        collectibles[i] = level->level_collectibles[i];
        collectibles[i].collected = 0;
        collectibles[i].bob_offset = (float)(rand() % 100) / 100.0f * 6.28f;
    }

    // Copy moving platforms
    num_moving_platforms = level->num_moving;
    for (int i = 0; i < level->num_moving; i++) {
        moving_platforms[i] = level->level_moving_platforms[i];
    }

    // Initialize timer
    level_timer = level->time_limit;
    level_time_limit = level->time_limit;

    // Give double jump power-up on level 1+
    has_double_jump = (level_num > 0);
}

void reset_player(void)
{
    Level *level = &levels[current_level];
    player.x = level->start_pos.x;
    player.y = level->start_pos.y;
    player.w = 40;
    player.h = 40;
    player_vy = 0;
         player_rotation = 0;
    is_on_ground = 0;
    coyote_timer = 0;
    jump_buffer = 0;
    jump_held = 0;
    double_jump_used = 0;
    invincibility_timer = 0;

    // Reset timer
    level_timer = level->time_limit;
}

void update_collectibles(void)
{
    for (int i = 0; i < total_collectibles; i++) {
        if (!collectibles[i].collected) {
            collectibles[i].bob_offset += 0.1f;

            // Check collision with player
            if (is_colliding(player, collectibles[i].rect)) {
                collectibles[i].collected = 1;
                collected_count++;
                score += 100;

                // Collection particles
                for (int j = 0; j < 10; j++) {
                    add_particle(collectibles[i].rect.x + collectibles[i].rect.w/2,
                               collectibles[i].rect.y + collectibles[i].rect.h/2,
                               (float)(rand() % 20 - 10) / 5.0f,
                               (float)(rand() % 20 - 10) / 5.0f,
                               255, 255, 0, 50.0f);
                }
            }
        }
    }
}

void render_collectibles(void)
{
    for (int i = 0; i < total_collectibles; i++) {
        if (!collectibles[i].collected) {
            // Bobbing animation
            float bob = SDL_sin(collectibles[i].bob_offset) * 5.0f;
            SDL_FRect bobbing_rect = {
                collectibles[i].rect.x,
                collectibles[i].rect.y + bob,
                collectibles[i].rect.w,
                collectibles[i].rect.h
            };

            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255); // Yellow
            SDL_RenderFillRect(renderer, &bobbing_rect);

            // Glow effect
            SDL_SetRenderDrawColor(renderer, 255, 255, 200, 100);
            SDL_FRect glow = {bobbing_rect.x - 2, bobbing_rect.y - 2,
                            bobbing_rect.w + 4, bobbing_rect.h + 4};
            SDL_RenderFillRect(renderer, &glow);
        }
    }
}

void update_moving_platforms(void)
{
    for (int i = 0; i < num_moving_platforms; i++) {
        MovingPlatform *platform = &moving_platforms[i];

        platform->rect.x += platform->vx;
        platform->rect.y += platform->vy;

        // Horizontal movement bounds
        if (platform->vx != 0) {
            if (platform->rect.x <= platform->start_x || platform->rect.x >= platform->end_x) {
                platform->vx = -platform->vx;
            }
        }

        // Vertical movement bounds
        if (platform->vy != 0) {
            if (platform->rect.y <= platform->start_y || platform->rect.y >= platform->end_y) {
                platform->vy = -platform->vy;
            }
        }
    }
}

void render_moving_platforms(void)
{
    SDL_SetRenderDrawColor(renderer, 150, 100, 200, 255); // Purple
    for (int i = 0; i < num_moving_platforms; i++) {
        SDL_RenderFillRect(renderer, &moving_platforms[i].rect);
    }
}

int is_colliding(SDL_FRect a, SDL_FRect b)
{
    return (a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y);
}

void render_hud(void)
{
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    // Score
    char score_text[64];
    SDL_snprintf(score_text, sizeof(score_text), "Score: %d", score);
    SDL_RenderDebugText(renderer, 10, 10, score_text);

    // Lives
    char lives_text[32];
    SDL_snprintf(lives_text, sizeof(lives_text), "Lives: %d", lives);
    SDL_RenderDebugText(renderer, 10, 30, lives_text);

    // Level
    char level_text[32];
    SDL_snprintf(level_text, sizeof(level_text), "Level: %d", current_level + 1);
    SDL_RenderDebugText(renderer, 10, 50, level_text);

    // Collectibles
    char collectible_text[64];
    SDL_snprintf(collectible_text, sizeof(collectible_text), "Gems: %d/%d", collected_count, total_collectibles);
    SDL_RenderDebugText(renderer, 10, 70, collectible_text);

    // Timer
    char timer_text[32];
    int minutes = (int)(level_timer / 60.0f);
    int seconds = (int)(level_timer) % 60;
    SDL_snprintf(timer_text, sizeof(timer_text), "Time: %02d:%02d", minutes, seconds);

    // Change color based on remaining time
    if (level_timer <= 10.0f) {
        SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255); // Red when low
    } else if (level_timer <= 30.0f) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255); // Yellow when medium
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // White when plenty
    }
    SDL_RenderDebugText(renderer, 10, 90, timer_text);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // Reset color

    // Power-ups
    if (has_double_jump) {
        SDL_RenderDebugText(renderer, 10, 110, "Double Jump: ON");
    }

    // Instructions
    if (game_over) {
        SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255);
        SDL_RenderDebugText(renderer, w/2 - 100, h/2 - 50, "GAME OVER");
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        if (level_timer <= 0) {
            SDL_RenderDebugText(renderer, w/2 - 60, h/2 - 30, "TIME'S UP!");
        }
        SDL_RenderDebugText(renderer, w/2 - 80, h/2 - 20, "Press R to restart");
    } else if (game_won) {
        SDL_SetRenderDrawColor(renderer, 100, 255, 100, 255);
        if (current_level < MAX_LEVELS - 1) {
            SDL_RenderDebugText(renderer, w/2 - 80, h/2 - 50, "LEVEL COMPLETE!");
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDebugText(renderer, w/2 - 100, h/2 - 20, "Press N for next level");
            SDL_RenderDebugText(renderer, w/2 - 80, h/2, "Press R to restart");
        } else {
            SDL_RenderDebugText(renderer, w/2 - 100, h/2 - 50, "GAME COMPLETE!");
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDebugText(renderer, w/2 - 80, h/2 - 20, "Press R to restart");
        }
    } else {
        // Controls
        SDL_RenderDebugText(renderer, w - 300, 10, "Controls:");
        SDL_RenderDebugText(renderer, w - 300, 30, "Arrow Keys / WASD: Move");
        SDL_RenderDebugText(renderer, w - 300, 50, "Space / Up: Jump");
        if (has_double_jump) {
            SDL_RenderDebugText(renderer, w - 300, 70, "Double Jump Available!");
        }
        SDL_RenderDebugText(renderer, w - 300, 90, "Collect all gems to win!");
    }
}

void render_background(void)
{
    // Gradient background
    for (int y = 0; y < h; y++) {
        float ratio = (float)y / h;
        Uint8 r = (Uint8)(20 + ratio * 60);
        Uint8 g = (Uint8)(30 + ratio * 80);
        Uint8 b = (Uint8)(60 + ratio * 120);
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_RenderLine(renderer, 0, y, w, y);
    }
}

