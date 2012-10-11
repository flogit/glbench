//    This file is part of glBench.

//    glBench is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.

//    glBench is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with glBench.  If not, see <http://www.gnu.org/licenses/>.

#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <list>
#include <cmath>

#include <sys/time.h>
#include <string.h>

#include <SDL/SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>

#include "main.h"

#define BUFFER_OFFSET_CAST(i) reinterpret_cast<void*>(i)

////////////////////////////////////////////////////////////////////////
// GL extensions for VBO
////////////////////////////////////////////////////////////////////////
PFNGLGENBUFFERSPROC    glGenBuffers    = 0;
PFNGLBINDBUFFERPROC    glBindBuffer    = 0;
PFNGLBUFFERDATAPROC    glBufferData    = 0;
PFNGLDELETEBUFFERSPROC glDeleteBuffers = 0;

////////////////////////////////////////////////////////////////////////
int main(int, char**)
{
    std::cout << "X--------------------------------------------------X" << std::endl;
    std::cout << "|                   GlBench v1.0                   |" << std::endl;
    std::cout << "|                                                  |" << std::endl;
    std::cout << "|  - 'Space' to enable/disable rotation of model   |" << std::endl;
    std::cout << "|    but disable it when making accurate benchmark |" << std::endl;
    std::cout << "X--------------------------------------------------X" << std::endl;

    // Default display config
    struct DisplayConfig display_config;
    display_config.g_windows_width  = 600;
    display_config.g_windows_height = 600;
    display_config.g_rotation_angle_y = 0.0;
    display_config.g_rotation_angle_x = -20.0;
    display_config.g_move_forward = -1.5;
    display_config.g_rotation = true;

    // Default rendering config
    struct RenderingConfig rendering_config;
    rendering_config.rendering_method = CALL_LIST;
    rendering_config.nb_triangles = 320000;
    rendering_config.triangle_strip = true;
    rendering_config.color = true;
    rendering_config.texture = false;
    rendering_config.smooth_shading = true;
    rendering_config.back_face_painting = true;
    rendering_config.wireframe = false;

    // Default rendering data
    struct RenderingData rendering_data;
    rendering_data.texture_id = 0;
    rendering_data.call_list_id = 0;
    rendering_data.index_buffer_id  = 0;
    rendering_data.vertex_buffer_id = 0;

    // Initialization
    init_sdl(display_config);
    init_gl_extensions();
    generate_model(rendering_config, rendering_data);
    init_gl(rendering_data, display_config, rendering_config);
    print_config(rendering_config);

    bool quit_requested = false;
    std::deque<long> rendering_times;
    bool first_frame = true;

    // Main loop
    do
    {
        // Event handler function
        bool rendering_rendering_config_has_changed = event_sdl(display_config, rendering_config, quit_requested);

        if (rendering_rendering_config_has_changed)
        {
            first_frame = true;
            generate_model(rendering_config, rendering_data);
            init_gl(rendering_data, display_config, rendering_config);
            rendering_times.clear();
            std::cout << std::endl << "------------------------------------------------------" << std::endl;
            print_config(rendering_config);
        }

        struct timeval start;
        gettimeofday(&start, NULL);

        // Render function
        render(rendering_data, rendering_config, display_config);

        struct timeval end;
        gettimeofday(&end, NULL);

        // First frame is longer to process, skip it fir the time benchmarking
        if (first_frame)
        {
            rendering_times.clear();
            first_frame = false;
        }
        else
        {
            long current_rendering_time = elapsed_time(start, end);

            if (rendering_times.size() >= 30)
            {
                rendering_times.pop_back();
            }
            rendering_times.push_front(current_rendering_time);

            double mean_rendering_time = 0.0;
            for (std::deque<long>::const_iterator it = rendering_times.begin(); it != rendering_times.end(); ++it)
            {
                mean_rendering_time += static_cast<double>(*it);
            }
            mean_rendering_time = mean_rendering_time / static_cast<double>(rendering_times.size());

            std::cout << "\r" << rendering_config.nb_triangles << " triangles rendered in " << static_cast<unsigned int>(mean_rendering_time + 0.5) << " ms";
            if (rendering_times.size() < 30)
            {
                std::cout << "*";
            }
            std::cout << "      " << std::flush;

            if (display_config.g_rotation)
            {
                display_config.g_rotation_angle_y += 0.02 * current_rendering_time;
            }
        }

        GLenum error = glGetError();
        if (error != GL_NO_ERROR)
        {
            std::cout << "Warning : Opengl error (" << error << ")" << std::endl;
        }

    } while (!quit_requested);

    std::cout << std::endl;

    delete_call_list(rendering_data);
    delete_vbo(rendering_data);

    SDL_Quit();
}

