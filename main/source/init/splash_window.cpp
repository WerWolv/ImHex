#include "init/splash_window.hpp"

#include <hex/api/imhex_api.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/utils_macos.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <fonts/fontawesome_font.h>
#include <GLFW/glfw3.h>

#include <unistd.h>

#include <chrono>
#include <future>
#include <numeric>

using namespace std::literals::chrono_literals;

namespace hex::init {

    WindowSplash::WindowSplash() : m_window(nullptr) {
        this->initGLFW();
        this->initImGui();

        ImHexApi::System::impl::setGPUVendor(reinterpret_cast<const char *>(glGetString(GL_VENDOR)));
    }

    WindowSplash::~WindowSplash() {
        this->exitImGui();
        this->exitGLFW();
    }


    std::future<bool> WindowSplash::processTasksAsync() {
        return std::async(std::launch::async, [this] {
            bool status = true;

            std::atomic<u32> tasksCompleted = 0;
            for (const auto &[name, task, async] : this->m_tasks) {
                auto runTask = [&, task = task, name = name] {
                    {
                        std::lock_guard guard(this->m_progressMutex);
                        this->m_currTaskName = name;
                    }

                    ON_SCOPE_EXIT {
                        tasksCompleted++;
                        this->m_progress = float(tasksCompleted) / this->m_tasks.size();
                    };

                    auto startTime = std::chrono::high_resolution_clock::now();
                    if (!task())
                        status = false;
                    auto endTime = std::chrono::high_resolution_clock::now();

                    log::info("Task '{}' finished in {} ms", name, std::chrono::duration_cast<std::chrono::milliseconds>(endTime-startTime).count());
                };

                try {
                    if (async) {
                        TaskManager::createBackgroundTask(name, [runTask](auto&){ runTask(); });
                    } else {
                        runTask();
                    }

                } catch (std::exception &e) {
                    log::error("Init task '{}' threw an exception: {}", name, e.what());
                    status = false;
                }
            }

            while (tasksCompleted < this->m_tasks.size()) {
                std::this_thread::sleep_for(100ms);
            }

            // Small extra delay so the last progress step is visible
            std::this_thread::sleep_for(100ms);

            return status;
        });
    }

    bool WindowSplash::loop() {
        // Load splash screen image from romfs
        auto splash = romfs::get("splash.png");
        ImGui::Texture splashTexture = ImGui::Texture(reinterpret_cast<const ImU8 *>(splash.data()), splash.size());

        // If the image couldn't be loaded correctly, something went wrong during the build process
        // Close the application since this would lead to errors later on anyway.
        if (!splashTexture.isValid()) {
            log::fatal("Could not load splash screen image!");
            exit(EXIT_FAILURE);
        }

        // Launch init tasks in background
        auto tasksSucceeded = processTasksAsync();

        auto scale = ImHexApi::System::getGlobalScale();

        // Splash window rendering loop
        while (!glfwWindowShouldClose(this->m_window)) {
            glfwPollEvents();

            // Start a new ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // Draw the splash screen background
            auto drawList = ImGui::GetForegroundDrawList();
            {

                drawList->AddImage(splashTexture, ImVec2(0, 0), splashTexture.getSize() * scale);

                drawList->AddText(ImVec2(15, 120) * scale, ImColor(0xFF, 0xFF, 0xFF, 0xFF), hex::format("WerWolv 2020 - {0}", &__DATE__[7]).c_str());

                #if defined(DEBUG) && defined(GIT_BRANCH) && defined(GIT_COMMIT_HASH)
                    drawList->AddText(ImVec2(15, 140) * scale, ImColor(0xFF, 0xFF, 0xFF, 0xFF), hex::format("{0} : {1} {2}@{3}", IMHEX_VERSION, ICON_FA_CODE_BRANCH, GIT_BRANCH, GIT_COMMIT_HASH).c_str());
                #else
                    drawList->AddText(ImVec2(15, 140) * scale, ImColor(0xFF, 0xFF, 0xFF, 0xFF), hex::format("{0}", IMHEX_VERSION).c_str());
                #endif
            }

            // Draw the task progress bar
            {
                std::lock_guard guard(this->m_progressMutex);
                drawList->AddRectFilled(ImVec2(0, splashTexture.getSize().y - 5) * scale, ImVec2(splashTexture.getSize().x * this->m_progress, splashTexture.getSize().y) * scale, 0xFFFFFFFF);
                drawList->AddText(ImVec2(15, splashTexture.getSize().y - 25) * scale, ImColor(0xFF, 0xFF, 0xFF, 0xFF), hex::format("[{}] {}...", "|/-\\"[ImU32(ImGui::GetTime() * 15) % 4], this->m_currTaskName).c_str());
            }

            // Render the frame
            ImGui::Render();
            int displayWidth, displayHeight;
            glfwGetFramebufferSize(this->m_window, &displayWidth, &displayHeight);
            glViewport(0, 0, displayWidth, displayHeight);
            glClearColor(0.00F, 0.00F, 0.00F, 0.00F);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(this->m_window);

            // Check if all background tasks have finished so the splash screen can be closed
            if (tasksSucceeded.wait_for(0s) == std::future_status::ready) {
                return tasksSucceeded.get();
            }
        }

        return false;
    }

