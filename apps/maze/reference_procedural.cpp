// REFERENCE ONLY — not part of the build (only main.cpp is compiled).
//
// This is the first, more procedural C++/SFML 3 port of maze.c, kept for
// reference next to the idiomatic version in main.cpp. It stays close to the
// original C structure: free functions, file-scope state, a plain Map struct.

#include <SFML/Graphics.hpp>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Directory the running executable lives in, used to resolve relative asset
// paths regardless of the current working directory. Falls back to the current
// path if the executable location cannot be determined.
static fs::path executable_dir()
{
#if defined(__linux__)
    std::error_code ec;
    fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (!ec)
        return exe.parent_path();
#endif
    return fs::current_path();
}

// Resolve a relative asset path against the executable's directory. Tries, in
// order: next to the executable, one level up, then two levels up, and finally
// the original relative path as a fallback.
static std::string asset_path(const std::string &relative)
{
    const fs::path base = executable_dir();
    for (const char *prefix : {"", "../", "../../"})
    {
        fs::path candidate = base / (std::string(prefix) + relative);
        if (fs::exists(candidate))
            return candidate.string();
    }
    return relative;
}

struct Map
{
    int width = 0;
    int height = 0;
    std::vector<std::string> matrix;
};

static int px = 0, py = 0;

static const std::array<std::string, 3> worlds = {
    "assets/map01.txt",
    "assets/map02.txt",
    "assets/map03.txt"};
static int worlds_current = -1;

// After loading, extract the player start position 'P' and blank that tile.
static void map_post_load(Map &map)
{
    for (int yi = 0; yi < map.height; yi++)
        for (int xi = 0; xi < map.width; xi++)
            if (map.matrix[yi][xi] == 'P')
            {
                map.matrix[yi][xi] = ' ';
                px = xi;
                py = yi;
            }
}

static void map_load(Map &map, const std::string &file_path)
{
    map = Map{};

    std::ifstream file(asset_path(file_path));
    if (!file)
    {
        std::cerr << "map not found: [[ " << file_path << " ]]\n";
        std::exit(1);
    }

    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r') // tolerate CRLF line endings
            line.pop_back();
        if (map.width == 0)
            map.width = static_cast<int>(line.size());
        line.resize(map.width, ' '); // keep every row the same width
        map.matrix.push_back(line);
    }
    map.height = static_cast<int>(map.matrix.size());

    map_post_load(map);
}

// Advance to the next world map, wrapping back to the first after the last one.
// (The original C version indexed past the array end here and crashed.)
static void next_world(Map &map)
{
    worlds_current = (worlds_current + 1) % static_cast<int>(worlds.size());
    map_load(map, worlds[worlds_current]);
}

// Try to move the player onto tile (tx, ty) if it is in bounds and not a wall.
// Reaching the exit ('E') loads the next map.
static void try_move(Map &map, int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= map.width || ty >= map.height)
        return;

    char going = map.matrix[ty][tx];
    if (going == '#')
        return;

    px = tx;
    py = ty;
    if (going == 'E')
        next_world(map);
}

int main()
{
    sf::RenderWindow window(sf::VideoMode({400u, 300u}), "mini-maze with SFML",
                            sf::Style::Default);

    const std::array<std::string, 4> filename = {
        "assets/P.bmp",
        "assets/-.bmp",
        "assets/W.bmp",
        "assets/E.bmp"};

    std::array<sf::Texture, 4> textures;
    for (std::size_t i = 0; i < filename.size(); i++)
    {
        if (!textures[i].loadFromFile(asset_path(filename[i])))
        {
            std::cerr << "file-name not found: [[ " << filename[i] << " ]]\n";
            return 1;
        }
    }

    Map map;
    next_world(map);

    const float tile_size = 32.f;

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (const auto *key = event->getIf<sf::Event::KeyPressed>())
            {
                int tx = px, ty = py;
                switch (key->code)
                {
                case sf::Keyboard::Key::Escape:
                    window.close();
                    break;
                case sf::Keyboard::Key::Down:
                    ty++;
                    break;
                case sf::Keyboard::Key::Up:
                    ty--;
                    break;
                case sf::Keyboard::Key::Right:
                    tx++;
                    break;
                case sf::Keyboard::Key::Left:
                    tx--;
                    break;
                default:
                    break;
                }
                try_move(map, tx, ty);
            }
        }

        // Mouse: move onto a clicked tile only when it is orthogonally adjacent.
        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
        {
            sf::Vector2f m =
                window.mapPixelToCoords(sf::Mouse::getPosition(window));
            int tx = static_cast<int>(m.x / tile_size);
            int ty = static_cast<int>(m.y / tile_size);
            if ((std::abs(tx - px) == 1 && ty == py) ||
                (std::abs(ty - py) == 1 && tx == px))
                try_move(map, tx, ty);
        }

        window.clear(sf::Color::Black);

        for (int yi = 0; yi < map.height; yi++)
            for (int xi = 0; xi < map.width; xi++)
            {
                char map_char = map.matrix[yi][xi];
                if (yi == py && xi == px)
                    map_char = 'P';

                int tile_type = 1; // default: floor
                switch (map_char)
                {
                case 'P':
                    tile_type = 0;
                    break;
                case ' ':
                    tile_type = 1;
                    break;
                case '#':
                    tile_type = 2;
                    break;
                case 'E':
                    tile_type = 3;
                    break;
                }

                sf::Sprite sprite(textures[tile_type]);
                sprite.setPosition({xi * tile_size, yi * tile_size});
                window.draw(sprite);
            }

        window.display();
    }

    return 0;
}