////////////////////////////////////////////////////////////////////////
long elapsed_time(const struct timeval& in_start, const struct timeval& in_end)
{
    long seconds  = in_end.tv_sec  - in_start.tv_sec;
    long useconds = in_end.tv_usec - in_start.tv_usec;
    return seconds * 1000 + static_cast<long>(static_cast<double>(useconds) / 1000.0 + 0.5);
}

////////////////////////////////////////////////////////////////////////
void init_sdl(const DisplayConfig& in_display_config)
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_WM_SetCaption("GlBench", NULL);

    SDL_SetVideoMode(in_display_config.g_windows_width, in_display_config.g_windows_height, 32, SDL_OPENGL | SDL_GL_DOUBLEBUFFER);
}

////////////////////////////////////////////////////////////////////////
void init_gl_extensions()
{
    const unsigned char *exts = glGetString(GL_EXTENSIONS);
    if (strstr(reinterpret_cast<const char*>(exts), "GL_ARB_vertex_buffer_object") == NULL)
    {
        std::cout << "Warning : VBO extension is not supported by our graphic card" << std::endl;
    }
    else
    {
        glGenBuffers    = reinterpret_cast<PFNGLGENBUFFERSPROC>   (SDL_GL_GetProcAddress("glGenBuffers"));
        glBindBuffer    = reinterpret_cast<PFNGLBINDBUFFERPROC>   (SDL_GL_GetProcAddress("glBindBuffer"));
        glBufferData    = reinterpret_cast<PFNGLBUFFERDATAPROC>   (SDL_GL_GetProcAddress("glBufferData"));
        glDeleteBuffers = reinterpret_cast<PFNGLDELETEBUFFERSPROC>(SDL_GL_GetProcAddress("glDeleteBuffers"));
    }
}

////////////////////////////////////////////////////////////////////////
void init_gl(RenderingData& io_rendering_data, const DisplayConfig& in_display_config, const RenderingConfig& in_rendering_config)
{
    glViewport(0 , 0, static_cast<GLsizei>(in_display_config.g_windows_width), static_cast<GLsizei>(in_display_config.g_windows_height));

    if (in_rendering_config.wireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    else
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glEnable(GL_DEPTH_TEST);                // Enable Z-buffer for visibility
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);

    glEnable(GL_COLOR_MATERIAL);

    // Back face painting
    if (in_rendering_config.back_face_painting)
    {
        glDisable(GL_CULL_FACE);
        glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    }
    else
    {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
    }

    // Texturing
    if (in_rendering_config.texture)
    {
        glEnable(GL_TEXTURE_2D);
    }
    else
    {
        glDisable(GL_TEXTURE_2D);
    }

    // Smooth shading
    glShadeModel(in_rendering_config.smooth_shading ? GL_SMOOTH : GL_FLAT);

    process_texturing(io_rendering_data, in_rendering_config);
    process_call_list(io_rendering_data, in_rendering_config);
    process_vbo(io_rendering_data, in_rendering_config);
}