    static void centerWindow(GLFWwindow *window) {
        // Get the primary monitor
        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        if (!monitor)
            return;

        // Get information about the monitor
        const GLFWvidmode *mode = glfwGetVideoMode(monitor);
        if (!mode)
            return;

        // Get the position of the monitor's viewport on the virtual screen
        int monitorX, monitorY;
        glfwGetMonitorPos(monitor, &monitorX, &monitorY);

        // Get the window size
        int windowWidth, windowHeight;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);

        // Center the splash screen on the monitor
        glfwSetWindowPos(window, monitorX + (mode->width - windowWidth) / 2, monitorY + (mode->height - windowHeight) / 2);
    }

    void WindowSplash::initGLFW() {
        glfwSetErrorCallback([](int error, const char *desc) {
            log::error("GLFW Error [{}] : {}", error, desc);
        });

        if (!glfwInit()) {
            log::fatal("Failed to initialize GLFW!");
            exit(EXIT_FAILURE);
        }

        // Configure used OpenGL version
        #if defined(OS_MACOS)
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
        #else
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        #endif

        // Make splash screen non-resizable, undecorated and transparent
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_FLOATING, GLFW_FALSE);

        // Create the splash screen window
        this->m_window = glfwCreateWindow(1, 400, "Starting ImHex...", nullptr, nullptr);
        if (this->m_window == nullptr) {
            log::fatal("Failed to create GLFW window!");
            exit(EXIT_FAILURE);
        }

        // Calculate native scale factor for hidpi displays
        {
            float xScale = 0, yScale = 0;
            glfwGetWindowContentScale(this->m_window, &xScale, &yScale);

            auto meanScale = std::midpoint(xScale, yScale);
            if (meanScale <= 0.0F)
                meanScale = 1.0F;

            #if defined(OS_MACOS)
                meanScale /= getBackingScaleFactor();
            #endif

            ImHexApi::System::impl::setGlobalScale(meanScale);
            ImHexApi::System::impl::setNativeScale(meanScale);

            log::info("Native scaling set to: {:.1f}", meanScale);
        }

        glfwSetWindowSize(this->m_window, 640_scaled, 400_scaled);
        centerWindow(this->m_window);

        glfwMakeContextCurrent(this->m_window);
        glfwSwapInterval(1);
    }

    void WindowSplash::initImGui() {
        // Initialize ImGui
        IMGUI_CHECKVERSION();
        GImGui = ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(this->m_window, true);

        #if defined(OS_MACOS)
            ImGui_ImplOpenGL3_Init("#version 150");
        #else
            ImGui_ImplOpenGL3_Init("#version 130");
        #endif

        auto &io = ImGui::GetIO();

        ImGui::GetStyle().ScaleAllSizes(ImHexApi::System::getGlobalScale());

        // Load fonts necessary for the splash screen
        {
            io.Fonts->Clear();

            ImFontConfig cfg;
            cfg.OversampleH = cfg.OversampleV = 1, cfg.PixelSnapH = true;
            cfg.SizePixels = 13.0_scaled;
            io.Fonts->AddFontDefault(&cfg);

            cfg.MergeMode = true;

            ImWchar fontAwesomeRange[] = {
                ICON_MIN_FA, ICON_MAX_FA, 0
            };
            std::uint8_t *px;
            int w, h;
            io.Fonts->AddFontFromMemoryCompressedTTF(font_awesome_compressed_data, font_awesome_compressed_size, 11.0_scaled, &cfg, fontAwesomeRange);
            io.Fonts->GetTexDataAsRGBA32(&px, &w, &h);

            // Create new font atlas
            GLuint tex;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA8, GL_UNSIGNED_INT, px);
            io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(tex));
        }

        // Don't save window settings for the splash screen
        io.IniFilename = nullptr;
    }

    void WindowSplash::exitGLFW() {
        glfwDestroyWindow(this->m_window);
        glfwTerminate();
    }

    void WindowSplash::exitImGui() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

}
