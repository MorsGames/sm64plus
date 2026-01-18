#include <stdint.h>
#include <stdbool.h>

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif
#include <PR/gbi.h>

#ifdef __MINGW32__
#define FOR_WINDOWS 1
#else
#define FOR_WINDOWS 0
#endif

#if FOR_WINDOWS
#include <GL/glew.h>
#include "SDL.h"
#define GL_GLEXT_PROTOTYPES 1
#include "SDL_opengl.h"
#else
#ifndef TARGET_MACOS
#include <SDL2/SDL.h>
#else
#include <SDL_opengl.h>
#include <stdio.h>
#endif
#define GL_GLEXT_PROTOTYPES 1
#ifndef TARGET_MACOS
#include <SDL2/SDL_opengles2.h>
#endif
#endif

#include "gfx_cc.h"
#include "gfx_pc.h"
#include "gfx_rendering_api.h"

#include "./game/settings.h"

struct ShaderProgram {
    uint32_t shader_id;
    GLuint opengl_program_id;
    uint8_t num_inputs;
    bool used_textures[2];
    uint8_t num_floats;
    GLint attrib_locations[7];
    uint8_t attrib_sizes[7];
    uint8_t num_attribs;
    bool used_noise;
    GLint frame_count_location;
    GLint window_height_location;
    GLint uTex0Size_location;
    GLint uTex1Size_location;
    GLint uTex0Filter_location;
    GLint uTex1Filter_location;
};

struct GLTexture {
    GLuint gltex;
    float size[2];
    bool filter;
};

#define TEX_CACHE_STEP 512

static int tex_cache_size = 0;
static int num_textures = 0;
static struct GLTexture *tex_cache = NULL;

static struct ShaderProgram *current_program = NULL;
static struct GLTexture *opengl_tex[2] = {NULL, NULL};
static int opengl_curtex = 0;

static struct ShaderProgram shader_program_pool[64];
static uint8_t shader_program_pool_size;
static GLuint opengl_vbo;

static uint32_t frame_count;
static uint32_t current_height;

// Internal resolution framebuffer objects
static GLuint internal_framebuffer = 0;
static GLuint internal_color_texture = 0;
static GLuint internal_depth_renderbuffer = 0;
static uint32_t internal_width = 0;
static uint32_t internal_height = 0;

// Resolved framebuffer for multisampling
static GLuint resolved_framebuffer = 0;
static GLuint resolved_color_texture = 0;

// Blit shader for upscaling
static GLuint blit_program = 0;
static GLuint blit_vbo = 0;
static GLint blit_texture_location = -1;
static GLint blit_position_location = -1;
static GLint blit_texcoord_location = -1;

// N64-style horizontal blur
static GLuint blur_framebuffer = 0;
static GLuint blur_texture = 0;
static GLuint blur_program = 0;
static GLint blur_texture_location = -1;
static GLint blur_position_location = -1;
static GLint blur_texcoord_location = -1;
static GLint blur_screen_width_location = -1;

// Get display dimensions based on fullscreen/windowed mode
static void get_display_dimensions(uint32_t *width, uint32_t *height) {
    if (configFullscreen) {
        if (configCustomFullscreenResolution) {
            *width = configFullscreenWidth;
            *height = configFullscreenHeight;
            return;
        }
        
        SDL_DisplayMode mode;
        if (SDL_GetCurrentDisplayMode(0, &mode) == 0) {
            *width = mode.w;
            *height = mode.h;
            return;
        }
    }

    *width = configWindowWidth;
    *height = configWindowHeight;
}

// Create a framebuffer object
static void create_framebuffer(GLuint* framebuffer, GLuint* texture, GLuint* renderbuffer, uint32_t width, uint32_t height, bool multisample, bool depth) {
    glGenFramebuffers(1, framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, *framebuffer);

    glGenTextures(1, texture);
    if (multisample) {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, *texture);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, configAntiAliasing, GL_RGBA8, width, height, GL_TRUE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, *texture, 0);
    }
    else {
        glBindTexture(GL_TEXTURE_2D, *texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *texture, 0);
    }

    if (depth) {
        glGenRenderbuffers(1, renderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, *renderbuffer);
        if (multisample) {
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, configAntiAliasing, GL_DEPTH_COMPONENT24, width, height);
        }
        else {
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        }
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, *renderbuffer);
    }

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("Error: Framebuffer is not complete!\n");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Create internal framebuffer for render-to-texture
static void create_internal_framebuffer(uint32_t width, uint32_t height) {
    if (internal_framebuffer != 0) {
        glDeleteFramebuffers(1, &internal_framebuffer);
        glDeleteTextures(1, &internal_color_texture);
        glDeleteRenderbuffers(1, &internal_depth_renderbuffer);
    }

    if (resolved_framebuffer != 0) {
        glDeleteFramebuffers(1, &resolved_framebuffer);
        glDeleteTextures(1, &resolved_color_texture);
    }

    internal_width = width;
    internal_height = height;

    bool multisample = configAntiAliasing > 0;
    create_framebuffer(&internal_framebuffer, &internal_color_texture, &internal_depth_renderbuffer, width, height, multisample, true);

    if (multisample) {
        create_framebuffer(&resolved_framebuffer, &resolved_color_texture, NULL, width, height, false, false);
    }
}

