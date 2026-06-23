// maze: a tile-based maze rendered from BMP images.
//
// C++/SFML 3 port of the original SDL3 program (see ../../maze.c). Behavior is
// kept the same: arrow keys / clicks on adjacent tiles move the player around a
// map loaded from a text file; walls block movement; stepping on the exit
// advances to the next map (wrapping back to the first after the last).
//
// Needs an assets/ directory containing P.bmp, -.bmp, W.bmp, E.bmp and
// map01.txt..map03.txt next to (or one/two levels above) the executable.

#include <SFML/Graphics.hpp>

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr float kTileSize = 32.f;

// The four tile kinds, ordered to match the texture/asset arrays below.
enum class Tile { Player, Floor, Wall, Exit };

// Directory the running executable lives in, used to resolve relative asset
// paths regardless of the current working directory.
fs::path executable_dir() {
#if defined(__linux__)
    std::error_code ec;
    if (fs::path exe = fs::read_symlink("/proc/self/exe", ec); !ec)
        return exe.parent_path();
#endif
    return fs::current_path();
}

// Resolve a relative asset path against the executable's directory, searching
// next to the executable, then one and two levels up.
fs::path asset_path(std::string_view relative) {
    const fs::path base = executable_dir();
    for (std::string_view prefix : {"", "../", "../../"}) {
        fs::path candidate = base / (std::string(prefix) + std::string(relative));
        if (fs::exists(candidate))
            return candidate;
    }
    return relative;
}

// A maze level: a rectangular grid of characters plus the player's start cell.
class Map {
public:
    Map() = default;

    explicit Map(const fs::path& path) {
        std::ifstream file(path);
        if (!file)
            throw std::runtime_error("map not found: " + path.string());

        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r')  // tolerate CRLF endings
                line.pop_back();
            if (rows_.empty())
                width_ = static_cast<int>(line.size());
            line.resize(width_, ' ');  // keep every row the same width
            rows_.push_back(std::move(line));
        }

        // Extract the player start ('P') and blank that cell.
        for (int y = 0; y < height(); ++y)
            for (int x = 0; x < width_; ++x)
                if (rows_[y][x] == 'P') {
                    rows_[y][x] = ' ';
                    player_ = {x, y};
                }
    }

    int width() const { return width_; }
    int height() const { return static_cast<int>(rows_.size()); }
    char at(int x, int y) const { return rows_[y][x]; }
    bool contains(int x, int y) const {
        return x >= 0 && y >= 0 && x < width_ && y < height();
    }
    sf::Vector2i player() const { return player_; }

private:
    int width_ = 0;
    std::vector<std::string> rows_;
    sf::Vector2i player_{0, 0};
};

// Maps the on-screen character of a cell to its tile kind.
Tile tile_of(char c) {
    switch (c) {
        case 'P': return Tile::Player;
        case '#': return Tile::Wall;
        case 'E': return Tile::Exit;
        default:  return Tile::Floor;
    }
}

class Game {
public:
    Game()
        : window_(sf::VideoMode({400u, 300u}), "mini-maze with SFML",
                  sf::Style::Default) {
        constexpr std::array<std::string_view, 4> files = {
            "assets/P.bmp", "assets/-.bmp", "assets/W.bmp", "assets/E.bmp"};
        for (std::size_t i = 0; i < files.size(); ++i)
            if (!textures_[i].loadFromFile(asset_path(files[i])))
                throw std::runtime_error("texture not found: " +
                                         std::string(files[i]));
        load_next_world();
    }

    void run() {
        while (window_.isOpen()) {
            handle_events();
            handle_mouse();
            render();
        }
    }

private:
    void load_next_world() {
        world_ = (world_ + 1) % worlds_.size();
        map_ = Map(asset_path(worlds_[world_]));
        player_ = map_.player();
    }

    // Move onto (x, y) if it is in bounds and not a wall; the exit loads the
    // next world.
    void try_move(int x, int y) {
        if (!map_.contains(x, y))
            return;
        const char going = map_.at(x, y);
        if (going == '#')
            return;
        player_ = {x, y};
        if (going == 'E')
            load_next_world();
    }

    void handle_events() {
        while (const std::optional event = window_.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window_.close();
            } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                using Key = sf::Keyboard::Key;
                switch (key->code) {
                    case Key::Escape: window_.close(); break;
                    case Key::Up:     try_move(player_.x, player_.y - 1); break;
                    case Key::Down:   try_move(player_.x, player_.y + 1); break;
                    case Key::Left:   try_move(player_.x - 1, player_.y); break;
                    case Key::Right:  try_move(player_.x + 1, player_.y); break;
                    default: break;
                }
            }
        }
    }

    // Clicking an orthogonally adjacent tile moves the player onto it.
    void handle_mouse() {
        if (!sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
            return;
        const sf::Vector2f m =
            window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
        const int x = static_cast<int>(m.x / kTileSize);
        const int y = static_cast<int>(m.y / kTileSize);
        const bool adjacent = (std::abs(x - player_.x) == 1 && y == player_.y) ||
                              (std::abs(y - player_.y) == 1 && x == player_.x);
        if (adjacent)
            try_move(x, y);
    }

    void render() {
        window_.clear(sf::Color::Black);
        for (int y = 0; y < map_.height(); ++y)
            for (int x = 0; x < map_.width(); ++x) {
                const Tile tile = (x == player_.x && y == player_.y)
                                      ? Tile::Player
                                      : tile_of(map_.at(x, y));
                sf::Sprite sprite(textures_[static_cast<std::size_t>(tile)]);
                sprite.setPosition({x * kTileSize, y * kTileSize});
                window_.draw(sprite);
            }
        window_.display();
    }

    sf::RenderWindow window_;
    std::array<sf::Texture, 4> textures_;
    std::array<std::string_view, 3> worlds_ = {
        "assets/map01.txt", "assets/map02.txt", "assets/map03.txt"};
    std::size_t world_ = static_cast<std::size_t>(-1);
    Map map_;
    sf::Vector2i player_{0, 0};
};

}  // namespace

int main() {
    try {
        Game().run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
    return 0;
}
