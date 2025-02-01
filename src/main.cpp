#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "window/window.hpp"
#include "shader_cache/shader_cache.hpp"
#include "batcher/generated/batcher.hpp"
#include "vertex_geometry/vertex_geometry.hpp"

#include <cstdio>
#include <cstdlib>

unsigned int SCREEN_WIDTH = 640;
unsigned int SCREEN_HEIGHT = 480;

static void error_callback(int error, const char *description) { fprintf(stderr, "Error: %s\n", description); }

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

int main() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("mwe_shader_cache_logs.txt", true);
    file_sink->set_level(spdlog::level::info);
    std::vector<spdlog::sink_ptr> sinks = {console_sink, file_sink};

    Window window;
    window.initialize_glfw_glad_and_return_window(SCREEN_WIDTH, SCREEN_HEIGHT, "glfw window", false, false, false);

    std::vector<ShaderType> requested_shaders = {ShaderType::ABSOLUTE_POSITION_WITH_SOLID_COLOR,
                                                 ShaderType::TRANSFORM_V_WITH_TEXTURES};
    ShaderCache shader_cache(requested_shaders, sinks);
    Batcher batcher(shader_cache);

    int width, height;

    glm::vec4 color = glm::vec4(.5, .5, .5, 1);
    shader_cache.set_uniform(ShaderType::ABSOLUTE_POSITION_WITH_SOLID_COLOR, ShaderUniformVariable::RGBA_COLOR, color);

    IndexedVertices ivp = generate_grid(glm::vec3(0, 0, 0), 2, 2, 50, 50, 0.01);

    while (!glfwWindowShouldClose(window.glfw_window)) {

        glfwGetFramebufferSize(window.glfw_window, &width, &height);

        glViewport(0, 0, width, height);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        batcher.absolute_position_with_solid_color_shader_batcher.queue_draw(0, ivp.indices, ivp.vertices);
        batcher.absolute_position_with_solid_color_shader_batcher.draw_everything();

        glfwSwapBuffers(window.glfw_window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window.glfw_window);

    glfwTerminate();
    exit(EXIT_SUCCESS);
}
