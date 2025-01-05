#include "window.hpp"
#include "init/splash_window.hpp"

#include <hex/api/imhex_api.hpp>
#include <hex/api/event_manager.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>
#include <fmt/chrono.h>

#include <romfs/romfs.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <opengl_support.h>

#include <wolv/utils/guards.hpp>

#include <future>
#include <numeric>
#include <random>
#include <hex/api/task_manager.hpp>
#include <nlohmann/json.hpp>

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
        this->loadAssets();

        {
            auto glVendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
            auto glRenderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
            auto glVersion = reinterpret_cast<const char *>(glGetString(GL_VERSION));
            auto glShadingLanguageVersion = reinterpret_cast<const char *>(glGetString(GL_SHADING_LANGUAGE_VERSION));

            log::debug("OpenGL Vendor: '{}'", glVendor);
            log::debug("OpenGL Renderer: '{}'", glRenderer);
            log::debug("OpenGL Version: '{}'", glVersion);
            log::debug("OpenGL Shading Language Version: '{}'", glShadingLanguageVersion);

            ImHexApi::System::impl::setGPUVendor(glVendor);
            ImHexApi::System::impl::setGLRenderer(glRenderer);
        }

        RequestAddInitTask::subscribe([this](const std::string& name, bool async, const TaskFunction &function){
            m_tasks.push_back(Task{ name, function, async, false });
        });
    }

    WindowSplash::~WindowSplash() {
        this->exitImGui();
        this->exitGLFW();
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

    static ImColor getHighlightColor(u32 index) {
        static auto highlightConfig = nlohmann::json::parse(romfs::get("splash_colors.json").string());
        static std::list<nlohmann::json> selectedConfigs;
        static nlohmann::json selectedConfig;

        static std::mt19937 random(std::random_device{}());

        if (selectedConfigs.empty()) {
            const auto now = []{
                const auto now = std::chrono::system_clock::now();
                const auto time = std::chrono::system_clock::to_time_t(now);

                return fmt::localtime(time);
            }();

            for (const auto &colorConfig : highlightConfig) {
                if (!colorConfig.contains("time")) {
                    selectedConfigs.push_back(colorConfig);
                } else {
                    const auto &time = colorConfig["time"];
                    const auto &start = time["start"];
                    const auto &end = time["end"];

                    if ((now.tm_mon + 1) >= start[0] && (now.tm_mon + 1) <= end[0]) {
                        if (now.tm_mday >= start[1] && now.tm_mday <= end[1]) {
                            selectedConfigs.push_back(colorConfig);
                        }
                    }
                }
            }

            // Remove the default color theme if there's another one available
            if (selectedConfigs.size() != 1)
                selectedConfigs.erase(selectedConfigs.begin());

            selectedConfig = *std::next(selectedConfigs.begin(), random() % selectedConfigs.size());

            log::debug("Using '{}' highlight color theme", selectedConfig["name"].get<std::string>());
        }

        const auto colorString = selectedConfig["colors"][index % selectedConfig["colors"].size()].get<std::string>();

        if (colorString == "random") {
            float r, g, b;
            ImGui::ColorConvertHSVtoRGB(
                    float(random() % 360) / 100.0F,
                    float(25 + random() % 70) / 100.0F,
                    float(85 + random() % 10) / 100.0F,
                    r, g, b);

            return { r, g, b, 0x50 / 255.0F };
        } else if (colorString.starts_with("#")) {
            u32 color = std::strtoul(colorString.substr(1).c_str(), nullptr, 16);

            return IM_COL32((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, 0x50);
        } else {
            log::error("Invalid color string '{}'", colorString);
            return IM_COL32(0xFF, 0x00, 0xFF, 0xFF);
        }
    }

    void WindowSplash::createTask(const Task& task) {
        auto runTask = [&, task] {
            try {
                // Save an iterator to the current task name
                decltype(m_currTaskNames)::iterator taskNameIter;
                {
                    std::lock_guard guard(m_progressMutex);
                    m_currTaskNames.push_back(task.name + "...");
                    taskNameIter = std::prev(m_currTaskNames.end());
                }

                // When the task finished, increment the progress bar
                ON_SCOPE_EXIT {
                    m_completedTaskCount += 1;
                    m_progress = float(m_completedTaskCount) / float(m_totalTaskCount);
                };

                // Execute the actual task and track the amount of time it took to run
                auto startTime = std::chrono::high_resolution_clock::now();
                bool taskStatus = task.callback();
                auto endTime = std::chrono::high_resolution_clock::now();

                auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

                if (taskStatus)
                    log::info("Task '{}' finished successfully in {} ms", task.name, milliseconds);
                else
                    log::warn("Task '{}' finished unsuccessfully in {} ms", task.name, milliseconds);

                // Track the overall status of the tasks
                m_taskStatus = m_taskStatus && taskStatus;

                // Erase the task name from the list of running tasks
                {
                    std::lock_guard guard(m_progressMutex);
                    m_currTaskNames.erase(taskNameIter);
                }
            } catch (const std::exception &e) {
                log::error("Init task '{}' threw an exception: {}", task.name, e.what());
                m_taskStatus = false;
            } catch (...) {
                log::error("Init task '{}' threw an unidentifiable exception", task.name);
                m_taskStatus = false;
            }
        };

        m_totalTaskCount += 1;

        // If the task can be run asynchronously, run it in a separate thread
        // otherwise run it in this thread and wait for it to finish
        if (task.async) {
            std::thread([name = task.name, runTask = std::move(runTask)] {
                TaskManager::setCurrentThreadName(name);
                runTask();
            }).detach();
        } else {
            runTask();
        }
    }

    std::future<bool> WindowSplash::processTasksAsync() {
        return std::async(std::launch::async, [this] {
            TaskManager::setCurrentThreadName("Init Tasks");

            auto startTime = std::chrono::high_resolution_clock::now();

            // Check every 100ms if all tasks have run
            while (true) {
                // Loop over all registered init tasks
                for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it) {
                    // Construct a new task callback
                    if (!it->running) {
                        this->createTask(*it);
                        it->running = true;
                    }
                }

                {
                    std::scoped_lock lock(m_tasksMutex);
                    if (m_completedTaskCount >= m_totalTaskCount)
                        break;
                }

                std::this_thread::sleep_for(100ms);
            }

            auto endTime = std::chrono::high_resolution_clock::now();

            auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            log::info("ImHex fully started in {}ms", milliseconds);

            // Small extra delay so the last progress step is visible
            std::this_thread::sleep_for(100ms);

            return m_taskStatus.load();
        });
    }


    FrameResult WindowSplash::fullFrame() {
        glfwSetWindowSize(m_window, 640, 400);
        centerWindow(m_window);

        glfwPollEvents();

        // Start a new ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Draw the splash screen background
        auto drawList = ImGui::GetBackgroundDrawList();
        {

            // Draw the splash screen background
            drawList->AddImage(this->m_splashBackgroundTexture, ImVec2(0, 0), this->m_splashBackgroundTexture.getSize());

            {

                // Function to highlight a given number of bytes at a position in the splash screen
                const auto highlightBytes = [&](ImVec2 start, size_t count, ImColor color, float opacity) {
                    // Dimensions and number of bytes that are drawn. Taken from the splash screen image
                    const auto hexSize = ImVec2(29, 18);
                    const auto hexSpacing = ImVec2(17.4, 15);
                    const auto hexStart = ImVec2(27, 127);

                    constexpr auto HexCount = ImVec2(13, 7);

                    bool isStart = true;

                    color.Value.w *= opacity;

                    // Loop over all the bytes on the splash screen
                    for (u32 y = u32(start.y); y < u32(HexCount.y); y += 1) {
                        for (u32 x = u32(start.x); x < u32(HexCount.x); x += 1) {
                            if (count-- == 0)
                                return;

                            // Find the start position of the byte to draw
                            auto pos = hexStart + ImVec2(float(x), float(y)) * (hexSize + hexSpacing);

                            // Fill the rectangle in the byte with the given color
                            drawList->AddRectFilled(pos + ImVec2(0, -hexSpacing.y / 2), pos + hexSize + ImVec2(0, hexSpacing.y / 2), color);

                            // Add some extra color on the right if the current byte isn't the last byte, and we didn't reach the right side of the image
                            if (count > 0 && x != u32(HexCount.x) - 1)
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
                for (const auto &highlight : this->m_highlights)
                    highlightBytes(highlight.start, highlight.count, highlight.color, this->m_progressLerp);
            }

            this->m_progressLerp += (m_progress - this->m_progressLerp) * 0.1F;

            // Draw the splash screen foreground
            drawList->AddImage(this->m_splashTextTexture, ImVec2(0, 0), this->m_splashTextTexture.getSize());

            // Draw the "copyright" notice
            drawList->AddText(ImVec2(35, 85), ImColor(0xFF, 0xFF, 0xFF, 0xFF), hex::format("WerWolv\n2020 - {0}", &__DATE__[7]).c_str());

            // Draw version information
            // In debug builds, also display the current commit hash and branch
            #if defined(DEBUG)
                const static auto VersionInfo = hex::format("{0} : {1}@{2}", ImHexApi::System::getImHexVersion().get(), ImHexApi::System::getCommitBranch(), ImHexApi::System::getCommitHash());
            #else
                const static auto VersionInfo = hex::format("{0}", ImHexApi::System::getImHexVersion().get());
            #endif

            drawList->AddText(ImVec2((this->m_splashBackgroundTexture.getSize().x - ImGui::CalcTextSize(VersionInfo.c_str()).x) / 2, 105), ImColor(0xFF, 0xFF, 0xFF, 0xFF), VersionInfo.c_str());
        }

        // Draw the task progress bar
        {
            std::lock_guard guard(m_progressMutex);

            const auto progressBackgroundStart = ImVec2(99, 357);
            const auto progressBackgroundSize = ImVec2(442, 30);

            const auto progressStart = progressBackgroundStart + ImVec2(0, 20);
            const auto progressSize = ImVec2(progressBackgroundSize.x * m_progress, 10);

            // Draw progress bar
            drawList->AddRectFilled(progressStart, progressStart + progressSize, 0xD0FFFFFF);

            // Draw task names separated by | characters
            if (!m_currTaskNames.empty()) {
                drawList->PushClipRect(progressBackgroundStart, progressBackgroundStart + progressBackgroundSize, true);
                drawList->AddText(progressStart + ImVec2(5, -20), ImColor(0xFF, 0xFF, 0xFF, 0xFF), hex::format("{}", fmt::join(m_currTaskNames, " | ")).c_str());
                drawList->PopClipRect();
            }
        }

        // Render the frame
        ImGui::Render();
        int displayWidth, displayHeight;
        glfwGetFramebufferSize(m_window, &displayWidth, &displayHeight);
        glViewport(0, 0, displayWidth, displayHeight);
        glClearColor(0.00F, 0.00F, 0.00F, 0.00F);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(m_window);

        // Check if all background tasks have finished so the splash screen can be closed
        if (this->m_tasksSucceeded.wait_for(0s) == std::future_status::ready) {
            if (this->m_tasksSucceeded.get()) {
                log::debug("All tasks finished successfully!");
                return FrameResult::Success;
            } else {
                log::warn("All tasks finished, but some failed");
                return FrameResult::Failure;
            }
        }

        return FrameResult::Running;
    }

    bool WindowSplash::loop() {
        // Splash window rendering loop
        while (true) {
            auto frameResult = this->fullFrame();

            if (frameResult == FrameResult::Success)
                return true;
            else if (frameResult == FrameResult::Failure)
                return false;
        }
    }

    void WindowSplash::initGLFW() {
        glfwSetErrorCallback([](int errorCode, const char *desc) {
            bool isWaylandError = errorCode == GLFW_PLATFORM_ERROR;
            #if defined(GLFW_FEATURE_UNAVAILABLE)
                isWaylandError = isWaylandError || (errorCode == GLFW_FEATURE_UNAVAILABLE);
            #endif
            isWaylandError = isWaylandError && std::string_view(desc).contains("Wayland");

            if (isWaylandError) {
                // Ignore error spam caused by Wayland not supporting moving or resizing
                // windows or querying their position and size.
                return;
            }

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
            glfwWindowHint(GLFW_COCOA_GRAPHICS_SWITCHING, GLFW_TRUE);
        #else
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        #endif

	#if defined(OS_LINUX)
        #if defined(GLFW_WAYLAND_APP_ID)
	        glfwWindowHintString(GLFW_WAYLAND_APP_ID, "imhex");
        #endif

        #if defined(GLFW_SCALE_FRAMEBUFFER)
            glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_TRUE);
        #endif

        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
	#endif

        // Make splash screen non-resizable, undecorated and transparent
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_FLOATING, GLFW_FALSE);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);

        // Create the splash screen window
        m_window = glfwCreateWindow(1, 1, "Starting ImHex...", nullptr, nullptr);
        if (m_window == nullptr) {
            hex::nativeErrorMessage(hex::format(
                "Failed to create GLFW window: [{}] {}.\n"
                "You may not have a renderer available.\n"
                "The most common cause of this is using a virtual machine\n"
                "You may want to try a release artifact ending with 'NoGPU'"
                , lastGlfwError.errorCode, lastGlfwError.desc));
            std::exit(EXIT_FAILURE);
        }

        ImHexApi::System::impl::setMainWindowHandle(m_window);

        // Force window to be fully opaque by default
        glfwSetWindowOpacity(m_window, 1.0F);

        // Calculate native scale factor for hidpi displays
        {
            float xScale = 0, yScale = 0;
            glfwGetWindowContentScale(m_window, &xScale, &yScale);

            auto meanScale = std::midpoint(xScale, yScale);
            if (meanScale <= 0.0F)
                meanScale = 1.0F;

            meanScale /= hex::ImHexApi::System::getBackingScaleFactor();                

            ImHexApi::System::impl::setGlobalScale(meanScale);
            ImHexApi::System::impl::setNativeScale(meanScale);

            log::info("Native scaling set to: {:.1f}", meanScale);
        }

        glfwMakeContextCurrent(m_window);
        glfwSwapInterval(1);
    }

    void WindowSplash::initImGui() {
        // Initialize ImGui
        IMGUI_CHECKVERSION();
        GImGui = ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(m_window, true);

        #if defined(OS_MACOS)
            ImGui_ImplOpenGL3_Init("#version 150");
        #elif defined(OS_WEB)
            ImGui_ImplOpenGL3_Init();
            ImGui_ImplGlfw_InstallEmscriptenCallbacks(m_window, "#canvas");
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
            cfg.SizePixels = ImHexApi::Fonts::DefaultFontSize;
            io.Fonts->AddFontDefault(&cfg);

            std::uint8_t *px;
            int w, h;
            io.Fonts->GetTexDataAsAlpha8(&px, &w, &h);

            // Create new font atlas
            GLuint tex;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, px);
            io.Fonts->SetTexID(tex);
        }

        // Don't save window settings for the splash screen
        io.IniFilename = nullptr;
    }

    /**
     * @brief Initialize resources for the splash window
     */
    void WindowSplash::loadAssets() {

        // Load splash screen image from romfs
        this->m_splashBackgroundTexture = ImGuiExt::Texture::fromImage(romfs::get("splash_background.png").span(), ImGuiExt::Texture::Filter::Linear);
        this->m_splashTextTexture = ImGuiExt::Texture::fromImage(romfs::get("splash_text.png").span(), ImGuiExt::Texture::Filter::Linear);

        // If the image couldn't be loaded correctly, something went wrong during the build process
        // Close the application since this would lead to errors later on anyway.
        if (!this->m_splashBackgroundTexture.isValid() || !this->m_splashTextTexture.isValid()) {
            log::error("Could not load splash screen image!");
        }

        std::mt19937 rng(std::random_device{}());

        u32 lastPos = 0;
        u32 lastCount = 0;
        u32 index = 0;
        for (auto &highlight : this->m_highlights) {
            u32 newPos = lastPos + lastCount + (rng() % 35);
            u32 newCount = (rng() % 7) + 3;
            highlight.start.x = float(newPos % 13);
            highlight.start.y = float(newPos / 13);
            highlight.count = newCount;

            highlight.color = getHighlightColor(index);

            lastPos = newPos;
            lastCount = newCount;
            index += 1;
        }
    }

    void WindowSplash::startStartupTasks() {
        // Launch init tasks in the background
        this->m_tasksSucceeded = processTasksAsync();
    }

    void WindowSplash::exitGLFW() const {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    void WindowSplash::exitImGui() const {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

}