////////////////////////////////////////////////////////////////////////
bool event_sdl(DisplayConfig& io_display_config, RenderingConfig& io_rendering_config, bool& out_quit_requested)
{
    static int mouse_current_position_x = -1;
    static int mouse_current_position_y = -1;
    static bool mouse_left_button_down = false;

    bool rendering_config_has_changed = false;

    SDL_PumpEvents();                       // load all the events

    SDL_Event event;
    while (SDL_PollEvent(&event))           // poll event by event
    {
        switch (event.type)
        {
            ///////////////////////////////////
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                    case SDLK_q:
                    case SDLK_ESCAPE:
                        out_quit_requested = true;
                        break;

                    case SDLK_EQUALS:
                    case SDLK_PLUS:
                    case SDLK_KP_PLUS:
                        io_rendering_config.nb_triangles *= 2;
                        rendering_config_has_changed = true;
                        break;

                    case SDLK_MINUS:
                    case SDLK_KP_MINUS:
                        if (io_rendering_config.nb_triangles > 2500)
                        {
                            io_rendering_config.nb_triangles /= 2;
                        }
                        rendering_config_has_changed = true;
                        break;
                    case SDLK_w:
                        io_rendering_config.wireframe = !io_rendering_config.wireframe;
                        rendering_config_has_changed = true;
                        break;
                    case SDLK_m:
                        io_rendering_config.smooth_shading = !io_rendering_config.smooth_shading;
                        rendering_config_has_changed = true;
                        break;
                    case SDLK_c:
                        io_rendering_config.color = !io_rendering_config.color;
                        rendering_config_has_changed = true;
                        break;
                    case SDLK_t:
                        io_rendering_config.texture = !io_rendering_config.texture;
                        rendering_config_has_changed = true;
                        break;
                    case SDLK_b:
                        io_rendering_config.back_face_painting = !io_rendering_config.back_face_painting;
                        rendering_config_has_changed = true;
                        break;
                    case SDLK_s:
                        io_rendering_config.triangle_strip = !io_rendering_config.triangle_strip;
                        rendering_config_has_changed = true;
                        break;
                    case SDLK_F1:
                    case SDLK_F2:
                    case SDLK_F3:
                    case SDLK_F4:
                        io_rendering_config.rendering_method = static_cast<E_RenderingMethod>(event.key.keysym.sym - SDLK_F1 + 1);
                        rendering_config_has_changed = true;
                        break;
                    case SDLK_SPACE:
                        io_display_config.g_rotation = !io_display_config.g_rotation;
                        break;
                    default:
                        break;
                }
                break;

            ///////////////////////////////////
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    mouse_left_button_down = true;
                    mouse_current_position_x = -1;
                    mouse_current_position_y = -1;
                }
                else if (event.button.button == SDL_BUTTON_WHEELDOWN)
                {
                    io_display_config.g_move_forward -= 0.1;
                }
                else if (event.button.button == SDL_BUTTON_WHEELUP)
                {
                    io_display_config.g_move_forward += 0.1;
                }
                break;

            ///////////////////////////////////
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    mouse_left_button_down = false;
                }
                break;

            ///////////////////////////////////
            case SDL_MOUSEMOTION:
                if (mouse_left_button_down)
                {
                    if (mouse_current_position_x == -1 || mouse_current_position_y == -1)
                    {
                        mouse_current_position_x = event.button.x;
                        mouse_current_position_y = event.button.y;
                    }
                    else
                    {
                        io_display_config.g_rotation_angle_x += event.button.y - mouse_current_position_y;
                        io_display_config.g_rotation_angle_y += event.button.x - mouse_current_position_x;
                        mouse_current_position_x = event.button.x;
                        mouse_current_position_y = event.button.y;
                    }
                }
                break;

            ///////////////////////////////////
            case SDL_QUIT:                  // Quit resquested : "close button" of the window
                out_quit_requested = true;
                break;

            ///////////////////////////////////
            default:
                break;
        }
    }

    return rendering_config_has_changed;
}