// Destroy internal framebuffer
static void destroy_internal_framebuffer(void) {
    if (internal_framebuffer != 0) {
        glDeleteFramebuffers(1, &internal_framebuffer);
        glDeleteTextures(1, &internal_color_texture);
        glDeleteRenderbuffers(1, &internal_depth_renderbuffer);
        internal_framebuffer = 0;
    }
    if (resolved_framebuffer != 0) {
        glDeleteFramebuffers(1, &resolved_framebuffer);
        glDeleteTextures(1, &resolved_color_texture);
        resolved_framebuffer = 0;
    }
}

// Create blur framebuffer
static void create_blur_framebuffer(uint32_t width, uint32_t height) {
    if (blur_framebuffer != 0) {
        glDeleteFramebuffers(1, &blur_framebuffer);
        glDeleteTextures(1, &blur_texture);
    }

    create_framebuffer(&blur_framebuffer, &blur_texture, NULL, width, height, false, false);
}

// Destroy blur framebuffer
static void destroy_blur_framebuffer(void) {
    if (blur_framebuffer != 0) {
        glDeleteFramebuffers(1, &blur_framebuffer);
        glDeleteTextures(1, &blur_texture);
        blur_framebuffer = 0;
    }
}

static GLuint create_shader_program(const char* vertex_shader_source, const char* fragment_shader_source) {
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return program;
}

// Blit shader source code
static const char* blit_vertex_shader_source =
"#version 120\n"
"attribute vec2 position;\n"
"attribute vec2 texCoord;\n"
"varying vec2 v_texCoord;\n"
"void main() {\n"
"    gl_Position = vec4(position, 0.0, 1.0);\n"
"    v_texCoord = texCoord;\n"
"}\n";

static const char* blit_fragment_shader_source =
"#version 120\n"
"uniform sampler2D u_texture;\n"
"varying vec2 v_texCoord;\n"
"void main() {\n"
"    gl_FragColor = texture2D(u_texture, v_texCoord);\n"
"}\n";

