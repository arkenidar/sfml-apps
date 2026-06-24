// maze: a tile-based maze rendered from BMP images.
//
// C++/SFML port of the original SDL3 program (see ../../maze.c). Builds against
// both SFML 2.x (Termux, CxxDroid) and SFML 3.x (Debian); the few places where
// the APIs diverge are guarded by SFML_VERSION_MAJOR. Behavior is kept the
// same: arrow keys / clicks on adjacent tiles move the player around a map
// loaded from a text file; walls block movement; stepping on the exit advances
// to the next map (wrapping back to the first after the last).
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

// Resolve a relative asset path by searching next to the executable, then one
// and two levels up, and likewise relative to the current working directory.
// The cwd bases matter on CxxDroid: it copies the binary out of (noexec)
// storage to launch it, so the executable's directory no longer holds the
// assets, but the working directory still points at the project.
fs::path asset_path(std::string_view relative) {
    const fs::path bases[] = {executable_dir(), fs::current_path()};
    for (const fs::path& base : bases)
        for (std::string_view prefix : {"", "../", "../../"}) {
            fs::path candidate =
                base / (std::string(prefix) + std::string(relative));
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

// Build an sf::FloatRect from (left, top, width, height). SFML 3 reshaped the
// constructor to take position/size vectors, so wrap the difference here.
sf::FloatRect make_rect(float left, float top, float width, float height) {
#if SFML_VERSION_MAJOR >= 3
    return sf::FloatRect({left, top}, {width, height});
#else
    return sf::FloatRect(left, top, width, height);
#endif
}

class Game {
public:
    // A borderless window the size of the whole display. True fullscreen on X11
    // relies on window-manager hints, which Termux:X11 lacks (no WM), so the
    // window wouldn't actually fill the screen; a borderless window sized at
    // creation does, with no WM and no video-mode switch. It matches CxxDroid's
    // native fullscreen too, and the letterboxed view scales the maze to fit.
    // Press Escape to exit.
    Game()
        : window_(sf::VideoMode::getDesktopMode(), "mini-maze with SFML",
                  sf::Style::None) {
        constexpr std::array<std::string_view, 4> files = {
            "assets/P.bmp", "assets/-.bmp", "assets/W.bmp", "assets/E.bmp"};
        for (std::size_t i = 0; i < files.size(); ++i)
            if (!textures_[i].loadFromFile(asset_path(files[i]).string()))
                throw std::runtime_error("texture not found: " +
                                         std::string(files[i]));
        const sf::Vector2u sz = window_.getSize();
        std::fprintf(stderr, "maze: window %ux%u\n", sz.x, sz.y);
        load_next_world();
    }

    void run() {
        while (window_.isOpen()) {
            handle_events();
            // Apply a held pointer every frame so dragging across adjacent tiles
            // keeps walking the player along, like holding the mouse button.
            if (pointer_down_)
                handle_click(pointer_pos_);
            render();
        }
    }

private:
    void load_next_world() {
        world_ = (world_ + 1) % worlds_.size();
        map_ = Map(asset_path(worlds_[world_]));
        player_ = map_.player();
        update_view();  // map dimensions changed; refit the view
    }

    // Fit the current map into the window preserving its aspect ratio. The
    // window is fullscreen on Android, so without this the maze is stretched to
    // the screen; a letterboxed viewport keeps tiles square and also makes
    // mapPixelToCoords translate touches/clicks back to map coordinates.
    void update_view() {
        const sf::Vector2u win = window_.getSize();
        if (win.x == 0 || win.y == 0)
            return;
        const float map_w = map_.width() * kTileSize;
        const float map_h = map_.height() * kTileSize;
        sf::View view(make_rect(0.f, 0.f, map_w, map_h));

        const float win_aspect = static_cast<float>(win.x) / win.y;
        const float map_aspect = map_w / map_h;
        float vw = 1.f, vh = 1.f, vx = 0.f, vy = 0.f;
        if (win_aspect > map_aspect) {  // window wider than map: bars left/right
            vw = map_aspect / win_aspect;
            vx = (1.f - vw) / 2.f;
        } else {                         // window taller than map: bars top/bottom
            vh = win_aspect / map_aspect;
            vy = (1.f - vh) / 2.f;
        }
        view.setViewport(make_rect(vx, vy, vw, vh));
        window_.setView(view);
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

    // Act on a key press. Key enumerators are spelled fully qualified
    // (sf::Keyboard::Key::X), which is valid for both SFML 2's unscoped enum
    // and SFML 3's scoped enum class.
    void handle_key(sf::Keyboard::Key code) {
        switch (code) {
            case sf::Keyboard::Key::Escape: window_.close(); break;
            case sf::Keyboard::Key::Up:     try_move(player_.x, player_.y - 1); break;
            case sf::Keyboard::Key::Down:   try_move(player_.x, player_.y + 1); break;
            case sf::Keyboard::Key::Left:   try_move(player_.x - 1, player_.y); break;
            case sf::Keyboard::Key::Right:  try_move(player_.x + 1, player_.y); break;
            default: break;
        }
    }

    void handle_events() {
#if SFML_VERSION_MAJOR >= 3
        // SFML 3: pollEvent() returns std::optional<sf::Event>; event kinds are
        // queried with is<>()/getIf<>(), and carry typed members (e.g. position).
        while (const std::optional event = window_.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window_.close();
            } else if (event->is<sf::Event::Resized>()) {
                update_view();
            } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                handle_key(key->code);
            } else if (const auto* mb =
                           event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mb->button == sf::Mouse::Button::Left)
                    press_pointer(mb->position);
            } else if (const auto* mb =
                           event->getIf<sf::Event::MouseButtonReleased>()) {
                if (mb->button == sf::Mouse::Button::Left)
                    pointer_down_ = false;
            } else if (const auto* mm =
                           event->getIf<sf::Event::MouseMoved>()) {
                pointer_pos_ = mm->position;
            } else if (const auto* t = event->getIf<sf::Event::TouchBegan>()) {
                if (t->finger == 0) press_pointer(t->position);
            } else if (const auto* t = event->getIf<sf::Event::TouchMoved>()) {
                if (t->finger == 0) pointer_pos_ = t->position;
            } else if (const auto* t = event->getIf<sf::Event::TouchEnded>()) {
                if (t->finger == 0) pointer_down_ = false;
            }
        }
#else
        // SFML 2: pollEvent(out) fills an sf::Event tagged by its .type field,
        // with a union of per-type members (key, mouseButton, touch).
        sf::Event event;
        while (window_.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window_.close();
            } else if (event.type == sf::Event::Resized) {
                update_view();
            } else if (event.type == sf::Event::KeyPressed) {
                handle_key(event.key.code);
            } else if (event.type == sf::Event::MouseButtonPressed) {
                if (event.mouseButton.button == sf::Mouse::Button::Left)
                    press_pointer({event.mouseButton.x, event.mouseButton.y});
            } else if (event.type == sf::Event::MouseButtonReleased) {
                if (event.mouseButton.button == sf::Mouse::Button::Left)
                    pointer_down_ = false;
            } else if (event.type == sf::Event::MouseMoved) {
                pointer_pos_ = {event.mouseMove.x, event.mouseMove.y};
            } else if (event.type == sf::Event::TouchBegan) {
                if (event.touch.finger == 0)
                    press_pointer({event.touch.x, event.touch.y});
            } else if (event.type == sf::Event::TouchMoved) {
                if (event.touch.finger == 0)
                    pointer_pos_ = {event.touch.x, event.touch.y};
            } else if (event.type == sf::Event::TouchEnded) {
                if (event.touch.finger == 0)
                    pointer_down_ = false;
            }
        }
#endif
    }

    // Begin a pointer hold (mouse-button-down or first finger touch). The held
    // pointer is applied each frame in run(), so a press also produces the first
    // step and dragging across adjacent tiles keeps the player walking.
    void press_pointer(sf::Vector2i pixel) {
        pointer_down_ = true;
        pointer_pos_ = pixel;
    }

    // Move the player onto the tile under the given window pixel if it is
    // orthogonally adjacent. mapPixelToCoords accounts for the letterboxed view.
    // After a step the pointer sits over the player's new tile (not adjacent),
    // so a stationary hold advances at most one tile until the pointer moves on.
    void handle_click(sf::Vector2i pixel) {
        const sf::Vector2f m = window_.mapPixelToCoords(pixel);
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
    bool pointer_down_ = false;        // left button / first finger held down
    sf::Vector2i pointer_pos_{0, 0};   // its latest window-pixel position
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
