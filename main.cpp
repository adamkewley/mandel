#include <SDL.h>
#include <SDL_ttf.h>

#include <iostream>
#include <cassert>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <vector>

using Real = float;

template<typename T>
struct Point {
    Point() = default;

    T x{};
    T y{};
};

template<typename T>
struct Dimensions {
    Dimensions() = default;

    T w{};
    T h{};
};

template<typename T>
struct Rect {
    Rect() = default;

    Point<T> pos{};
    Dimensions<T> dimensions{};
};

template<typename T>
struct PointInRect {
    PointInRect() = default;

    Point<T> point{};
    Rect<T> rect{};
};

template<typename T>
Point<Real> rel_pos(PointInRect<T> p) {
    Real x = static_cast<Real>(p.point.x - p.rect.pos.x)/p.rect.dimensions.w;
    Real y = static_cast<Real>(p.point.y - p.rect.pos.y)/p.rect.dimensions.h;
    return {x, y};
}

template<typename TIn, typename TOut>
PointInRect<TOut> map(PointInRect<TIn> pir, Rect<TOut> rect) {
    Point<Real> rel = rel_pos(pir);
    TOut x = rect.pos.x + (rel.x * rect.dimensions.w);
    TOut y = rect.pos.y + (rel.y * rect.dimensions.h);
    return {{x, y}, rect};
}

template<typename T>
PointInRect<T> zoom(PointInRect<T> pos, Real amount) {
    assert(amount > 0.0f);

    Point<Real> rel = rel_pos(pos);
    Real w_new = amount * pos.rect.dimensions.w;
    Real h_new = amount * pos.rect.dimensions.h;
    Rect<T> rect = {
            .pos = {
                    .x = pos.point.x - (rel.x * w_new),
                    .y = pos.point.y - (rel.y * h_new),
            },
            .dimensions = {
                    .w = w_new,
                    .h = h_new,
            },
    };

    return {pos.point, rect};
}

template<typename T>
Point<T>& operator+=(Point<T>& lhs, const Point<T>& rhs) {
    lhs.x += rhs.x;
    lhs.y += rhs.y;
    return lhs;
}

template<typename T>
Point<T> operator-(Point<T> lhs, const Point<T>& rhs) {
    lhs.x -= rhs.x;
    lhs.y -= rhs.y;
    return lhs;
}

template<typename T>
std::ostream& operator<<(std::ostream& o, const Point<T>& rhs) {
    o << rhs.x << ", " << rhs.y;
    return o;
}

unsigned msb(unsigned i) {
    unsigned ret = 0;
    while (i > 0) {
        i >>= 1U;
        ++ret;
    }
    return ret;
}

std::vector<Uint32> make_color_lut(const SDL_PixelFormat* fmt, unsigned iters, unsigned shift) {
    std::vector<Uint32> ret;
    for (auto i = 0U; i < iters; ++i) {
        Uint8 brightness = i << shift;
        ret.push_back(SDL_MapRGB(fmt, brightness, brightness, brightness));
    }
    ret.push_back(SDL_MapRGB(fmt, 0x00, 0x00, 0x00));
    return ret;
}

struct State {
    explicit State(SDL_Surface* _surf): surf{_surf} {
        assert(surf != nullptr);
        this->color_lut = make_color_lut(surf->format, num_iterations, 4U);
    }

    Dimensions<int> display_dimensions() {
        return {surf->w, surf->h};
    }

    Uint32 color(unsigned iters) {
        return color_lut.at(iters);
    }

    unsigned iterations() const {
        return num_iterations;
    }

    unsigned iterations(unsigned _num_iterations) {
        num_iterations = std::clamp(_num_iterations, 1U, 1024U);
        unsigned shift = num_iterations <= 256 ?
                std::min(7U, 9-msb(num_iterations)) :
                0U;
        this->color_lut = make_color_lut(surf->format, num_iterations, shift);
        return num_iterations;
    }

    static constexpr Rect<Real> initial_rect = {
        .pos = {.x = -2.5f, .y = -1.0f},
        .dimensions = { .w = 3.5f, .h = 2.0f },
    };

    Rect<Real> rect = initial_rect;
    SDL_Surface* surf;

private:
    unsigned num_iterations = 16;
    std::vector<Uint32> color_lut;
};

unsigned mandel_iterations(Real x0, Real y0, unsigned max_iterations) {
    Real x = 0;
    Real y = 0;
    Real x2 = 0;
    Real y2 = 0;
    size_t iter = 0;

    while (iter < max_iterations and x2+y2 <= 4) {
        y = 2*x*y + y0;
        x = x2-y2 + x0;
        x2 = x*x;
        y2 = y*y;
        ++iter;
    }

    return iter;
}

Uint32 mandel_color(State& s, Real x0, Real y0, unsigned num_iterations) {
    return s.color(mandel_iterations(x0, y0, num_iterations));
}