// Create and compile blit shaders
static void create_blit_shader(void) {
    if (blit_program != 0) return;

    blit_program = create_shader_program(blit_vertex_shader_source, blit_fragment_shader_source);

    blit_texture_location = glGetUniformLocation(blit_program, "u_texture");
    blit_position_location = glGetAttribLocation(blit_program, "position");
    blit_texcoord_location = glGetAttribLocation(blit_program, "texCoord");

    glGenBuffers(1, &blit_vbo);
    float quad_vertices[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    glBindBuffer(GL_ARRAY_BUFFER, blit_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, opengl_vbo);
}

// Destroy blit shader
static void destroy_blit_shader(void) {
    if (blit_program != 0) {
        glDeleteProgram(blit_program);
        blit_program = 0;
    }
    if (blit_vbo != 0) {
        glDeleteBuffers(1, &blit_vbo);
        blit_vbo = 0;
    }
}

// Blur shader source code
static const char* blur_fragment_shader_source =
"#version 120\n"
"uniform sampler2D u_texture;\n"
"uniform float u_screen_width;\n"
"varying vec2 v_texCoord;\n"
"void main() {\n"
"    float pixel_width = 1.0 / u_screen_width;\n"
"    vec4 color = vec4(0.0);\n"
"    float strength = 0.5;\n"
"    color += texture2D(u_texture, v_texCoord + vec2(-1.0 * pixel_width, 0.0)) * 0.25 * strength;\n"
"    color += texture2D(u_texture, v_texCoord) * (1.0 - 0.5 * strength);\n"
"    color += texture2D(u_texture, v_texCoord + vec2(1.0 * pixel_width, 0.0)) * 0.25 * strength;\n"
"    gl_FragColor = color;\n"
"}\n";

// Create and compile blur shaders
static void create_blur_shader(void) {
    if (blur_program != 0) return;

    blur_program = create_shader_program(blit_vertex_shader_source, blur_fragment_shader_source); // Reusing the blit vertex shader

    blur_texture_location = glGetUniformLocation(blur_program, "u_texture");
    blur_position_location = glGetAttribLocation(blur_program, "position");
    blur_texcoord_location = glGetAttribLocation(blur_program, "texCoord");
    blur_screen_width_location = glGetUniformLocation(blur_program, "u_screen_width");
}

// Destroy blur shader
static void destroy_blur_shader(void) {
    if (blur_program != 0) {
        glDeleteProgram(blur_program);
        blur_program = 0;
    }
}

// Blit internal framebuffer to main framebuffer with nearest neighbor filtering
static void blit_internal_framebuffer(GLuint texture) {
    if (configAntiAliasing > 0 && !configN64Blur) {
        // Resolve multisampled framebuffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, internal_framebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolved_framebuffer);
        glBlitFramebuffer(0, 0, internal_width, internal_height, 0, 0, internal_width, internal_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    // Save the current state
    GLboolean depth_test_enabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blend_enabled = glIsEnabled(GL_BLEND);
    GLboolean scissor_test_enabled = glIsEnabled(GL_SCISSOR_TEST);
    GLint array_buffer_binding;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_buffer_binding);
    
    uint32_t window_width, window_height;
    get_display_dimensions(&window_width, &window_height);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Calculate the viewport for aspect ratio shit
    int viewport_x = 0, viewport_y = 0;
    int viewport_width = window_width, viewport_height = window_height;

    if (configAspectRatio == 1 /* 4:3 */ || configAspectRatio == 2 /* 16:9 */) {
        float window_aspect = (float)window_width / (float)window_height;
        float target_aspect = (configAspectRatio == 1) ? (4.0f / 3.0f) : (16.0f / 9.0f);

        if (window_aspect > target_aspect) {
            viewport_width = (int)((float)window_height * target_aspect);
            viewport_x = (window_width - viewport_width) / 2;
        }
        else if (window_aspect < target_aspect) {
            viewport_height = (int)((float)window_width / target_aspect);
            viewport_y = (window_height - viewport_height) / 2;
        }
    }
    
    glViewport(viewport_x, viewport_y, viewport_width, viewport_height);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    
    // Clear the entire window with black, then draw the centered framebuffer
    if ((configAspectRatio == 1 || configAspectRatio == 2) && (viewport_x > 0 || viewport_y > 0)) {
        glViewport(0, 0, window_width, window_height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(viewport_x, viewport_y, viewport_width, viewport_height);
    }

    glUseProgram(blit_program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(blit_texture_location, 0);

    glBindBuffer(GL_ARRAY_BUFFER, blit_vbo);
    glEnableVertexAttribArray(blit_position_location);
    glVertexAttribPointer(blit_position_location, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(blit_texcoord_location);
    glVertexAttribPointer(blit_texcoord_location, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableVertexAttribArray(blit_position_location);
    glDisableVertexAttribArray(blit_texcoord_location);

    // Restore the state
    glBindBuffer(GL_ARRAY_BUFFER, array_buffer_binding);
    
    if (depth_test_enabled)
        glEnable(GL_DEPTH_TEST);
    if (blend_enabled)
        glEnable(GL_BLEND);
    if (scissor_test_enabled)
        glEnable(GL_SCISSOR_TEST);
}

static bool gfx_opengl_z_is_from_0_to_1(void) {
    return false;
}

static void gfx_opengl_vertex_array_set_attribs(struct ShaderProgram *prg) {
    size_t num_floats = prg->num_floats;
    size_t pos = 0;

    for (int i = 0; i < prg->num_attribs; i++) {
        glEnableVertexAttribArray(prg->attrib_locations[i]);
        glVertexAttribPointer(prg->attrib_locations[i], prg->attrib_sizes[i], GL_FLOAT, GL_FALSE, num_floats * sizeof(float), (void *) (pos * sizeof(float)));
        pos += prg->attrib_sizes[i];
    }
}

static void gfx_opengl_set_shader_uniforms(struct ShaderProgram *prg) {
    if (prg->used_noise) {
        glUniform1i(prg->frame_count_location, frame_count);
        glUniform1i(prg->window_height_location, current_height);
    }
}

static void gfx_opengl_set_texture_uniforms(struct ShaderProgram *prg, const int tile) {
    if (prg != NULL && prg->used_textures[tile] && opengl_tex[tile]) {
        if (tile == 0) {
            glUniform2f(prg->uTex0Size_location, opengl_tex[tile]->size[0], opengl_tex[tile]->size[1]);
            glUniform1i(prg->uTex0Filter_location, opengl_tex[tile]->filter);
        }
        else {
            glUniform2f(prg->uTex1Size_location, opengl_tex[tile]->size[0], opengl_tex[tile]->size[1]);
            glUniform1i(prg->uTex1Filter_location, opengl_tex[tile]->filter);
        }
    }
}

static void gfx_opengl_unload_shader(struct ShaderProgram *old_prg) {
    if (old_prg != NULL) {
        for (int i = 0; i < old_prg->num_attribs; i++) {
            glDisableVertexAttribArray(old_prg->attrib_locations[i]);
        }
    }
    current_program = NULL;
}

static void gfx_opengl_load_shader(struct ShaderProgram *new_prg) {
    current_program = new_prg;
    glUseProgram(new_prg->opengl_program_id);
    gfx_opengl_vertex_array_set_attribs(new_prg);
    gfx_opengl_set_shader_uniforms(new_prg);
    gfx_opengl_set_texture_uniforms(new_prg, 0);
    gfx_opengl_set_texture_uniforms(new_prg, 1);
}

static void append_str(char *buf, size_t *len, const char *str) {
    while (*str != '\0') buf[(*len)++] = *str++;
}

static void append_line(char *buf, size_t *len, const char *str) {
    while (*str != '\0') buf[(*len)++] = *str++;
    buf[(*len)++] = '\n';
}

static const char *shader_item_to_str(uint32_t item, bool with_alpha, bool only_alpha, bool inputs_have_alpha, bool hint_single_element) {
    if (!only_alpha) {
        switch (item) {
            case SHADER_0:
                return with_alpha ? "vec4(0.0, 0.0, 0.0, 0.0)" : "vec3(0.0, 0.0, 0.0)";
            case SHADER_INPUT_1:
                return with_alpha || !inputs_have_alpha ? "vInput1" : "vInput1.rgb";
            case SHADER_INPUT_2:
                return with_alpha || !inputs_have_alpha ? "vInput2" : "vInput2.rgb";
            case SHADER_INPUT_3:
                return with_alpha || !inputs_have_alpha ? "vInput3" : "vInput3.rgb";
            case SHADER_INPUT_4:
                return with_alpha || !inputs_have_alpha ? "vInput4" : "vInput4.rgb";
            case SHADER_TEXEL0:
                return with_alpha ? "texVal0" : "texVal0.rgb";
            case SHADER_TEXEL0A:
                return hint_single_element ? "texVal0.a" :
                    (with_alpha ? "vec4(texVal0.a, texVal0.a, texVal0.a, texVal0.a)" : "vec3(texVal0.a, texVal0.a, texVal0.a)");
            case SHADER_TEXEL1:
                return with_alpha ? "texVal1" : "texVal1.rgb";
        }
    } else {
        switch (item) {
            case SHADER_0:
                return "0.0";
            case SHADER_INPUT_1:
                return "vInput1.a";
            case SHADER_INPUT_2:
                return "vInput2.a";
            case SHADER_INPUT_3:
                return "vInput3.a";
            case SHADER_INPUT_4:
                return "vInput4.a";
            case SHADER_TEXEL0:
                return "texVal0.a";
            case SHADER_TEXEL0A:
                return "texVal0.a";
            case SHADER_TEXEL1:
                return "texVal1.a";
        }
    }
}

static void append_formula(char *buf, size_t *len, uint8_t c[2][4], bool do_single, bool do_multiply, bool do_mix, bool with_alpha, bool only_alpha, bool opt_alpha) {
    if (do_single) {
        append_str(buf, len, shader_item_to_str(c[only_alpha][3], with_alpha, only_alpha, opt_alpha, false));
    } else if (do_multiply) {
        append_str(buf, len, shader_item_to_str(c[only_alpha][0], with_alpha, only_alpha, opt_alpha, false));
        append_str(buf, len, " * ");
        append_str(buf, len, shader_item_to_str(c[only_alpha][2], with_alpha, only_alpha, opt_alpha, true));
    } else if (do_mix) {
        append_str(buf, len, "mix(");
        append_str(buf, len, shader_item_to_str(c[only_alpha][1], with_alpha, only_alpha, opt_alpha, false));
        append_str(buf, len, ", ");
        append_str(buf, len, shader_item_to_str(c[only_alpha][0], with_alpha, only_alpha, opt_alpha, false));
        append_str(buf, len, ", ");
        append_str(buf, len, shader_item_to_str(c[only_alpha][2], with_alpha, only_alpha, opt_alpha, true));
        append_str(buf, len, ")");
    } else {
        append_str(buf, len, "(");
        append_str(buf, len, shader_item_to_str(c[only_alpha][0], with_alpha, only_alpha, opt_alpha, false));
        append_str(buf, len, " - ");
        append_str(buf, len, shader_item_to_str(c[only_alpha][1], with_alpha, only_alpha, opt_alpha, false));
        append_str(buf, len, ") * ");
        append_str(buf, len, shader_item_to_str(c[only_alpha][2], with_alpha, only_alpha, opt_alpha, true));
        append_str(buf, len, " + ");
        append_str(buf, len, shader_item_to_str(c[only_alpha][3], with_alpha, only_alpha, opt_alpha, false));
    }
}

static struct ShaderProgram *gfx_opengl_create_and_load_new_shader(uint32_t shader_id) {
    struct CCFeatures cc_features;
    gfx_cc_get_features(shader_id, &cc_features);

    char vs_buf[1024];
    char fs_buf[1024];
    size_t vs_len = 0;
    size_t fs_len = 0;
    size_t num_floats = 4;

    // Vertex shader
    append_line(vs_buf, &vs_len, "#version 110");
    append_line(vs_buf, &vs_len, "attribute vec4 aVtxPos;");
    if (cc_features.used_textures[0] || cc_features.used_textures[1]) {
        append_line(vs_buf, &vs_len, "attribute vec2 aTexCoord;");
        append_line(vs_buf, &vs_len, "varying vec2 vTexCoord;");
        num_floats += 2;
    }
    if (cc_features.opt_fog) {
        append_line(vs_buf, &vs_len, "attribute vec4 aFog;");
        append_line(vs_buf, &vs_len, "varying vec4 vFog;");
        num_floats += 4;
    }
    for (int i = 0; i < cc_features.num_inputs; i++) {
        vs_len += sprintf(vs_buf + vs_len, "attribute vec%d aInput%d;\n", cc_features.opt_alpha ? 4 : 3, i + 1);
        vs_len += sprintf(vs_buf + vs_len, "varying vec%d vInput%d;\n", cc_features.opt_alpha ? 4 : 3, i + 1);
        num_floats += cc_features.opt_alpha ? 4 : 3;
    }
    append_line(vs_buf, &vs_len, "void main() {");
    if (cc_features.used_textures[0] || cc_features.used_textures[1]) {
        append_line(vs_buf, &vs_len, "vTexCoord = aTexCoord;");
    }
    if (cc_features.opt_fog) {
        append_line(vs_buf, &vs_len, "vFog = aFog;");
    }
    for (int i = 0; i < cc_features.num_inputs; i++) {
        vs_len += sprintf(vs_buf + vs_len, "vInput%d = aInput%d;\n", i + 1, i + 1);
    }
    append_line(vs_buf, &vs_len, "gl_Position = aVtxPos;");
    append_line(vs_buf, &vs_len, "}");

    // Fragment shader
    append_line(fs_buf, &fs_len, "#version 110");
    //append_line(fs_buf, &fs_len, "precision mediump float;");
    if (cc_features.used_textures[0] || cc_features.used_textures[1]) {
        append_line(fs_buf, &fs_len, "varying vec2 vTexCoord;");
    }
    if (cc_features.opt_fog) {
        append_line(fs_buf, &fs_len, "varying vec4 vFog;");
    }
    for (int i = 0; i < cc_features.num_inputs; i++) {
        fs_len += sprintf(fs_buf + fs_len, "varying vec%d vInput%d;\n", cc_features.opt_alpha ? 4 : 3, i + 1);
    }
    if (cc_features.used_textures[0]) {
        append_line(fs_buf, &fs_len, "uniform sampler2D uTex0;");
        append_line(fs_buf, &fs_len, "uniform vec2 uTex0Size;");
        append_line(fs_buf, &fs_len, "uniform bool uTex0Filter;");
    }
    if (cc_features.used_textures[1]) {
        append_line(fs_buf, &fs_len, "uniform sampler2D uTex1;");
        append_line(fs_buf, &fs_len, "uniform vec2 uTex1Size;");
        append_line(fs_buf, &fs_len, "uniform bool uTex1Filter;");
    }

    if (cc_features.used_textures[0] || cc_features.used_textures[1]) {
        if (configTextureFiltering == 0) {
            append_line(fs_buf, &fs_len, "vec4 filter3point(in sampler2D tex, in vec2 texCoord, in vec2 texSize) {");
            append_line(fs_buf, &fs_len, "  vec2 texel_pos = texCoord * texSize - 0.5;");
            append_line(fs_buf, &fs_len, "  vec2 frac_pos = fract(texel_pos);");
            append_line(fs_buf, &fs_len, "  vec2 floor_pos = floor(texel_pos);");
            append_line(fs_buf, &fs_len, "  float s = step(1.0, frac_pos.x + frac_pos.y);");
            append_line(fs_buf, &fs_len, "  vec2 p0_offset = floor_pos + vec2(s);");
            append_line(fs_buf, &fs_len, "  vec2 p1_offset = floor_pos + vec2(1.0 - s, s);");
            append_line(fs_buf, &fs_len, "  vec2 p2_offset = floor_pos + vec2(s, 1.0 - s);");
            append_line(fs_buf, &fs_len, "  vec4 c0 = texture2D(tex, (p0_offset + 0.5) / texSize);");
            append_line(fs_buf, &fs_len, "  vec4 c1 = texture2D(tex, (p1_offset + 0.5) / texSize);");
            append_line(fs_buf, &fs_len, "  vec4 c2 = texture2D(tex, (p2_offset + 0.5) / texSize);");
            append_line(fs_buf, &fs_len, "  vec2 weights = abs(frac_pos - s);");
            append_line(fs_buf, &fs_len, "  return c0 + weights.x * (c1 - c0) + weights.y * (c2 - c0);");
            append_line(fs_buf, &fs_len, "}");
            append_line(fs_buf, &fs_len, "vec4 sampleTexture(in sampler2D tex, in vec2 uv, in vec2 texSize, in bool dofilter) {");
            append_line(fs_buf, &fs_len, "if (dofilter)");
            append_line(fs_buf, &fs_len, "  return filter3point(tex, uv, texSize);");
            append_line(fs_buf, &fs_len, "else");
            append_line(fs_buf, &fs_len, "  return texture2D(tex, uv);");
            append_line(fs_buf, &fs_len, "}");
        }
        else {
            append_line(fs_buf, &fs_len, "vec4 sampleTexture(in sampler2D tex, in vec2 uv, in vec2 texSize, in bool dofilter) {");
            append_line(fs_buf, &fs_len, "return texture2D(tex, uv);");
            append_line(fs_buf, &fs_len, "}");
        }
    }

    if (cc_features.opt_alpha && cc_features.opt_noise) {
        append_line(fs_buf, &fs_len, "uniform int frame_count;");
        append_line(fs_buf, &fs_len, "uniform int window_height;");

        append_line(fs_buf, &fs_len, "float random(in vec3 value) {");
        append_line(fs_buf, &fs_len, "    float random = dot(sin(value), vec3(12.9898, 78.233, 37.719));");
        append_line(fs_buf, &fs_len, "    return fract(sin(random) * 143758.5453);");
        append_line(fs_buf, &fs_len, "}");
    }

    append_line(fs_buf, &fs_len, "void main() {");

    if (cc_features.used_textures[0]) {
        append_line(fs_buf, &fs_len, "vec4 texVal0 = sampleTexture(uTex0, vTexCoord, uTex0Size, uTex0Filter);");
    }
    if (cc_features.used_textures[1]) {
        append_line(fs_buf, &fs_len, "vec4 texVal1 = sampleTexture(uTex1, vTexCoord, uTex1Size, uTex1Filter);");
    }

    append_str(fs_buf, &fs_len, cc_features.opt_alpha ? "vec4 texel = " : "vec3 texel = ");
    if (!cc_features.color_alpha_same && cc_features.opt_alpha) {
        append_str(fs_buf, &fs_len, "vec4(");
        append_formula(fs_buf, &fs_len, cc_features.c, cc_features.do_single[0], cc_features.do_multiply[0], cc_features.do_mix[0], false, false, true);
        append_str(fs_buf, &fs_len, ", ");
        append_formula(fs_buf, &fs_len, cc_features.c, cc_features.do_single[1], cc_features.do_multiply[1], cc_features.do_mix[1], true, true, true);
        append_str(fs_buf, &fs_len, ")");
    } else {
        append_formula(fs_buf, &fs_len, cc_features.c, cc_features.do_single[0], cc_features.do_multiply[0], cc_features.do_mix[0], cc_features.opt_alpha, false, cc_features.opt_alpha);
    }
    append_line(fs_buf, &fs_len, ";");

    if (cc_features.opt_texture_edge && cc_features.opt_alpha) {
        append_line(fs_buf, &fs_len, "if (texel.a > 0.3) texel.a = 1.0; else discard;");
    }
    // TODO discard if alpha is 0?
    if (cc_features.opt_fog) {
        if (cc_features.opt_alpha) {
            append_line(fs_buf, &fs_len, "texel = vec4(mix(texel.rgb, vFog.rgb, vFog.a), texel.a);");
        } else {
            append_line(fs_buf, &fs_len, "texel = mix(texel, vFog.rgb, vFog.a);");
        }
    }

    if (cc_features.opt_alpha && cc_features.opt_noise) {
        if (configNoiseType) {
            append_line(fs_buf, &fs_len, "texel.a *= floor(clamp(random(vec3(floor(gl_FragCoord.xy), float(frame_count)) + texel.a - 0.5), 0.0, 1.0) + 0.5);");
        }
        else {
            append_line(fs_buf, &fs_len, "texel.a *= floor(clamp(random(vec3(floor(gl_FragCoord.xy * (240.0 / float(window_height))), float(frame_count)) + texel.a - 0.5), 0.0, 1.0) + 0.5);");
        }
    }

    if (cc_features.opt_alpha) {
        append_line(fs_buf, &fs_len, "gl_FragColor = texel;");
    } else {
        append_line(fs_buf, &fs_len, "gl_FragColor = vec4(texel, 1.0);");
    }
    append_line(fs_buf, &fs_len, "}");

    vs_buf[vs_len] = '\0';
    fs_buf[fs_len] = '\0';

    /*puts("Vertex shader:");
    puts(vs_buf);
    puts("Fragment shader:");
    puts(fs_buf);
    puts("End");*/

    const GLchar *sources[2] = { vs_buf, fs_buf };
    const GLint lengths[2] = { vs_len, fs_len };
    GLint success;

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &sources[0], &lengths[0]);
    glCompileShader(vertex_shader);
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint max_length = 0;
        glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &max_length);
        char error_log[1024];
        fprintf(stderr, "Vertex shader compilation failed\n");
        glGetShaderInfoLog(vertex_shader, max_length, &max_length, &error_log[0]);
        fprintf(stderr, "%s\n", &error_log[0]);
        abort();
    }

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &sources[1], &lengths[1]);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint max_length = 0;
        glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &max_length);
        char error_log[1024];
        fprintf(stderr, "Fragment shader compilation failed\n");
        glGetShaderInfoLog(fragment_shader, max_length, &max_length, &error_log[0]);
        fprintf(stderr, "%s\n", &error_log[0]);
        abort();
    }

    GLuint shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);

    size_t cnt = 0;

    struct ShaderProgram *prg = &shader_program_pool[shader_program_pool_size++];
    prg->attrib_locations[cnt] = glGetAttribLocation(shader_program, "aVtxPos");
    prg->attrib_sizes[cnt] = 4;
    ++cnt;

    if (cc_features.used_textures[0] || cc_features.used_textures[1]) {
        prg->attrib_locations[cnt] = glGetAttribLocation(shader_program, "aTexCoord");
        prg->attrib_sizes[cnt] = 2;
        ++cnt;
    }

    if (cc_features.opt_fog) {
        prg->attrib_locations[cnt] = glGetAttribLocation(shader_program, "aFog");
        prg->attrib_sizes[cnt] = 4;
        ++cnt;
    }

    for (int i = 0; i < cc_features.num_inputs; i++) {
        char name[16];
        sprintf(name, "aInput%d", i + 1);
        prg->attrib_locations[cnt] = glGetAttribLocation(shader_program, name);
        prg->attrib_sizes[cnt] = cc_features.opt_alpha ? 4 : 3;
        ++cnt;
    }

    prg->shader_id = shader_id;
    prg->opengl_program_id = shader_program;
    prg->num_inputs = cc_features.num_inputs;
    prg->used_textures[0] = cc_features.used_textures[0];
    prg->used_textures[1] = cc_features.used_textures[1];
    prg->num_floats = num_floats;
    prg->num_attribs = cnt;

    gfx_opengl_load_shader(prg);

    if (cc_features.used_textures[0]) {
        GLint sampler_location = glGetUniformLocation(shader_program, "uTex0");
        prg->uTex0Size_location = glGetUniformLocation(shader_program, "uTex0Size");
        prg->uTex0Filter_location = glGetUniformLocation(shader_program, "uTex0Filter");
        glUniform1i(sampler_location, 0);
    }
    if (cc_features.used_textures[1]) {
        GLint sampler_location = glGetUniformLocation(shader_program, "uTex1");
        prg->uTex1Size_location = glGetUniformLocation(shader_program, "uTex1Size");
        prg->uTex1Filter_location = glGetUniformLocation(shader_program, "uTex1Filter");
        glUniform1i(sampler_location, 1);
    }

    if (cc_features.opt_alpha && cc_features.opt_noise) {
        prg->frame_count_location = glGetUniformLocation(shader_program, "frame_count");
        prg->window_height_location = glGetUniformLocation(shader_program, "window_height");
        prg->used_noise = true;
    } else {
        prg->used_noise = false;
    }

    return prg;
}

