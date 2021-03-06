/***********************************************************
 * Lambda/8
 *
 * A Fictional Computer using Lisp as its system language
 **********************************************************/

#include <stdio.h>
#include <string.h>
#include <float.h>
#include <unistd.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>

#include "s7.h"

#include "api.h"
#include "terminal.h"
#include "buffer.h"
#include "editor.h"

typedef enum {
    GAME,
    EDITOR
} machine_modes;

machine_modes machine_mode = GAME;

//Starts up SDL and creates window
int init();

//Frees media and shuts down SDL
void myclose();

//The window we'll be rendering to
SDL_Window* gWindow = NULL;

//The window renderer
SDL_Renderer* gRenderer = NULL;

// The font
TTF_Font *gFont = NULL;

// Sprite textures (8 7-sprites, one per image)
#define MAX_SPRITES 256
SDL_Texture *gSprites[MAX_SPRITES] = { NULL };
int gMaxSprite = -1;

// Sound effects, not sure about the limit yet
Mix_Chunk *gSfx[72] = { NULL };
int gMaxSfx = -1;

// the terminal
terminal_t *gTerminal;

// the editor buffer
buffer_t *gBuffer;

// the script
char *gScript;
char *gScriptFilename;

void printText(const char *str, int x, int y) {
    SDL_Color color = { 255, 255, 255 };

    SDL_Surface *surface = TTF_RenderText_Solid(gFont, str, color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(gRenderer, surface);

    int texW = 0;
    int texH = 0;
    SDL_QueryTexture(texture, NULL, NULL, &texW, &texH);
    SDL_Rect rect = {x, y, texW, texH};
    SDL_RenderCopy(gRenderer, texture, NULL, &rect);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void printTextInverse(const char *str, int x, int y) {
    SDL_Color color = { 0, 255, 255 };

    SDL_Surface *surface = TTF_RenderText_Blended(gFont, str, color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(gRenderer, surface);

    int texW = 0;
    int texH = 0;
    SDL_QueryTexture(texture, NULL, NULL, &texW, &texH);
    SDL_Rect rect = {x, y, texW, texH};
    SDL_RenderCopy(gRenderer, texture, NULL, &rect);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

// FIXME get the size from the font iself... or something
void L8RenderTerminal(terminal_t* term) {
    // print line by line
    for (int i = 0, y = 0; i < term->w * term->h; i += term->w) {
        char *str = strndup(&term->buffer[i], term->w);

        printText(str, 0, y*8);
        ++y;
        free(str);
    }
}

void L8RenderCursor(terminal_t *term) {
    printTextInverse("_", term->column*8, term->row*8);
}


/*************************************************
 ** MAIN CODE
 ************************************************/
int init()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
        return 0;
    }

    /* if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) { */
    /*     printf("Warning: Linear texture filtering not enabled!"); */
    /* } */

    /* if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0")) { */
    /*     printf("Warning: Linear texture filtering not enabled!"); */
    /* } */

    gWindow = SDL_CreateWindow("Lambda/8",
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                SCREEN_WIDTH, SCREEN_HEIGHT,
                                SDL_WINDOW_SHOWN);

    if (gWindow == NULL) {
        printf("Window could not be created! SDL Error: %s\n", SDL_GetError());
        return 0;
    }

    gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED /* | SDL_RENDERER_PRESENTVSYNC */);
    if (gRenderer == NULL) {
        printf("Renderer could not be created! SDL Error: %s\n", SDL_GetError());
        return 0;
    }

    SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);

    SDL_RenderSetLogicalSize(gRenderer, L8_WIDTH, L8_HEIGHT);

    int imgFlags = IMG_INIT_PNG;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        printf("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
        return 0;
    }

    if (Mix_OpenAudio(22050, AUDIO_U8, 2, 2048) < 0) {
        printf("SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError());
        return 0;
    }

    if (TTF_Init() < 0) {
        printf("SDL_TTF could not initialize! SDL_image Error: %s\n", TTF_GetError());
        return 0;
    }

    if ((gFont = TTF_OpenFont("font.ttf", 8)) == NULL) {
        printf("TTF font could not initialize! SDL_image Error: %s\n", TTF_GetError());
        return 0;
    }

    // create the editor buffer
    gBuffer = Buffer_Create("scratch");

    // configure the terminal
    gTerminal = newTerminal(40, 25, L8RenderTerminal, L8RenderCursor);

    return 1;
}

void myclose()
{
    free(gTerminal);
    free(gBuffer);

    //Free loaded images
    for (int i = 0; gSprites[i]; ++i) {
        if (gSprites[i]) {
            SDL_DestroyTexture(gSprites[i]);
            gSprites[i] = NULL;
        }
    }

    for (int i = 0; gSfx[i]; ++i) {
        if (gSfx[i]) {
            Mix_FreeChunk(gSfx[i]);
            gSfx[i] = NULL;
        }
    }

    //Destroy window
    SDL_DestroyRenderer(gRenderer);
    SDL_DestroyWindow(gWindow);
    gWindow = NULL;
    gRenderer = NULL;

    //Quit SDL subsystems
    TTF_CloseFont(gFont);
    TTF_Quit();
    Mix_Quit();
    IMG_Quit();
    SDL_Quit();
}

char *load_file(const char *filename) {
    char *res;
    int r, size;
    FILE *fp = fopen(filename, "rb");
    if (!fp) printf("Could not open file %s\n", filename);

    /* Get size */
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* Load file into string value */
    res = malloc(size+1);
    res[size] = '\0';

    r = fread(res, 1, size, fp);
    fclose(fp);

    if (r != size) printf("Could not read file %s\n", filename);

    return res;
}

int load_script(s7_scheme *sc, const char *filename) {
    gScriptFilename = (char *) filename;
    gScript = load_file(gScriptFilename);
    s7_eval_c_string(sc, gScript);

    return 1;
}

int main(int argc, char* argv[]) {
    s7_scheme *sc = s7_init();
    s7_scheme *ES = s7_init();

    // bail out if aria fails to initialise
    if (!sc || !ES) {
        printf("out of memory\n");
        return EXIT_FAILURE;
    }

    // use game.lsp as default filename
    gScriptFilename = argc < 2 ? "game.lsp" : argv[1];

    // bail out if the file can't be loaded
    if (!access(gScriptFilename, R_OK) == 0) {
        printf("Cannot load %s\n", gScriptFilename);
        return 1;
    }

    //Start up SDL and create window
    if (!init())
    {
        printf("Failed to initialize SDL!\n");
        exit(-1);
    }

    // init the machine's Lisp
    initMachineLisp(sc);
    load_script(sc, gScriptFilename);

    // init the editor's Lisp
    initEditorLisp(ES);


    //Main loop flag
    int quit = 0;

    //Event handler
    SDL_Event e;
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    int keys = 0;
    int lastkeys = 0;

    //Start counting frames per second
    int countedFrames = 0;

    //While application is running
    while (!quit) {
        int startFrame = SDL_GetTicks();
        lastkeys = keys;
        keys = 0;

        if (machine_mode == GAME) {
            //Handle events on queue
            while (SDL_PollEvent(&e) != 0) {
                if (e.type == SDL_QUIT) {
                    quit = 1;
                }
                else if (e.type == SDL_KEYDOWN) {
                    switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        quit = 1;
                        break;
                    case SDLK_r:
                        if (e.key.keysym.mod & KMOD_CTRL) {
                            s7_eval_c_string(sc, gScript);
                        }
                        break;
                    case SDLK_F1:
                        //SDL_RenderSetLogicalSize(gRenderer, L8_WIDTH, L8_HEIGHT);
                        machine_mode = GAME;
                        break;
                    case SDLK_F2:
                        machine_mode = EDITOR;
                        /* SDL_RenderSetLogicalSize(gRenderer, */
                        /*                          SCREEN_WIDTH, SCREEN_HEIGHT); */
                        Buffer_Clear(gBuffer);
                        Insert_String(gBuffer, gScript);
                        Set_Modified(gBuffer, 0);
                        break;
                    default:
                        break;
                    }
                }
            }

            // get keys
            if (state[SDL_SCANCODE_UP])    { keys |= (1 << 0); }
            if (state[SDL_SCANCODE_DOWN])  { keys |= (1 << 1); }
            if (state[SDL_SCANCODE_LEFT])  { keys |= (1 << 2); }
            if (state[SDL_SCANCODE_RIGHT]) { keys |= (1 << 3); }

            if (state[SDL_SCANCODE_Z])     { keys |= (1 << 4); }
            if (state[SDL_SCANCODE_X])     { keys |= (1 << 5); }
            if (state[SDL_SCANCODE_A])     { keys |= (1 << 6); }
            if (state[SDL_SCANCODE_S])     { keys |= (1 << 7); }

            s7_define_variable(sc, "LASTKEYS", s7_make_integer(sc, lastkeys));
            s7_define_variable(sc, "KEYS", s7_make_integer(sc, keys));

            // get mouse
            int buttons, x, y;
            buttons = SDL_GetMouseState(&x, &y);

            // scale the physical coordinates to logical coordinates
            //ar_bind_global(S, "MOUSEX", ar_new_number(S, x * L8_WIDTH / SCREEN_WIDTH));
            // ar_bind_global(S, "MOUSEY", ar_new_number(S, y * L8_HEIGHT / SCREEN_HEIGHT));
            //ar_bind_global(S, "BUTTONS", ar_new_number(S, buttons));

            //Clear screen
            //SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
            //SDL_RenderClear(gRenderer);

            // run lisp
            s7_call(sc, s7_name_to_value(sc, "update"), s7_nil(sc));
        }
        else if (machine_mode == EDITOR) {
            //Handle events on queue
            while (SDL_PollEvent(&e) != 0) {
                //User requests quit
                if (e.type == SDL_QUIT) {
                    quit = 1;
                }
                else if (e.type == SDL_KEYDOWN) {
                    char keystr[64] = {'\0'};
                    int mods = 0;

                    // CONSIDER HOW TO IMPLEMENT MOUSE EVENTS

                    /* // get mouse */
                    /* int buttons, x, y; */
                    /* buttons = SDL_GetMouseState(&x, &y); */

                    /* // scale the physical coordinates to logical coordinates */
                    /* ar_bind_global(S, "MOUSEX", ar_new_number(S, x * L8_WIDTH / SCREEN_WIDTH)); */
                    /* ar_bind_global(S, "MOUSEY", ar_new_number(S, y * L8_HEIGHT / SCREEN_HEIGHT)); */
                    /* ar_bind_global(S, "BUTTONS", ar_new_number(S, buttons)); */

                    switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        quit = 1;
                        break;
                    case SDLK_F1:
                        /* SDL_RenderSetLogicalSize(gRenderer, L8_WIDTH, L8_HEIGHT); */
                        machine_mode = GAME;
                        break;
                    case SDLK_F2:
                        /* SDL_RenderSetLogicalSize(gRenderer, */
                        /*                          SCREEN_WIDTH, SCREEN_HEIGHT); */
                        machine_mode = EDITOR;
                        break;
                    case SDLK_F5:
                        //save
                        free(gScript);
                        gScript = renderGapBuffer(gBuffer->contents);
                        break;
                    case SDLK_F6:
                        // load
                        Buffer_Clear(gBuffer);
                        Insert_String(gBuffer, gScript);
                        break;

                    case SDLK_F9:
                        // load
                        s7_load(ES, "editor.lsp");
                        Buffer_Clear(gBuffer);
                        Insert_String(gBuffer, gScript);
                        break;

                    case SDLK_RETURN:
                        Insert_Char(gBuffer, '\n');
                        break;
                    case SDLK_BACKSPACE:
                        if (gBuffer->point > 0) {
                            Point_Move(gBuffer, -1);
                            Delete(gBuffer, 1);
                        }
                        break;
                    case SDLK_DELETE:
                        if (gBuffer->point < gBuffer->contents->count) {
                            Delete(gBuffer, 1);
                        }
                        break;
                    case SDLK_LEFT:
                        snprintf(keystr, 63,
                                 "(handle-key \"<left>\" %d)",
                                 mods);
                        s7_eval_c_string(ES, keystr);
                        break;
                    case SDLK_RIGHT:
                        snprintf(keystr, 63,
                                 "(handle-key \"<right>\" %d)",
                                 mods);
                        s7_eval_c_string(ES, keystr);
                        break;
                    case SDLK_UP:
                        snprintf(keystr, 63,
                                 "(handle-key \"<up>\" %d)",
                                 mods);
                        s7_eval_c_string(ES, keystr);
                        break;
                    case SDLK_DOWN:
                        snprintf(keystr, 63,
                                 "(handle-key \"<down>\" %d)",
                                 mods);
                        s7_eval_c_string(ES, keystr);
                        break;
                    case SDLK_HOME:
                        Point_Set(gBuffer, 0);
                        break;
                    case SDLK_END:
                        Point_Set(gBuffer, gBuffer->contents->count);
                        break;
                    default:
                        if (e.key.keysym.mod & KMOD_SHIFT)
                            mods |= 1;
                        if (e.key.keysym.mod & KMOD_CTRL)
                            mods |= 2;
                        if (e.key.keysym.mod & KMOD_ALT)
                            mods |= 4;
                        if (e.key.keysym.sym == SDLK_BACKSLASH) {
                            snprintf(keystr, 63,
                                     "(handle-key \"\\\\\" %d)",
                                     mods);
                        }
                        else {
                            snprintf(keystr, 63,
                                     "(handle-key \"%c\" %d)",
                                     e.key.keysym.sym,
                                     mods);
                        }
                        s7_eval_c_string(ES, keystr);
                        break;
                    }
                }
            }

            SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
            SDL_RenderClear(gRenderer);

            // tell the buffer to draw to the terminal
            Buffer_Render_Screen(gBuffer, gTerminal);
        }

        //Update screen
        SDL_RenderPresent(gRenderer);
        ++countedFrames;

        //If frame finished early
        int frameTicks = SDL_GetTicks() - startFrame;
        if (frameTicks < SCREEN_TICKS_PER_FRAME) {
            //Wait remaining time
            SDL_Delay(SCREEN_TICKS_PER_FRAME - frameTicks);
        }
    }

    //Free resources and close SDL
    myclose();

    free(sc);

    return 0;
}