void draw_mandelbrot(State& s) {
    if (SDL_LockSurface(s.surf) != 0) {
        throw std::runtime_error{"draw_mandelbrot: unable to lock SDL_Surface for software rendering"};
    }

    const Point<Real> origin = s.rect.pos;
    const Point<Real> one_unit_abspos =
            map(PointInRect<int>{.point = {1, 1}, .rect = {.pos = {0,0}, .dimensions = s.display_dimensions()}}, s.rect).point;
    const Point<Real> one_unit_relpos = one_unit_abspos - origin;
    const Dimensions<int> d = s.display_dimensions();

    auto pixels = static_cast<Uint32*>(s.surf->pixels);
    unsigned rowstart = 0;
    Point<Real> realpos = origin;

    for (Point<int> p{}; p.y < d.h; ++p.y, rowstart+=d.w) {
        realpos.x = origin.x;
        for (p.x = 0; p.x < d.w; ++p.x) {
            pixels[rowstart + p.x] = mandel_color(s, realpos.x, realpos.y, s.iterations());
            realpos.x += one_unit_relpos.x;
        }
        realpos.y += one_unit_relpos.y;
    }

    SDL_UnlockSurface(s.surf);
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "sdl2: SDL_Init failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    if (TTF_Init() == -1) {
        std::cerr << "sdl2: TTF_Init failed: " << TTF_GetError() << std::endl;
        return -1;
    }

    SDL_Window* w = SDL_CreateWindow(
            "Some window",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            1024,
            768,
            SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);

    if (w == nullptr) {
        std::cerr << "sdl2: SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_Surface* s = SDL_GetWindowSurface(w);

    if (s == nullptr) {
        std::cerr << "sdl2: SDL_GetWindowSurface failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    State st{s};

    TTF_Font* font = TTF_OpenFont("../FantasqueSansMono-Regular.ttf", 16);
    if (font == nullptr) {
        std::cerr << "sdl2: TTF_OpenFont failed: " << TTF_GetError() << std::endl;
        return -1;
    }

    SDL_Event e;
    bool first_motion = true;
    size_t frame = 0;
    const auto started = std::chrono::steady_clock::now();
    auto last_frame_time = std::chrono::steady_clock::now();
    for (;;) {
        ++frame;
        draw_mandelbrot(st);
        auto cur_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> run_secs = cur_time - started;
        std::chrono::duration<double> frame_time = cur_time - last_frame_time;
        last_frame_time = cur_time;

        {
            std::stringstream ss;
            ss << "screen res = " << st.display_dimensions().w << "x" << st.display_dimensions().h << std::endl;
            ss << "view space:" << std::endl;
            ss << "    x = [" << st.rect.pos.x << ", " << st.rect.pos.x + st.rect.dimensions.w << "]" << std::endl;
            ss << "    y = [" << st.rect.pos.y << ", " << st.rect.pos.y + st.rect.dimensions.h << "]" << std::endl;
            ss << "iterations = " << st.iterations() << std::endl;
            ss << "frame = " << frame << std::endl;
            ss << "elapsed = " << run_secs.count() << " s" << std::endl;
            ss << "cur. FPS = " << 1/frame_time.count() << " frames/sec" << std::endl;
            SDL_Surface* font_surf = TTF_RenderText_Blended_Wrapped(font, ss.str().c_str(), SDL_Color { .r = 0xff, .g = 0x00, .b = 0x00, .a = 0xff }, 1000);
            if (font_surf == nullptr) {
                std::cerr << "sdl2: TTF_RenderText_Solid failed: " << TTF_GetError() << std::endl;
                return -1;
            }
            SDL_Rect txtpos{.x = 16, .y = 16, st.display_dimensions().w, st.display_dimensions().h};
            SDL_BlitSurface(font_surf, NULL, st.surf, &txtpos);
            SDL_FreeSurface(font_surf);
        }

        if (SDL_UpdateWindowSurface(w) != 0) {
            std::cerr << "sdl2: SDL_UpdateWindowSurface failed: " << SDL_GetError() << std::endl;
            return -1;
        }

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        return 0;
                    case SDLK_UP:
                        st.iterations(2*st.iterations());
                        break;
                    case SDLK_DOWN:
                        st.iterations(st.iterations()/2);
                        break;
                    case SDLK_r:
                        st.rect = State::initial_rect;
                        break;
                }
            } else if (e.type == SDL_QUIT) {
                return 0;
            } else if (e.type == SDL_MOUSEWHEEL) {
                PointInRect<Real> mouse_pos_in_mandel{};
                {
                    PointInRect<int> mouse_pos;
                    SDL_GetMouseState(&mouse_pos.point.x, &mouse_pos.point.y);
                    SDL_GetWindowSize(w, &mouse_pos.rect.dimensions.w, &mouse_pos.rect.dimensions.h);
                    mouse_pos_in_mandel = map(mouse_pos, st.rect);
                }

                st.rect = zoom(mouse_pos_in_mandel, e.wheel.y > 0 ? 0.9 : 1.1111111).rect;
            } else if (e.type == SDL_MOUSEMOTION) {
                if ((e.motion.state & SDL_BUTTON_LMASK) == 0) {
                    continue;
                }

                if (first_motion) {
                    first_motion = false;
                    continue;
                }

                PointInRect<int> mouse_pos = {
                        .point = {-e.motion.xrel, -e.motion.yrel},
                        .rect = { .pos = {0, 0}, .dimensions = st.display_dimensions() },
                };

                st.rect.pos = map(mouse_pos, st.rect).point;
            }
        }
    }

    TTF_CloseFont(font);
    SDL_DestroyWindow(w);
    TTF_Quit();
    SDL_Quit();
}