static struct ShaderProgram *gfx_opengl_lookup_shader(uint32_t shader_id) {
    for (size_t i = 0; i < shader_program_pool_size; i++) {
        if (shader_program_pool[i].shader_id == shader_id) {
            return &shader_program_pool[i];
        }
    }
    return NULL;
}

static void gfx_opengl_shader_get_info(struct ShaderProgram *prg, uint8_t *num_inputs, bool used_textures[2]) {
    *num_inputs = prg->num_inputs;
    used_textures[0] = prg->used_textures[0];
    used_textures[1] = prg->used_textures[1];
}

static GLuint gfx_opengl_new_texture(void) {
    if (num_textures >= tex_cache_size) {
        tex_cache_size += TEX_CACHE_STEP;
        tex_cache = realloc(tex_cache, sizeof(struct GLTexture) * tex_cache_size);
        // Invalidate to prevent pointing to garbage
        opengl_tex[0] = NULL;
        opengl_tex[1] = NULL;
    }
    glGenTextures(1, &tex_cache[num_textures].gltex);
    return num_textures++;
}

static void gfx_opengl_select_texture(int tile, GLuint texture_id) {
     opengl_tex[tile] = tex_cache + texture_id;
     opengl_curtex = tile;
     glActiveTexture(GL_TEXTURE0 + tile);
     glBindTexture(GL_TEXTURE_2D, opengl_tex[tile]->gltex);
     gfx_opengl_set_texture_uniforms(current_program, tile);
}