////////////////////////////////////////////////////////////////////////
void print_config(const RenderingConfig& in_rendering_config)
{
    std::cout << " - ('F1/2/3/4') Rendering method .. ";
    if (in_rendering_config.rendering_method == IMMEDIATE)
        std::cout << "Immediate" << std::endl;
    else if (in_rendering_config.rendering_method == CALL_LIST)
        std::cout << "Call list" << std::endl;
    else if (in_rendering_config.rendering_method == STATIC_VBO)
        std::cout << "Static VBO" << std::endl;
    else if (in_rendering_config.rendering_method == DYNAMIC_VBO)
        std::cout << "Dynamic VBO" << std::endl;
    else
        std::cout << "Not yet implemented" << std::endl;
    std::cout << " - ('s') Triangles strip mode ..... " << in_rendering_config.triangle_strip << std::endl;
    std::cout << " - ('c') Colored model ............ " << in_rendering_config.color << std::endl;
    std::cout << " - ('t') Textured model ........... " << in_rendering_config.texture << std::endl;
    std::cout << " - ('m') Smooth shading ........... " << in_rendering_config.smooth_shading << std::endl;
    std::cout << " - ('b') Back face painting ....... " << in_rendering_config.back_face_painting << std::endl;
    std::cout << " - ('w') Wireframe model .......... " << in_rendering_config.wireframe << std::endl;
}

////////////////////////////////////////////////////////////////////////
Vector3d compute_normal(const Vector3d& in_v1, const Vector3d& in_v2, const Vector3d& in_v3)
{
    Vector3d vector1 = in_v2 - in_v1;
    Vector3d vector2 = in_v3 - in_v1;
    Vector3d normal(vector1.y * vector2.z - vector1.z * vector2.y,    // cross product
                    vector1.z * vector2.x - vector1.x * vector2.z,
                    vector1.x * vector2.y - vector1.y * vector2.x);
    return normal / sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
}

////////////////////////////////////////////////////////////////////////
void fill_normal(std::vector<Vertex>& out_vertices, unsigned int in_id1, unsigned int in_id2, unsigned int in_id3)
{
    Vector3d normal = compute_normal(out_vertices.at(in_id1).coord,
                                     out_vertices.at(in_id2).coord,
                                     out_vertices.at(in_id3).coord);
    out_vertices.at(in_id1).normal = out_vertices.at(in_id1).normal + normal;
    out_vertices.at(in_id2).normal = out_vertices.at(in_id2).normal + normal;
    out_vertices.at(in_id3).normal = out_vertices.at(in_id3).normal + normal;
}

