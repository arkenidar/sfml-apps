// maze/images example: a tile-based maze rendered from BMP images.
//
// Build with CMake (see CMakeLists.txt). Needs an assets/ directory containing
// P.bmp, -.bmp, W.bmp, E.bmp and map01.txt..map03.txt next to (or one/two
// levels above) the executable.
//
// Self-contained: this file carries only the helper code the maze actually
// uses (asset loading, map handling, the input/event loop).

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Resolve a relative asset path against the executable's directory so the
// program works regardless of the current working directory it is launched
// from. Tries, in order: next to the executable, one level up, then two levels
// up, and finally the original relative path as a fallback. Returns a malloc'd
// string; the caller must free() it.
static char *asset_path(const char *relative)
{
    const char *base = SDL_GetBasePath(); // owned by SDL; do not free
    char *result = NULL;
    if (base)
    {
        const char *prefixes[] = {"", "../", "../../"};
        for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++)
        {
            size_t len = strlen(base) + strlen(prefixes[i]) + strlen(relative) + 1;
            char *path = malloc(len);
            sprintf(path, "%s%s%s", base, prefixes[i], relative);
            FILE *f = fopen(path, "r");
            if (f)
            {
                fclose(f);
                result = path;
                break;
            }
            free(path);
        }
    }
    if (!result)
    {
        result = malloc(strlen(relative) + 1);
        strcpy(result, relative);
    }
    return result;
}

static int px = 0, py = 0;

static char *worlds[] = {
    "assets/map01.txt",
    "assets/map02.txt",
    "assets/map03.txt"};
static int worlds_current = -1;

typedef struct map_struct
{
    int width, height;
    char **matrix;
} Map;

static Map map = {.matrix = NULL, .width = 0, .height = 0};

static void map_unload(Map *map)
{
    for (int i = 0; i < map->height; i++)
        free(map->matrix[i]);
    free(map->matrix);

    map->matrix = NULL;
    map->width = 0;
    map->height = 0;
}

static void map_post_load(Map *map)
{
    for (int yi = 0; yi < map->height; yi++)
        for (int xi = 0; xi < map->width; xi++)
        {
            char map_char = map->matrix[yi][xi];
            if (map_char == 'P')
            {
                map->matrix[yi][xi] = ' ';
                px = xi;
                py = yi;
            }
        }
}

static void map_load(Map *map, char *file_path)
{
    map_unload(map);
    int map_width = -1;
    int map_height = 0;
    char **map_matrix = (char **)calloc(1, sizeof(char *));
    char *full_path = asset_path(file_path);
    FILE *file = fopen(full_path, "r");
    free(full_path);
    char line[1001];
    while (fgets(line, 1001, file))
    {
        if (map_width == -1)
            map_width = strlen(line) - 1;
        line[map_width] = '\0';

        map_height += 1;

        map_matrix = (char **)realloc(map_matrix, sizeof(char *) * map_height);
        map_matrix[map_height - 1] = calloc(map_width + 1, sizeof(char));
        strcpy(map_matrix[map_height - 1], line);
    }

    map->height = map_height;
    map->width = map_width;
    map->matrix = map_matrix;

    map_post_load(map);
}

// Log an SDL error when a bool-returning SDL call reports failure.
static void ensure(bool ok)
{
    if (!ok)
        printf("SDL error: %s\n", SDL_GetError());
}

static float mx, my;
static SDL_MouseButtonFlags mbuttons;
static int mouse_left_down, mouse_left_just_down, mouse_left_down_previous = 0;

// Pump SDL events and update input/maze state. Returns 0 to quit, 1 to keep
// running.
static int events(void)
{

    SDL_PumpEvents();
    mbuttons = SDL_GetMouseState(&mx, &my);

    mouse_left_down = mbuttons & SDL_BUTTON_LMASK;
    mouse_left_just_down = mouse_left_down && !mouse_left_down_previous;
    mouse_left_down_previous = mouse_left_down;

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {

        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
            return 0;

        switch (event.type)
        {

        case SDL_EVENT_QUIT:
            return 0;

        case SDL_EVENT_KEY_DOWN:

            int tx = px;
            int ty = py;
            switch (event.key.key)
            {
            case SDLK_DOWN:
                ty++;
                break;
            case SDLK_UP:
                ty--;
                break;
            case SDLK_RIGHT:
                tx++;
                break;
            case SDLK_LEFT:
                tx--;
                break;
            }

            if (tx < map.width && ty < map.height && tx >= 0 && ty >= 0)
            {

                char going = map.matrix[ty][tx];
                if (going != '#')
                {
                    px = tx;
                    py = ty;
                    if (going == 'E')
                        map_load(&map, worlds[++worlds_current]);
                }
            }

            break;
        }
    }
    return 1;
}

int main(int argc, char *argv[])
{
    ensure(SDL_Init(SDL_INIT_VIDEO));
    int view_width = 400, view_height = 300;
    SDL_Window *window;
    SDL_Renderer *renderer;
    ensure(SDL_CreateWindowAndRenderer("mini-maze with (lib)SDL3", view_width, view_height, SDL_WINDOW_RESIZABLE, &window, &renderer));

    char *filename[4] = {
        "assets/P.bmp",
        "assets/-.bmp",
        "assets/W.bmp",
        "assets/E.bmp"};
    const int count = 4;

    SDL_Surface *image[count];
    SDL_Texture *texture[count];
    for (int i = 0; i < count; i++)
    {
        char *full_path = asset_path(filename[i]);
        image[i] = SDL_LoadBMP(full_path);
        free(full_path);
        if (image[i] == NULL)
        {
            printf("file-name not found: [[ %s ]]\n", filename[i]);
            return 1;
        }
        texture[i] = SDL_CreateTextureFromSurface(renderer, image[i]);
    }

    map_load(&map, worlds[++worlds_current]);

    while (events())
    {
        const int tile_size = 32;

        if (mouse_left_down)
        {
            int tx = (int)mx / tile_size;
            int ty = (int)my / tile_size;
            if (tx < map.width && ty < map.height)
            {
                if (
                    (abs(tx - px) == 1 && ty == py) || (abs(ty - py) == 1 && tx == px))
                {
                    char going = map.matrix[ty][tx];
                    if (going != '#')
                    {
                        px = tx;
                        py = ty;
                        if (going == 'E')
                            map_load(&map, worlds[++worlds_current]);
                    }
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        for (int yi = 0; yi < map.height; yi++)
            for (int xi = 0; xi < map.width; xi++)
            {
                char map_char = map.matrix[yi][xi];
                if (yi == py && xi == px)
                    map_char = 'P';
                int map_tile_type = 1;
                switch (map_char)
                {
                case 'P':
                    map_tile_type = 0;
                    break;
                case ' ':
                    map_tile_type = 1;
                    break;
                case '#':
                    map_tile_type = 2;
                    break;
                case 'E':
                    map_tile_type = 3;
                    break;
                }
                SDL_FRect dst_rect = {xi * tile_size, yi * tile_size, tile_size, tile_size};
                SDL_RenderTexture(renderer, texture[map_tile_type], NULL, &dst_rect);
            }

        SDL_RenderPresent(renderer);
    }

    map_unload(&map);

    for (int i = 0; i < count; i++)
    {
        SDL_DestroyTexture(texture[i]);
        SDL_DestroySurface(image[i]);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
    return 0;
}
