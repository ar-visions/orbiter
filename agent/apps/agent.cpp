#include <vk/vk.hpp>

using namespace ion;

const uint32_t WIDTH       = 800;
const uint32_t HEIGHT      = 600;
const symbol   MODEL_NAME  = "flower22";

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;

    Vertex() { }
    Vertex(float *v_pos, int p_index, float *v_uv, int uv_index, float *v_normal, int n_index) {
        pos      = {    v_pos[3 * p_index  + 0], v_pos[3 * p_index + 1], v_pos[3 * p_index + 2] };
        uv       = {     v_uv[2 * uv_index + 0], 1.0f - v_uv[2 * uv_index + 1] };
        normal   = { v_normal[3 * n_index  + 0], v_normal[3 * n_index + 1], v_normal[3 * n_index + 2] };
    }

    /// only run once
    doubly<prop> meta() const {
        return {
            prop { "pos",      pos      },
            prop { "normal",   normal   },
            prop { "uv",       uv       }
        };
    }

    type_register(Vertex);

    bool operator==(const Vertex& b) const { return pos == b.pos && normal == b.normal && uv == b.uv; }
};

/// still using std hash and std unordered_map
namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.uv) << 1);
        }
    };
}

/// uniform has an update method with a pipeline arg
struct UniformBufferObject {
    alignas(16) m44f  model;
    alignas(16) m44f  view;
    alignas(16) m44f  proj;
    alignas(16) vec4f light_dir [MAX_PBR_LIGHTS];
    alignas(16) vec4f light_rgba[MAX_PBR_LIGHTS];

    void update(Pipeline::impl *pipeline) {
        VkExtent2D &ext = pipeline->device->swapChainExtent;

        static auto startTime   = std::chrono::high_resolution_clock::now();
        auto        currentTime = std::chrono::high_resolution_clock::now();
        float       time        = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
        
        model = glm::rotate(m44f(1.0f), time * glm::radians(90.0f) * 0.1f, vec3f(0.0f, 0.0f, 1.0f));
        view  = glm::lookAt(vec3f(2.0f, 2.0f, 5.0f), vec3f(0.0f, 0.0f, 0.0f), vec3f(0.0f, 0.0f, 1.0f));
        proj  = glm::perspective(glm::radians(22.0f), ext.width / (float) ext.height, 0.1f, 10.0f);
        proj[1][1] *= -1;

        /// setup some scene lights
        light_dir [0] = vec4f(glm::normalize(vec3f(0.0f,  1.0f, -1.0f)), 0.0f);
        light_rgba[0] = vec4f(1.0, 1.0, 1.0, 1.0);

        light_dir [1] = vec4f(glm::normalize(vec3f(-1.0f, 0.0f, 1.0f)), 0.0f);
        light_rgba[1] = vec4f(1.0, 0.2, 0.2, 1.0);

        light_dir [2] = vec4f(glm::normalize(vec3f(-0.5f, 1.0f, 0.3f)), 0.0f);
        light_rgba[2] = vec4f(0.0, 0.2, 0.6, 1.0);
    }
};

/// we need pipelines for the ground & perimeter
/// Earth needs multiple pipeline
struct GardenView:mx {
    struct impl {
        Vulkan    vk { 1, 0 }; /// this lazy loads 1.0 when GPU performs that action [singleton data]
        vec2i     sz;          /// store current window size
        GPU       gpu;         /// GPU class, responsible for holding onto GPU, Surface and GLFWwindow
        Device    device;      /// Device created with GPU
        lambda<void()> reloading;

        Pipeline pipeline; /// pipeline for single object scene

        static void resized(vec2i &sz, GardenView::impl* app) {
            app->sz = sz;
            app->device->framebufferResized = true;
        }

        void init(vec2i &sz) {
            gpu      = GPU::select(sz, ResizeFn(resized), this);
            device   = Device::create(gpu);
            pipeline = Pipeline(Graphics<UniformBufferObject, Vertex>(device, MODEL_NAME));
        }

        void run() {
            while (!glfwWindowShouldClose(gpu->window)) {
                glfwPollEvents();
                device->mtx.lock();
                device->drawFrame(pipeline);
                device->mtx.unlock();
            }
            vkDeviceWaitIdle(device);
        }

        operator bool() { return sz.x > 0; }
        type_register(impl);
    };
    
    mx_object(GardenView, mx, impl);

    GardenView(vec2i sz):GardenView() {
        data->init(sz);
    }

    /// return the class in main() to loop and return exit-code
    operator int() {
        try {
            data->run();
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
};

int main() {
    return GardenView({ WIDTH, HEIGHT });
}