////////////////////////////////////////////////////////////////////////
void generate_model(const RenderingConfig& in_rendering_config, RenderingData& out_rendering_data)
{
    out_rendering_data.geometry.vertices.clear();
    out_rendering_data.geometry.triangles_strip.clear();

    const unsigned int nb_subdivisions = static_cast<int>(sqrt(static_cast<double>(in_rendering_config.nb_triangles) / 2.0) + 0.5);

    const double texture_coef = 10.0;

    // Vertex generation
    for (unsigned int i = 0; i <= nb_subdivisions; ++i)
    {
        double ratio_i = static_cast<double>(i) / static_cast<double>(nb_subdivisions);

        const Vector3d color(1.0 - ratio_i, ratio_i, 1.0 - ratio_i);

        for (unsigned int j = 0; j <= nb_subdivisions; ++j)
        {
            double ratio_j = static_cast<double>(j) / static_cast<double>(nb_subdivisions);

            // Theta and phi
            const double theta = -M_PI / 2.0 + M_PI * ratio_i;
            const double phi = 2.0 * M_PI * ratio_j;

            // Construction of vertex
            Vertex v;

            // Polar equation of a pseudo-donuts
            v.coord  = Vector3d(cos(theta) * cos(phi), cos(theta) * sin(phi), sin(theta) * cos(theta));
            v.color  = color;
            v.normal = Vector3d(0.0, 0.0, 0.0);
            v.texture_coordinate = Vector3d(texture_coef * ratio_i, texture_coef * ratio_j, 0.0);

            out_rendering_data.geometry.vertices.push_back(v);
        }
    }

    // Triangle and Vertex's normal generation
    // Compute the normal direction of each vertex with the sum of neighbor triangle's normal
    for (unsigned int i = 0; i < nb_subdivisions; ++i)
    {
        TriangleStrip triangle_strip;
        for (unsigned int j = 0; j <= nb_subdivisions; ++j)
        {
            triangle_strip.vertex_ids.push_back(j + (i + 1) * (nb_subdivisions + 1));
            triangle_strip.vertex_ids.push_back(j +  i      * (nb_subdivisions + 1));

            if (j != nb_subdivisions)
            {
                fill_normal(out_rendering_data.geometry.vertices,
                            j + i * (nb_subdivisions + 1),
                            (j + 1) + i * (nb_subdivisions + 1),
                            j + (i + 1) * (nb_subdivisions + 1));
                fill_normal(out_rendering_data.geometry.vertices,
                            j + (i + 1) * (nb_subdivisions + 1),
                            (j + 1) + i * (nb_subdivisions + 1),
                            (j + 1) + (i + 1) * (nb_subdivisions + 1));
            }
        }

        Vector3d sum_normal_extremum = out_rendering_data.geometry.vertices.at(i * (nb_subdivisions + 1)).normal + out_rendering_data.geometry.vertices.at(nb_subdivisions + i * (nb_subdivisions + 1)).normal;
        out_rendering_data.geometry.vertices.at(i * (nb_subdivisions + 1)).normal = sum_normal_extremum;
        out_rendering_data.geometry.vertices.at(nb_subdivisions + i * (nb_subdivisions + 1)).normal = sum_normal_extremum;

        out_rendering_data.geometry.triangles_strip.push_back(triangle_strip);
    }

    // Normalize the direction computed before, with the number of neighbor triangle
    for (unsigned int i = 0; i <= nb_subdivisions; ++i)
    {
        for (unsigned int j = 0; j <= nb_subdivisions; ++j)
        {
            if (i == 0 || i == nb_subdivisions)
            {
                out_rendering_data.geometry.vertices.at(j + i * (nb_subdivisions + 1)).normal = out_rendering_data.geometry.vertices.at(j + i * (nb_subdivisions + 1)).normal / 3.0;
            }
            else
            {
                out_rendering_data.geometry.vertices.at(j + i * (nb_subdivisions + 1)).normal = out_rendering_data.geometry.vertices.at(j + i * (nb_subdivisions + 1)).normal / 6.0;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////
void process_texturing(RenderingData& io_rendering_data, const RenderingConfig& in_rendering_config)
{
    delete_texturing(io_rendering_data);

    if (in_rendering_config.texture)
    {
        glGenTextures(1, &io_rendering_data.texture_id);
        glBindTexture(GL_TEXTURE_2D, io_rendering_data.texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        GLubyte texture_data[16] = { 0xFF, 0xFF, 0xFF, 0, 0, 0, 0, 0,
                                     0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0 };

        gluBuild2DMipmaps(GL_TEXTURE_2D, 4, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, texture_data);
    }
}

////////////////////////////////////////////////////////////////////////
void delete_texturing(RenderingData& io_rendering_data)
{
    if (io_rendering_data.texture_id)
    {
        glDeleteTextures(1, &io_rendering_data.texture_id);
        glBindTexture(GL_TEXTURE_2D, 0);
        io_rendering_data.texture_id = 0;
    }
}

////////////////////////////////////////////////////////////////////////
void process_call_list(RenderingData& io_rendering_data, const RenderingConfig& in_rendering_config)
{
    delete_call_list(io_rendering_data);

    if (in_rendering_config.rendering_method == CALL_LIST)
    {
        io_rendering_data.call_list_id = glGenLists(1);
        glNewList(io_rendering_data.call_list_id, GL_COMPILE);
        paint_gl(io_rendering_data.geometry, in_rendering_config);
        glEndList();
    }
}

////////////////////////////////////////////////////////////////////////
void delete_call_list(RenderingData& io_rendering_data)
{
    if (io_rendering_data.call_list_id)
    {
        glDeleteLists(io_rendering_data.call_list_id, 1);
        io_rendering_data.call_list_id = 0;
    }
}

////////////////////////////////////////////////////////////////////////
void process_vbo(RenderingData& io_rendering_data, const RenderingConfig& in_rendering_config)
{
    delete_vbo(io_rendering_data);

    if (in_rendering_config.rendering_method == STATIC_VBO || in_rendering_config.rendering_method == DYNAMIC_VBO)
    {
        const int gl_draw_method = (in_rendering_config.rendering_method == DYNAMIC_VBO) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;

        unsigned int vertex_data_size = 6;
        if (in_rendering_config.color)
        {
            vertex_data_size += 3;
        }
        if (in_rendering_config.texture)
        {
            vertex_data_size += 3;
        }

        // VBO
        GLfloat *vertex_buffer = new GLfloat[io_rendering_data.geometry.vertices.size() * vertex_data_size];
        unsigned int offset = 0;
        for (unsigned int i = 0; i < io_rendering_data.geometry.vertices.size(); ++i)
        {
            vertex_buffer[offset++] = static_cast<GLfloat> (io_rendering_data.geometry.vertices.at(i).coord.x);
            vertex_buffer[offset++] = static_cast<GLfloat> (io_rendering_data.geometry.vertices.at(i).coord.y);
            vertex_buffer[offset++] = static_cast<GLfloat> (io_rendering_data.geometry.vertices.at(i).coord.z);
            vertex_buffer[offset++] = static_cast<GLfloat> (io_rendering_data.geometry.vertices.at(i).normal.x);
            vertex_buffer[offset++] = static_cast<GLfloat> (io_rendering_data.geometry.vertices.at(i).normal.y);
            vertex_buffer[offset++] = static_cast<GLfloat> (io_rendering_data.geometry.vertices.at(i).normal.z);
            if (in_rendering_config.color)
            {
                vertex_buffer[offset++] = static_cast<GLfloat> (io_rendering_data.geometry.vertices.at(i).color.x);
                vertex_buffer[offset++] = static_cast<GLfloat> (io_rendering_data.geometry.vertices.at(i).color.y);
                vertex_buffer[offset++] = static_cast<GLfloat> (io_rendering_data.geometry.vertices.at(i).color.z);
            }
            if (in_rendering_config.texture)
            {
                vertex_buffer[offset++] = static_cast<GLfloat> (io_rendering_data.geometry.vertices.at(i).texture_coordinate.x);
                vertex_buffer[offset++] = static_cast<GLfloat> (io_rendering_data.geometry.vertices.at(i).texture_coordinate.y);
                vertex_buffer[offset++] = static_cast<GLfloat> (io_rendering_data.geometry.vertices.at(i).texture_coordinate.z);
            }
        }

        glGenBuffers(1, &io_rendering_data.vertex_buffer_id);
        glBindBuffer(GL_ARRAY_BUFFER, io_rendering_data.vertex_buffer_id);
        glBufferData(GL_ARRAY_BUFFER, io_rendering_data.geometry.vertices.size() * vertex_data_size * sizeof(GLfloat), vertex_buffer, gl_draw_method);

        // IBO
        if (in_rendering_config.triangle_strip)
        {
            unsigned int nb_index = 0;
            for (std::list<TriangleStrip>::const_iterator it = io_rendering_data.geometry.triangles_strip.begin(); it != io_rendering_data.geometry.triangles_strip.end(); ++it)
            {
                nb_index += (*it).vertex_ids.size();
            }
            GLuint *p_index_buffer = new GLuint[nb_index];

            unsigned int offset = 0;
            for (std::list<TriangleStrip>::const_iterator it = io_rendering_data.geometry.triangles_strip.begin(); it != io_rendering_data.geometry.triangles_strip.end(); ++it)
            {
                for (unsigned int j = 0; j < (*it).vertex_ids.size(); ++j)
                {
                    p_index_buffer[offset] = static_cast<GLuint>((*it).vertex_ids.at(j));
                    ++offset;
                }
            }

            glGenBuffers(1, &io_rendering_data.index_buffer_id);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, io_rendering_data.index_buffer_id);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, nb_index * sizeof(GLuint), p_index_buffer, gl_draw_method);

            delete[] p_index_buffer;
        }
        else
        {
            unsigned int nb_index = 0;
            for (std::list<TriangleStrip>::const_iterator it = io_rendering_data.geometry.triangles_strip.begin(); it != io_rendering_data.geometry.triangles_strip.end(); ++it)
            {
                nb_index += ((*it).vertex_ids.size() - 2) * 3;
            }
            GLuint *p_index_buffer = new GLuint[nb_index];

            unsigned int offset = 0;
            for (std::list<TriangleStrip>::const_iterator it = io_rendering_data.geometry.triangles_strip.begin(); it != io_rendering_data.geometry.triangles_strip.end(); ++it)
            {
                for (unsigned int i = 0; i < (*it).vertex_ids.size() - 2; ++i)
                {
                    for (unsigned int j = 0; j < 3; ++j)
                    {
                        if (i % 2 == 0)
                        {
                            p_index_buffer[offset] = static_cast<GLuint>((*it).vertex_ids.at(i + j));
                            ++offset;
                        }
                        else
                        {
                            p_index_buffer[offset] = static_cast<GLuint>((*it).vertex_ids.at(i + 2 - j));
                            ++offset;
                        }
                    }
                }
            }

            glGenBuffers(1, &io_rendering_data.index_buffer_id);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, io_rendering_data.index_buffer_id);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, nb_index * sizeof(GLuint), p_index_buffer, gl_draw_method);

            delete[] p_index_buffer;
        }

        // Enable client state
        unsigned int buffer_offset = 0;
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, vertex_data_size * sizeof(GLfloat), BUFFER_OFFSET_CAST(buffer_offset));
        buffer_offset += 12;
        glEnableClientState(GL_NORMAL_ARRAY);
        glNormalPointer(GL_FLOAT, vertex_data_size * sizeof(GLfloat), BUFFER_OFFSET_CAST(buffer_offset));
        buffer_offset += 12;
        if (in_rendering_config.color)
        {
            glEnableClientState(GL_COLOR_ARRAY);
            glColorPointer(3, GL_FLOAT, vertex_data_size * sizeof(GLfloat), BUFFER_OFFSET_CAST(buffer_offset));
            buffer_offset += 12;
        }
        if (in_rendering_config.texture)
        {
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(3, GL_FLOAT, vertex_data_size * sizeof(GLfloat), BUFFER_OFFSET_CAST(buffer_offset));
            buffer_offset += 12;
        }
    }
}

