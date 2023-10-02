#include "window.hpp"
#include "init/splash_window.hpp"

#include <hex/api/imhex_api.hpp>
#include <hex/api/task.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/utils_macos.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <fonts/fontawesome_font.h>
#include <GLFW/glfw3.h>

#include <wolv/utils/guards.hpp>

#include <unistd.h>

#include <chrono>
#include <future>
#include <numeric>
#include <random>

#if defined (OS_EMSCRIPTEN)
    #define GLFW_INCLUDE_ES3
    #include <GLES3/gl3.h>
    #include <emscripten/html5.h>
#else
    #include <imgui_impl_opengl3_loader.h>
#endif

using namespace std::literals::chrono_literals;

namespace hex::init {

    struct GlfwError {
        int errorCode = 0;
        std::string desc;
    };

    GlfwError lastGlfwError;

    WindowSplash::WindowSplash() : m_window(nullptr) {
        this->initGLFW();
        this->initImGui();
        this->initMyself();

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

            // Loop over all registered init tasks
            for (const auto &[name, task, async] : this->m_tasks) {

                // Construct a new task callback
                auto runTask = [&, task = task, name = name] {
                    try {
                        // Save an iterator to the current task name
                        decltype(this->m_currTaskNames)::iterator taskNameIter;
                        {
                            std::lock_guard guard(this->m_progressMutex);
                            this->m_currTaskNames.push_back(name + "...");
                            taskNameIter = std::prev(this->m_currTaskNames.end());
                        }

                        // When the task finished, increment the progress bar
                        ON_SCOPE_EXIT {
                            tasksCompleted++;
                            this->m_progress = float(tasksCompleted) / this->m_tasks.size();
                        };

                        // Execute the actual task and track the amount of time it took to run
                        auto startTime = std::chrono::high_resolution_clock::now();
                        bool taskStatus = task();
                        auto endTime = std::chrono::high_resolution_clock::now();

                        log::info("Task '{}' finished {} in {} ms",
                                  name,
                                  taskStatus ? "successfully" : "unsuccessfully",
                                  std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count()
                        );

                        // Track the overall status of the tasks
                        status = status && taskStatus;

                        // Erase the task name from the list of running tasks
                        {
                            std::lock_guard guard(this->m_progressMutex);
                            this->m_currTaskNames.erase(taskNameIter);
                        }
                    } catch (const std::exception &e) {
                        log::error("Init task '{}' threw an exception: {}", name, e.what());
                        status = false;
                    } catch (...) {
                        log::error("Init task '{}' threw an unidentifiable exception", name);
                        status = false;
                    }
                };


                // If the task can be run asynchronously, run it in a separate thread
                // otherwise run it in this thread and wait for it to finish
                if (async) {
                    std::thread([runTask]{ runTask(); }).detach();
                } else {
                    runTask();
                }
            }

            // Check every 100ms if all tasks have run
            while (tasksCompleted < this->m_tasks.size()) {
                std::this_thread::sleep_for(100ms);
            }

            // Small extra delay so the last progress step is visible
            std::this_thread::sleep_for(100ms);

            return status;
        });
    }

    FrameResult WindowSplash::fullFrame() {
        glfwPollEvents();

        // Start a new ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        auto scale = ImHexApi::System::getGlobalScale();

        // Draw the splash screen background
        auto drawList = ImGui::GetBackgroundDrawList();
        {

            // Draw the splash screen background
            drawList->AddImage(this->splashBackgroundTexture, ImVec2(0, 0), this->splashBackgroundTexture.getSize() * scale);

            {

                // Function to highlight a given number of bytes at a position in the splash screen
                const auto highlightBytes = [&](ImVec2 start, size_t count, ImColor color, float opacity) {
                    // Dimensions and number of bytes that are drawn. Taken from the splash screen image
                    const auto hexSize = ImVec2(29, 18) * scale;
                    const auto hexSpacing = ImVec2(17.4, 15) * scale;
                    const auto hexStart = ImVec2(27, 127) * scale;

                    const auto hexCount = ImVec2(13, 7);

                    bool isStart = true;

                    color.Value.w *= opacity;

                    // Loop over all the bytes on the splash screen
                    for (u32 y = u32(start.y); y < u32(hexCount.y); y += 1) {
                        for (u32 x = u32(start.x); x < u32(hexCount.x); x += 1) {
                            if (count-- == 0)
                                return;

                            // Find the start position of the byte to draw
                            auto pos = hexStart + ImVec2(float(x), float(y)) * (hexSize + hexSpacing);

                            // Fill the rectangle in the byte with the given color
                            drawList->AddRectFilled(pos + ImVec2(0, -hexSpacing.y / 2), pos + hexSize + ImVec2(0, hexSpacing.y / 2), color);

                            // Add some extra color on the right if the current byte isn't the last byte, and we didn't reach the right side of the image
                            if (count > 0 && x != u32(hexCount.x) - 1)
                                drawList->AddRectFilled(pos + ImVec2(hexSize.x, -hexSpacing.y / 2), pos + hexSize + ImVec2(hexSpacing.x, hexSpacing.y / 2), color);

                            // Add some extra color on the left if this is the first byte we're highlighting
                            if (isStart) {
                                isStart = false;
                                drawList->AddRectFilled(pos - hexSpacing / 2, pos + ImVec2(0, hexSize.y + hexSpacing.y / 2), color);
                            }

                            // Add some extra color on the right if this is the last byte
                            if (count == 0) {
                                drawList->AddRectFilled(pos + ImVec2(hexSize.x, -hexSpacing.y / 2), pos + hexSize + hexSpacing / 2, color);
                            }
                        }

                        start.x = 0;
                    }
                };

                // Draw all highlights, slowly fading them in as the init tasks progress
                for (const auto &highlight : this->highlights)
                    highlightBytes(highlight.start, highlight.count, highlight.color, this->progressLerp);
            }

            this->progressLerp += (this->m_progress - this->progressLerp) * 0.1F;

            // Draw the splash screen foreground
            drawList->AddImage(this->splashTextTexture, ImVec2(0, 0), this->splashTextTexture.getSize() * scale);

            // Draw the "copyright" notice
            drawList->AddText(ImVec2(35, 85) * scale, ImColor(0xFF, 0xFF, 0xFF, 0xFF), hex::format("WerWolv\n2020 - {0}", &__DATE__[7]).c_str());

            // Draw version information
            // In debug builds, also display the current commit hash and branch
            #if defined(DEBUG)
                const static auto VersionInfo = hex::format("{0} : {1} {2}@{3}", ImHexApi::System::getImHexVersion(), ICON_FA_CODE_BRANCH, ImHexApi::System::getCommitBranch(), ImHexApi::System::getCommitHash());
            #else
                const static auto VersionInfo = hex::format("{0}", ImHexApi::System::getImHexVersion());
            #endif

            drawList->AddText(ImVec2((this->splashBackgroundTexture.getSize().x * scale - ImGui::CalcTextSize(VersionInfo.c_str()).x) / 2, 105 * scale), ImColor(0xFF, 0xFF, 0xFF, 0xFF), VersionInfo.c_str());
        }

        // Draw the task progress bar
        {
            std::lock_guard guard(this->m_progressMutex);

            const auto progressBackgroundStart = ImVec2(99, 357) * scale;
            const auto progressBackgroundSize = ImVec2(442, 30) * scale;

            const auto progressStart = progressBackgroundStart + ImVec2(0, 20) * scale;
            const auto progressSize = ImVec2(progressBackgroundSize.x * this->m_progress, 10 * scale);

            // Draw progress bar
            drawList->AddRectFilled(progressStart, progressStart + progressSize, 0xD0FFFFFF);

            // Draw task names separated by | characters
            if (!this->m_currTaskNames.empty()) {
                drawList->PushClipRect(progressBackgroundStart, progressBackgroundStart + progressBackgroundSize, true);
                drawList->AddText(progressStart + ImVec2(5, -20) * scale, ImColor(0xFF, 0xFF, 0xFF, 0xFF), hex::format("{}", fmt::join(this->m_currTaskNames, " | ")).c_str());
                drawList->PopClipRect();
            }
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
        if (this->tasksSucceeded.wait_for(0s) == std::future_status::ready) {
            if (this->tasksSucceeded.get()) {
                log::debug("All tasks finished with success !");
                return FrameResult::success;
            } else {
                log::warn("All tasks finished, but some failed");
                return FrameResult::failure;
            }
        }

        return FrameResult::wait;
    }

    bool WindowSplash::loop() {
        // Splash window rendering loop
        while (true) {
            auto res = this->fullFrame();
            if (res == FrameResult::success) return true;
            else if (res == FrameResult::failure) return false;
        }
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
        glfwSetErrorCallback([](int errorCode, const char *desc) {
            lastGlfwError.errorCode = errorCode;
            lastGlfwError.desc = std::string(desc);
            log::error("GLFW Error [{}] : {}", errorCode, desc);
        });

        if (!glfwInit()) {
            log::fatal("Failed to initialize GLFW!");
            std::exit(EXIT_FAILURE);
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
        glfwWindowHint(GLFW_SAMPLES, 1);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);

        // Create the splash screen window
        this->m_window = glfwCreateWindow(1, 400, "Starting ImHex...", nullptr, nullptr);
        if (this->m_window == nullptr) {
            hex::nativeErrorMessage(hex::format(
                "Failed to create GLFW window: [{}] {}.\n"
                "You may not have a renderer available.\n"
                "The most common cause of this is using a virtual machine\n"
                "You may want to try a release artifact ending with 'NoGPU'"
                , lastGlfwError.errorCode, lastGlfwError.desc));
            std::exit(EXIT_FAILURE);
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
        #elif defined(OS_EMSCRIPTEN)
            ImGui_ImplOpenGL3_Init();
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
            io.Fonts->GetTexDataAsAlpha8(&px, &w, &h);

            // Create new font atlas
            GLuint tex;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, px);
            io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(tex));
        }

        // Don't save window settings for the splash screen
        io.IniFilename = nullptr;
    }

    /**
     * @brief Initialize resources for the splash window
     */
    void WindowSplash::initMyself() {

        // Load splash screen image from romfs
        this->splashBackgroundTexture = ImGui::Texture(romfs::get("splash_background.png").span());
        this->splashTextTexture = ImGui::Texture(romfs::get("splash_text.png").span());

        // If the image couldn't be loaded correctly, something went wrong during the build process
        // Close the application since this would lead to errors later on anyway.
        if (!this->splashBackgroundTexture.isValid() || !this->splashTextTexture.isValid()) {
            log::fatal("Could not load splash screen image!");
            std::exit(EXIT_FAILURE);
        }



        std::mt19937 rng(std::random_device{}());

        u32 lastPos = 0;
        u32 lastCount = 0;
        for (auto &highlight : this->highlights) {
            auto newPos = lastPos + lastCount + (rng() % 40);
            auto newCount = (rng() % 7) + 3;
            highlight.start.x = newPos % 13;
            highlight.start.y = newPos / 13;
            highlight.count = newCount;

            {
                float r, g, b;
                ImGui::ColorConvertHSVtoRGB(
                        (rng() % 360) / 100.0F,
                        (25 + rng() % 70) / 100.0F,
                        (85 + rng() % 10) / 100.0F,
                        r, g, b);
                highlight.color = ImColor(r, g, b, 0x50 / 255.0F);
            }

            lastPos = newPos;
            lastCount = newCount;
        }
    }

    void WindowSplash::startStartupTasks() {
        // Launch init tasks in the background
        this->tasksSucceeded = processTasksAsync();
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
