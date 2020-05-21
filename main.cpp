#include <SDL.h>
#include <SDL_ttf.h>
#include <GL/glew.h>
#include <GL/glut.h>

#include <iostream>
#include <fstream>
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

struct State {
    explicit State(int _width, int _height) : width{_width}, height{_height}  {
    }

    Dimensions<int> display_dimensions() {
        return {width, height};
    }

    unsigned iterations() const {
        return num_iterations;
    }

    unsigned iterations(unsigned _num_iterations) {
        num_iterations = std::clamp(_num_iterations, 1U, 1024U);
        return num_iterations;
    }

    static constexpr Rect<Real> initial_rect = {
        .pos = {.x = -2.5f, .y = -1.0f},
        .dimensions = { .w = 3.5f, .h = 2.0f },
    };

    Rect<Real> rect = initial_rect;

private:
    unsigned num_iterations = 16;
    int width = 0;
    int height = 0;
};

struct ExampleGLProg {
  GLuint program;
  GLuint vertex_shader;
  GLuint frag_shader;
  GLint attrib_LVertexPos2D;
  GLuint vao;
  GLuint vbo;
  GLuint ibo;
  GLint uniform_x_rescale;
  GLint uniform_x_offset;
  GLint uniform_y_rescale;
  GLint uniform_y_offset;
  GLint uniform_num_iterations;
};

void draw_mandelbrot(State& s, ExampleGLProg& p, SDL_Window* w) {
  glClear(GL_COLOR_BUFFER_BIT);
  glEnableClientState(GL_VERTEX_ARRAY);
  glUseProgram(p.program);



  glUniform1f(p.uniform_x_rescale, s.rect.dimensions.w/2.0f);
  glUniform1f(p.uniform_x_offset, s.rect.dimensions.w/2.0f + s.rect.pos.x);
  glUniform1f(p.uniform_y_rescale, s.rect.dimensions.h/2.0f);
  glUniform1f(p.uniform_y_offset, -s.rect.dimensions.h/2.0f - s.rect.pos.y);
  glUniform1i(p.uniform_num_iterations, s.iterations());
  glBindVertexArray(p.vao);
  glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_INT, nullptr);
  glBindVertexArray(static_cast<GLuint>(0));
  glUseProgram(static_cast<GLuint>(0));
  glDisableClientState(GL_VERTEX_ARRAY);
  //  draw_mandelbrot_software(s);
}