////////////////////////////////////////////////////////////////////////
void delete_vbo(RenderingData& io_rendering_data)
{
    if (io_rendering_data.vertex_buffer_id || io_rendering_data.index_buffer_id)
    {
        if (io_rendering_data.vertex_buffer_id) // vbo
        {
            // Disable client state
            glDisableClientState(GL_VERTEX_ARRAY);
            glDisableClientState(GL_COLOR_ARRAY);
            glDisableClientState(GL_NORMAL_ARRAY);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            // Unbind and delete buffer
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glDeleteBuffers(1, &io_rendering_data.vertex_buffer_id);
            io_rendering_data.vertex_buffer_id = 0;
        }
        if (io_rendering_data.index_buffer_id)  // ibo
        {
            // Unbind and delete buffer
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glDeleteBuffers(1, &io_rendering_data.index_buffer_id);
            io_rendering_data.index_buffer_id = 0;
        }
    }
}

////////////////////////////////////////////////////////////////////////
void paint_gl(const Geometry& in_geometry, const RenderingConfig& in_rendering_config)
{
    if (!in_geometry.triangles_strip.empty())
    {
        if (!in_rendering_config.color)
        {
            glColor3d(1.0, 1.0, 1.0);
        }

        if (!in_rendering_config.triangle_strip)
        {
            glBegin(GL_TRIANGLES);
        }
        for (std::list<TriangleStrip>::const_iterator it = in_geometry.triangles_strip.begin();
                                                      it != in_geometry.triangles_strip.end();
                                                    ++it)
        {
            const TriangleStrip& triangle = *it;

            if (in_rendering_config.triangle_strip)
            {
                glBegin(GL_TRIANGLE_STRIP);

                for (unsigned int i = 0; i < triangle.vertex_ids.size(); ++i)    // for each vertices of the triangle
                {
                    paint_gl(in_rendering_config, in_geometry.vertices.at(triangle.vertex_ids[i]));
                }

                glEnd();
            }
            else
            {
                for (unsigned int i = 0; i < triangle.vertex_ids.size() - 2; ++i)    // for each vertices of the triangle
                {
                    for (unsigned int j = 0; j < 3; ++j)
                    {
                        if (i % 2 == 0)
                        {
                            paint_gl(in_rendering_config, in_geometry.vertices.at(triangle.vertex_ids[i + j]));
                        }
                        else
                        {
                            paint_gl(in_rendering_config, in_geometry.vertices.at(triangle.vertex_ids[i + 2 - j]));
                        }
                    }
                }
            }
        }
        if (!in_rendering_config.triangle_strip)
        {
            glEnd();
        }
    }
}

