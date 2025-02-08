#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "graphics/draw_info/draw_info.hpp"
#include "graphics/window/window.hpp"
#include "graphics/shader_cache/shader_cache.hpp"
#include "graphics/batcher/generated/batcher.hpp"
#include "graphics/vertex_geometry/vertex_geometry.hpp"
#include "graphics/colors/colors.hpp"

#include "utility/input_state/input_state.hpp"
#include "utility/glfw_lambda_callback_manager/glfw_lambda_callback_manager.hpp"
#include "utility/fps_camera/fps_camera.hpp"

#include <cstdio>
#include <cstdlib>

Colors colors;

class PickingTexture {
  public:
    struct PixelInfo {
        int object_id = 0;
        // i have to leave these here for now because I'm using rbg, later on switch to monotone and remove theses
        int draw_id = 0;
        int primitive_id = 0;
    };

    void initialize(unsigned int window_width_px, unsigned int window_height_px) {

        // Create the FBO
        glGenFramebuffers(1, &frame_buffer_gl_handle);
        glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer_gl_handle);

        // Create the texture object for the primitive information buffer
        glGenTextures(1, &picking_texture_gl_handle);
        glBindTexture(GL_TEXTURE_2D, picking_texture_gl_handle);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32UI, window_width_px, window_height_px, 0, GL_RGB_INTEGER,
                     GL_UNSIGNED_INT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, picking_texture_gl_handle, 0);

        // Create the texture object for the depth buffer
        glGenTextures(1, &depth_texture_gl_handle);
        glBindTexture(GL_TEXTURE_2D, depth_texture_gl_handle);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, window_width_px, window_height_px, 0, GL_DEPTH_COMPONENT,
                     GL_FLOAT, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture_gl_handle, 0);

        // Verify that the FBO is correct
        GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

        if (Status != GL_FRAMEBUFFER_COMPLETE) {
            printf("FB error, status: 0x%x\n", Status);
            exit(1);
        }

        // Restore the default framebuffer
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void enable_writing() { glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frame_buffer_gl_handle); }

    void disable_writing() {
        // Bind back the default framebuffer
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }

    PixelInfo read_pixel(unsigned int x, unsigned int y) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, frame_buffer_gl_handle);

        glReadBuffer(GL_COLOR_ATTACHMENT0);

        PixelInfo pixel;
        glReadPixels(x, y, 1, 1, GL_RGB_INTEGER, GL_UNSIGNED_INT, &pixel);

        glReadBuffer(GL_NONE);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        return pixel;
    }

  private:
    GLuint frame_buffer_gl_handle = 0;
    GLuint picking_texture_gl_handle = 0;
    GLuint depth_texture_gl_handle = 0;
};

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

    bool start_with_mouse_captured = true;
    window.initialize_glfw_glad_and_return_window(SCREEN_WIDTH, SCREEN_HEIGHT, "glfw window", false,
                                                  start_with_mouse_captured, false);

    InputState input_state;
    FPSCamera fps_camera;

    float cam_reach = 3;

    std::function<void(unsigned int)> char_callback = [](unsigned int codepoint) {};
    std::function<void(int, int, int, int)> key_callback = [&](int key, int scancode, int action, int mods) {
        if (action == GLFW_PRESS || action == GLFW_RELEASE) {
            Key &active_key = *input_state.glfw_code_to_key.at(key);
            bool is_pressed = (action == GLFW_PRESS);
            active_key.pressed_signal.set_signal(is_pressed);
        }
    };
    std::function<void(double, double)> mouse_pos_callback = [&](double xpos, double ypos) {
        fps_camera.mouse_callback(xpos, ypos);
    };
    std::function<void(int, int, int)> mouse_button_callback = [&](int button, int action, int mods) {
        if (action == GLFW_PRESS || action == GLFW_RELEASE) {
            Key &active_key = *input_state.glfw_code_to_key.at(button);
            bool is_pressed = (action == GLFW_PRESS);
            active_key.pressed_signal.set_signal(is_pressed);
        }
    };
    GLFWLambdaCallbackManager glcm(window.glfw_window, char_callback, key_callback, mouse_pos_callback,
                                   mouse_button_callback);

    std::vector<ShaderType> requested_shaders = {
        ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_COLORED_VERTEX,
        ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_OBJECT_ID,
    };
    ShaderCache shader_cache(requested_shaders, sinks);
    Batcher batcher(shader_cache);

    GLuint ltw_matrices_gl_name;
    glm::mat4 ltw_matrices[1024];

    // initialize all matrices to identity matrices
    for (int i = 0; i < 1024; ++i) {
        ltw_matrices[i] = glm::mat4(1.0f);
    }

    glGenBuffers(1, &ltw_matrices_gl_name);
    glBindBuffer(GL_UNIFORM_BUFFER, ltw_matrices_gl_name);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(ltw_matrices), ltw_matrices, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ltw_matrices_gl_name);

    int width, height;

    /*IndexedVertexPositions ivp = generate_unit_cube();*/
    /*ivp.transform.scale = glm::vec3(.25);*/
    /*ivp.transform.rotation = glm::vec3(.1);*/

    IndexedVertexPositions cone_ivp = vertex_geometry::generate_cone(10, 1, .25);
    cone_ivp.transform.rotation = glm::vec3(.3, .2, .1);

    ltw_matrices[1] = cone_ivp.transform.get_transform_matrix();

    IndexedVertexPositions cyl_ivp = vertex_geometry::generate_cylinder(10, 1, .25);
    cone_ivp.transform.rotation = glm::vec3(0, .3, .8);

    ltw_matrices[2] = cyl_ivp.transform.get_transform_matrix();

    IndexedVertexPositions *selected_object = nullptr;

    PickingTexture picking_texture;
    picking_texture.initialize(SCREEN_WIDTH, SCREEN_HEIGHT);

    shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_OBJECT_ID,
                             ShaderUniformVariable::CAMERA_TO_CLIP, fps_camera.get_projection_matrix());
    shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_OBJECT_ID,
                             ShaderUniformVariable::WORLD_TO_CAMERA, glm::mat4(1));

    shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_COLORED_VERTEX,
                             ShaderUniformVariable::CAMERA_TO_CLIP, fps_camera.get_projection_matrix());
    shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_COLORED_VERTEX,
                             ShaderUniformVariable::WORLD_TO_CAMERA, glm::mat4(1));

    double last_time = glfwGetTime();
    while (!glfwWindowShouldClose(window.glfw_window)) {
        double current_time = glfwGetTime();
        double delta_time = current_time - last_time;
        last_time = current_time;

        glfwGetFramebufferSize(window.glfw_window, &width, &height);

        shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_OBJECT_ID,
                                 ShaderUniformVariable::WORLD_TO_CAMERA, fps_camera.get_view_matrix());

        shader_cache.set_uniform(ShaderType::CWL_V_TRANSFORMATION_UBOS_1024_WITH_COLORED_VERTEX,
                                 ShaderUniformVariable::WORLD_TO_CAMERA, fps_camera.get_view_matrix());

        fps_camera.process_input(input_state.is_pressed(EKey::LEFT_CONTROL), input_state.is_pressed(EKey::LEFT_SHIFT),
                                 input_state.is_pressed(EKey::w), input_state.is_pressed(EKey::a),
                                 input_state.is_pressed(EKey::s), input_state.is_pressed(EKey::d), delta_time);

        ltw_matrices[1] = cone_ivp.transform.get_transform_matrix();
        ltw_matrices[2] = cyl_ivp.transform.get_transform_matrix();

        glViewport(0, 0, width, height);

        picking_texture.enable_writing();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        std::vector<unsigned int> cone_object_ids(cone_ivp.xyz_positions.size(), 1);
        std::vector<unsigned int> cyl_object_ids(cyl_ivp.xyz_positions.size(), 2);

        batcher.cwl_v_transformation_ubos_1024_with_object_id_shader_batcher.queue_draw(
            0, cone_ivp.indices, cone_object_ids, cone_ivp.xyz_positions, cone_object_ids);

        batcher.cwl_v_transformation_ubos_1024_with_object_id_shader_batcher.queue_draw(
            1, cyl_ivp.indices, cyl_object_ids, cyl_ivp.xyz_positions, cyl_object_ids);
        batcher.cwl_v_transformation_ubos_1024_with_object_id_shader_batcher.draw_everything();

        picking_texture.disable_writing();

        if (input_state.is_pressed(EKey::LEFT_MOUSE_BUTTON)) {
            std::cout << "clicked mouse" << std::endl;

            // using center of screen now because we are relying on where you're looking
            unsigned int center_x = SCREEN_WIDTH / 2;
            unsigned int center_y = SCREEN_HEIGHT / 2;
            PickingTexture::PixelInfo clicked_pixel =
                picking_texture.read_pixel(center_x, SCREEN_HEIGHT - 1 - center_y);

            if (clicked_pixel.object_id == 1) {
                selected_object = &cone_ivp;
            }
            if (clicked_pixel.object_id == 2) {
                selected_object = &cyl_ivp;
            }
        }

        if (input_state.is_pressed(EKey::RIGHT_MOUSE_BUTTON)) {
            selected_object = nullptr;
        }

        if (selected_object != nullptr) {
            selected_object->transform.position =
                fps_camera.transform.position + cam_reach * fps_camera.transform.compute_forward_vector();
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        std::vector<glm::vec3> cs(cone_ivp.xyz_positions.size(), colors.bisque4);
        batcher.cwl_v_transformation_ubos_1024_with_colored_vertex_shader_batcher.queue_draw(
            1, cone_ivp.indices, cone_ivp.xyz_positions, cs, cone_object_ids);

        std::vector<glm::vec3> cyl_cs(cyl_ivp.xyz_positions.size(), colors.orange);
        batcher.cwl_v_transformation_ubos_1024_with_colored_vertex_shader_batcher.queue_draw(
            2, cyl_ivp.indices, cyl_ivp.xyz_positions, cyl_cs, cyl_object_ids);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        batcher.cwl_v_transformation_ubos_1024_with_colored_vertex_shader_batcher.draw_everything();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        glBindBuffer(GL_UNIFORM_BUFFER, ltw_matrices_gl_name);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ltw_matrices), ltw_matrices);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        TemporalBinarySignal::process_all();
        glfwSwapBuffers(window.glfw_window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window.glfw_window);

    glfwTerminate();
    exit(EXIT_SUCCESS);
}