std::string load_into_string(const char* path) {
  std::ifstream f{path};

  if (not f) {
    std::stringstream ss;
    ss << path << ": unable to load file";
    throw std::runtime_error{ss.str()};
  }

  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

ExampleGLProg create_prog() {
    GLuint prog = glCreateProgram();
    // dtor: glDeleteProgram
    if (prog == 0) {
        throw std::runtime_error{"OpenGL: glCreateProgram() failed"};
    }

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    // dtor: glDeleteShader
    if (vertex_shader == 0) {
      // LEAKS: prog
      throw std::runtime_error{"OpenGL: glCreateShader() failed"};
    }

    {
      std::string src = load_into_string("../shader.vert");
      const char* s = src.c_str();
      glShaderSource(vertex_shader, 1, &s, nullptr);
      glCompileShader(vertex_shader);
    }

    // error check: vertex shader compilation
    {
        GLint params;
        glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &params);
        if (params == GL_FALSE) {
            GLint log_len = 0;
            glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &log_len);

            std::vector<GLchar> errmsg(log_len);
            glGetShaderInfoLog(vertex_shader, log_len, &log_len, errmsg.data());

            std::stringstream ss;
            ss << "OpenGL: glCompileShader() failed";
            ss << errmsg.data();
            // LEAKS: prog + vertex_shader
            throw std::runtime_error{ss.str()};
        }
    }

    glAttachShader(prog, vertex_shader);
    // glDetachShader

    GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
    // glDeleteShader
    if (frag_shader == 0) {
      // LEAKS: prog
      // LEAKS: shader
      throw std::runtime_error{"OpenGL: glCreateShader() failed"};
    }

    {
      std::string src = load_into_string("../shader.frag");
      const char* s = src.c_str();
      glShaderSource(frag_shader, 1, &s, nullptr);
      glCompileShader(frag_shader);
    }

    // error check: fragment shader compilation
    {
      GLint params = GL_FALSE;
      glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &params);
      if (params == GL_FALSE) {
        GLint log_len = 0;
        glGetShaderiv(frag_shader, GL_INFO_LOG_LENGTH, &log_len);

        std::vector<GLchar> errmsg(log_len);
        glGetShaderInfoLog(frag_shader, log_len, &log_len, errmsg.data());

        std::stringstream ss;
        ss << "OpenGL: glCompileShader() failed: ";
        ss << errmsg.data();
        // LEAKS: prog + vertex_shader + frag_shader
        throw std::runtime_error{ss.str()};
      }
    }

    glAttachShader(prog, frag_shader);
    // dtor: glDetachShader

    glLinkProgram(prog);

    // error check: linking
    {
      GLint link_status = GL_FALSE;
      glGetProgramiv(prog, GL_LINK_STATUS, &link_status);
      if (link_status == GL_FALSE) {
        GLint log_len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &log_len);

        std::vector<GLchar> errmsg(log_len);
        glGetProgramInfoLog(prog, errmsg.size(), nullptr, errmsg.data());

        std::stringstream ss;
        ss << "OpenGL: glLinkProgram() failed: ";
        ss << errmsg.data();
        // LEAKS: prog + vertex_shader + frag_shader
        throw std::runtime_error{ss.str()};
      }
    }

    // get attribs
    GLint attrib_LVertexPos2D = glGetAttribLocation(prog, "LVertexPos2D");
    if (attrib_LVertexPos2D == -1) {
      // LEAKS: prog + vertex_shader + frag_shader
      throw std::runtime_error{"OpenGL: glGetAttribLocation() returned -1: LVertexPos2D is not a valid glsl program attribute"};
    }

    glClearColor(0.5f, 0.0f, 0.0f, 1.0f);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    GLfloat vbo_data[] = {
                          -1.0, -1.0,  // bottom-left
                          +1.0, -1.0,  // bottom-right
                          +1.0, +1.0,  // top-right
                          -1.0, +1.0   // top-left
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vbo_data), vbo_data, GL_STATIC_DRAW);

    GLuint ibo;
    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    GLuint ibo_data[] = { 0, 1, 2, 3 };
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(ibo_data), ibo_data, GL_STATIC_DRAW);

    GLint uniform_x_rescale = glGetUniformLocation(prog, "x_rescale");
    if (uniform_x_rescale == -1) {
      throw std::runtime_error{"cannot find x_rescale"};
    }

    GLint uniform_x_offset = glGetUniformLocation(prog, "x_offset");
    if (uniform_x_offset == -1) {
      throw std::runtime_error{"cannot find y_rescale"};
    }

    GLint uniform_y_rescale = glGetUniformLocation(prog, "y_rescale");
    if (uniform_y_rescale == -1) {
      throw std::runtime_error{"cannot find y_rescale"};
    }

    GLint uniform_y_offset = glGetUniformLocation(prog, "y_offset");
    if (uniform_y_offset == -1) {
      throw std::runtime_error{"cannot find y_offset"};
    }

    GLint uniform_num_iterations = glGetUniformLocation(prog, "num_iterations");
    if (uniform_num_iterations == -1) {
      throw std::runtime_error{"cannot find num_iterations"};
    }

    // Set up vao
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // vao: vertex positions
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(vbo);
    glVertexAttribPointer(vbo, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // vao: LVertexPos2D
    glEnableVertexAttribArray(attrib_LVertexPos2D);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(attrib_LVertexPos2D, 2, GL_FLOAT, GL_FALSE, 2*sizeof(GLfloat), nullptr);

    // vao: index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

    // vao: unbind
    glBindVertexArray(static_cast<GLuint>(0));

    return {
            .program = prog,
            .vertex_shader = vertex_shader,
            .frag_shader = frag_shader,
            .attrib_LVertexPos2D = attrib_LVertexPos2D,
            .vao = vao,
            .vbo = vbo,
            .ibo = ibo,
            .uniform_x_rescale = uniform_x_rescale,
            .uniform_x_offset = uniform_x_offset,
            .uniform_y_rescale = uniform_y_rescale,
            .uniform_y_offset = uniform_y_offset,
            .uniform_num_iterations = uniform_num_iterations
    };
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

#if __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetSwapInterval(0);

    SDL_Window* w = SDL_CreateWindow(
            "Some window",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            1024,
            768,
            SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN);
    if (w == nullptr) {
        std::cerr << "sdl2: SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_GLContext gl_ctx = SDL_GL_CreateContext(w);

    if (gl_ctx == nullptr) {
        std::cerr << "sdl2: SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    if (SDL_GL_MakeCurrent(w, gl_ctx) != 0) {
        std::cerr << "sdl2: SDL_GL_MakeCurrent failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    if (auto err = glewInit(); err != GLEW_OK) {
        std::cerr << "glewInit() failed: " << glewGetErrorString(err) << std::endl;
        return -1;
    }

    SDL_Renderer* r = SDL_CreateRenderer(
            w,
            -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (r == nullptr) {
        std::cerr << "SDL2: SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    ExampleGLProg p = create_prog();

    int win_width;
    int win_height;
    SDL_GetWindowSize(w, &win_width, &win_height);
    State st{win_width, win_height};

    TTF_Font* font = TTF_OpenFont("../FantasqueSansMono-Regular.ttf", 16);
    if (font == nullptr) {
        std::cerr << "sdl2: TTF_OpenFont failed: " << TTF_GetError() << std::endl;
        return -1;
    }

    // vsync: SDL_GL_SetSwapInterval(0);

    SDL_Event e;
    bool first_motion = true;
    size_t frame = 0;
    const auto started = std::chrono::steady_clock::now();
    auto last_frame_time = std::chrono::steady_clock::now();
    for (;;) {
        ++frame;
        draw_mandelbrot(st, p, w);
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
            SDL_Texture* t = SDL_CreateTextureFromSurface(r, font_surf);
            if (t == NULL) {
              throw std::runtime_error{"null texture?"};
            }
            SDL_Rect txtpos{.x = 16, .y = 16, font_surf->w, font_surf->h};
            SDL_RenderCopy(r, t, NULL, &txtpos);
            SDL_DestroyTexture(t);
            SDL_FreeSurface(font_surf);
        }

        SDL_GL_SwapWindow(w);

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
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(w);
    TTF_Quit();
    SDL_Quit();
}