////////////////////////////////////////////////////////////////////////
void paint_gl(const RenderingConfig& in_rendering_config, const Vertex& in_vertex)
{
    if (in_rendering_config.rendering_method == IMMEDIATE || in_rendering_config.rendering_method == CALL_LIST)
    {
        if (in_rendering_config.color)
        {
            glColor3d(in_vertex.color.x, in_vertex.color.y, in_vertex.color.z);
        }
        if (in_rendering_config.texture)
        {
            glTexCoord2d(in_vertex.texture_coordinate.x, in_vertex.texture_coordinate.y);
        }
    }

    glNormal3d(in_vertex.normal.x,
               in_vertex.normal.y,
               in_vertex.normal.z);

    glVertex3d(in_vertex.coord.x,
               in_vertex.coord.y,
               in_vertex.coord.z);
}

////////////////////////////////////////////////////////////////////////
void render(const RenderingData& in_rendering_data, const RenderingConfig& in_rendering_config, const DisplayConfig& in_display_config)
{
    // Set Projection Matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-0.1, 0.1, -0.1, 0.1, 0.1, 40.0);

    // Set Modelview Matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Clear color buffer (to display background) and Z buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Modelview matrix translation
    glTranslated(0.0, 0.0, in_display_config.g_move_forward);

    // Ligthing
    GLfloat light_position[] = {0.0f, 2.0f, 1.0f, 0.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);

    // Modelview matrix rotation
    glRotated(in_display_config.g_rotation_angle_y, 0.0, 1.0, 0.0);
    glRotated(in_display_config.g_rotation_angle_x, 1.0, 0.0, 0.0);

    // Painting
    if (in_rendering_config.rendering_method == IMMEDIATE)
    {
        paint_gl(in_rendering_data.geometry, in_rendering_config);
    }
    else if (in_rendering_config.rendering_method == CALL_LIST)
    {
        glCallList(in_rendering_data.call_list_id);
    }
    else if (in_rendering_config.rendering_method == DYNAMIC_VBO || in_rendering_config.rendering_method == STATIC_VBO)
    {
        if (!in_rendering_config.color)
        {
            glColor3d(1.0, 1.0, 1.0);
        }

        size_t offset = 0;
        for (std::list<TriangleStrip>::const_iterator it = in_rendering_data.geometry.triangles_strip.begin(); it != in_rendering_data.geometry.triangles_strip.end(); ++it)
        {
            if (in_rendering_config.triangle_strip)
            {
                const unsigned int vertex_ids_size = (*it).vertex_ids.size();
                glDrawElements(GL_TRIANGLE_STRIP, vertex_ids_size, GL_UNSIGNED_INT, BUFFER_OFFSET_CAST(offset));
                offset += vertex_ids_size * sizeof(GLuint);
            }
            else
            {
                const unsigned int vertex_ids_size = ((*it).vertex_ids.size() - 2) * 3;
                glDrawElements(GL_TRIANGLES, vertex_ids_size, GL_UNSIGNED_INT, BUFFER_OFFSET_CAST(offset));
                offset += vertex_ids_size * sizeof(GLuint);
            }
        }
    }

    glFlush();
    SDL_GL_SwapBuffers();
}