static void gfx_opengl_upload_texture(const uint8_t *rgba32_buf, int width, int height) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba32_buf);
    opengl_tex[opengl_curtex]->size[0] = width;
    opengl_tex[opengl_curtex]->size[1] = height;
}

static uint32_t gfx_cm_to_opengl(uint32_t val) {
    if (val & G_TX_CLAMP) {
        return GL_CLAMP_TO_EDGE;
    }
    return (val & G_TX_MIRROR) ? GL_MIRRORED_REPEAT : GL_REPEAT;
}

static void gfx_opengl_set_sampler_parameters(int tile, bool linear_filter, uint32_t cms, uint32_t cmt) {
    glActiveTexture(GL_TEXTURE0 + tile);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear_filter ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear_filter ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gfx_cm_to_opengl(cms));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gfx_cm_to_opengl(cmt));
    if (configTextureFiltering > 0 && configAnisotropicFiltering > 0) {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, configAnisotropicFiltering);
    }
    opengl_curtex = tile;
    if (opengl_tex[tile]) {
        opengl_tex[tile]->filter = (configTextureFiltering == 0 && linear_filter);
        gfx_opengl_set_texture_uniforms(current_program, tile);
    }
}

static void gfx_opengl_set_depth_test(bool depth_test) {
    if (depth_test) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
}

static void gfx_opengl_set_depth_mask(bool z_upd) {
    glDepthMask(z_upd ? GL_TRUE : GL_FALSE);
}

static void gfx_opengl_set_zmode_decal(bool zmode_decal) {
    if (zmode_decal) {
        glPolygonOffset(-2, -2);
        glEnable(GL_POLYGON_OFFSET_FILL);
    } else {
        glPolygonOffset(0, 0);
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
}

static void gfx_opengl_set_viewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
    current_height = height;
}

static void gfx_opengl_set_scissor(int x, int y, int width, int height) {
    glScissor(x, y, width, height);
}

static void gfx_opengl_set_use_alpha(bool use_alpha) {
    if (use_alpha) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
}

static void gfx_opengl_draw_triangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    //printf("flushing %d tris\n", buf_vbo_num_tris);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * buf_vbo_len, buf_vbo, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 3 * buf_vbo_num_tris);
}

static void gfx_opengl_init(void) {
#if FOR_WINDOWS
    glewInit();
#endif
    
    tex_cache_size = TEX_CACHE_STEP;
    tex_cache = calloc(tex_cache_size, sizeof(struct GLTexture));
    if (!tex_cache) {
        // Handling memory allocation failure
        fprintf(stderr, "Out of memory allocating texture cache!\n");
        exit(1);
    }

    glGenBuffers(1, &opengl_vbo);
    
    glBindBuffer(GL_ARRAY_BUFFER, opengl_vbo);
    
    glDepthFunc(GL_LEQUAL);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Enable multisampling if anti-aliasing is configured
    if (configAntiAliasing > 0) {
        glEnable(GL_MULTISAMPLE);
    }
}

static void gfx_opengl_on_resize(void) {
}

static void gfx_opengl_start_frame(void) {
    frame_count++;

    uint32_t window_width, window_height;
    get_display_dimensions(&window_width, &window_height);

    // I'm sweating BULLETS here
    if (internal_framebuffer == 0 || internal_width != gfx_current_dimensions.width || internal_height != gfx_current_dimensions.height) {
        create_internal_framebuffer(gfx_current_dimensions.width, gfx_current_dimensions.height);
        create_blit_shader();
        if (configN64Blur) {
            create_blur_framebuffer(gfx_current_dimensions.width, gfx_current_dimensions.height);
            create_blur_shader();
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, internal_framebuffer);
    glViewport(0, 0, internal_width, internal_height);

    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE); // Must be set to clear Z-buffer
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
}

static void apply_n64_blur(GLuint final_texture) {
    // Save the current state
    // MORS TODO: Surely, this is wildly inefficient? I don't think I'm supposed to do it like this, but it works so idc
    GLboolean depth_test_enabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blend_enabled = glIsEnabled(GL_BLEND);
    GLboolean scissor_test_enabled = glIsEnabled(GL_SCISSOR_TEST);
    GLint array_buffer_binding;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_buffer_binding);

    glBindFramebuffer(GL_FRAMEBUFFER, blur_framebuffer);
    glViewport(0, 0, internal_width, internal_height);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);

    glUseProgram(blur_program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, final_texture);
    glUniform1i(blur_texture_location, 0);
    glUniform1f(blur_screen_width_location, (float)internal_width);

    glBindBuffer(GL_ARRAY_BUFFER, blit_vbo);
    glEnableVertexAttribArray(blur_position_location);
    glVertexAttribPointer(blur_position_location, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(blur_texcoord_location);
    glVertexAttribPointer(blur_texcoord_location, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableVertexAttribArray(blur_position_location);
    glDisableVertexAttribArray(blur_texcoord_location);

    // Restore the state
    glBindBuffer(GL_ARRAY_BUFFER, array_buffer_binding);

    if (depth_test_enabled)
        glEnable(GL_DEPTH_TEST);
    if (blend_enabled)
        glEnable(GL_BLEND);
    if (scissor_test_enabled)
        glEnable(GL_SCISSOR_TEST);
}

static void gfx_opengl_end_frame(void) {
    if (configN64Blur) {
        GLuint final_texture = (configAntiAliasing > 0) ? resolved_color_texture : internal_color_texture;
        if (configAntiAliasing > 0) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, internal_framebuffer);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolved_framebuffer);
            glBlitFramebuffer(0, 0, internal_width, internal_height, 0, 0, internal_width, internal_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
        apply_n64_blur(final_texture);
        blit_internal_framebuffer(blur_texture);
    }
    else {
        blit_internal_framebuffer((configAntiAliasing > 0) ? resolved_color_texture : internal_color_texture);
    }
}

static void gfx_opengl_finish_render(void) {
}

struct GfxRenderingAPI gfx_opengl_api = {
    gfx_opengl_z_is_from_0_to_1,
    gfx_opengl_unload_shader,
    gfx_opengl_load_shader,
    gfx_opengl_create_and_load_new_shader,
    gfx_opengl_lookup_shader,
    gfx_opengl_shader_get_info,
    gfx_opengl_new_texture,
    gfx_opengl_select_texture,
    gfx_opengl_upload_texture,
    gfx_opengl_set_sampler_parameters,
    gfx_opengl_set_depth_test,
    gfx_opengl_set_depth_mask,
    gfx_opengl_set_zmode_decal,
    gfx_opengl_set_viewport,
    gfx_opengl_set_scissor,
    gfx_opengl_set_use_alpha,
    gfx_opengl_draw_triangles,
    gfx_opengl_init,
    gfx_opengl_on_resize,
    gfx_opengl_start_frame,
    gfx_opengl_end_frame,
    gfx_opengl_finish_render
};