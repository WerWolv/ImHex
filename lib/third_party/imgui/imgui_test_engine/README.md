# Dear ImGui Test Engine + Test Suite

Automation system for [Dear ImGui](https://github.com/ocornut/imgui) and applications/games/engines using Dear ImGui.

Because Dear ImGui tends to be wired in many low-level subsystems within your codebase, you can leverage that to automate actions interacting with them. It may be a very efficient solution to automate and tests many things (engine systems etc.).

## Contents

- [imgui_test_engine/](https://github.com/ocornut/imgui_test_engine/tree/main/imgui_test_engine): dear imgui test engine / automation system (library)
- [imgui_test_suite/](https://github.com/ocornut/imgui_test_engine/tree/main/imgui_test_suite): dear imgui test suite (app)
- [app_minimal/](https://github.com/ocornut/imgui_test_engine/tree/main/app_minimal): minimal demo app showcasing how to integrate the test engine (app)
- [shared/](https://github.com/ocornut/imgui_test_engine/tree/main/shared): shared C++ helpers for apps

## Don't blink

Running Dear ImGui Test Suite, in Fast Mode, with full rendering enabled:

https://user-images.githubusercontent.com/8225057/182409619-cd3bf990-b383-4a6c-a6ba-c5afe7557d6c.mp4

Quick overview of what automation code may look like like:
```cpp
ImGuiTest* test = IM_REGISTER_TEST(e, "demo_test", "test1");
test->TestFunc = [](ImGuiTestContext* ctx)
{
    ctx->SetRef("My Window");           // Set a base path so we don't have to specify full path afterwards
    ctx->ItemClick("My Button");        // Click "My Button" inside "My Window"
    ctx->ItemCheck("Node/Checkbox");    // Open "Node", find "Checkbox", ensure it is checked if not checked already.
    ctx->ItemInputValue("Slider", 123); // Find "Slider" and set the value to 123
    IM_CHECK_EQ(app->SliderValue, 123); // Check value on app side
    
    ctx->MenuCheck("//Dear ImGui Demo/Tools/About Dear ImGui"); // Show Dear ImGui About Window (assume Demo window is open)
};
```

## Overview

- Designed to **automate and test Dear ImGui applications**.
- We also use it to **self-test Dear ImGui itself**, reduce regression and facilitate contributions.
- **Test Engine interacts mostly from the point of view of an end-user, by injecting mouse/keyboard/gamepad** inputs into Dear ImGui's IO. It means it tries to "find its way" toward accomplishing an action. Opening an item may mean CTRL+Tabbing into a given widow, moving things out of the way, scrolling to locate the item, querying its open status, etc.
- It can be used for a variety of testing (smoke testing, integration/functional testing) or automation purposes (running tasks, capturing videos, etc.).
- Your app can be controlled from Dear ImGui + You can now automate your app = **You can test anything exposed in your app/engine**! (not only UI).
- It **can run in your windowed application**. **It can also run headless** (e.g. running GUI tests from a console or on a CI server without rendering).
- It **can run at simulated human speed** (for watching or exporting videos) or **can run in fast mode** (e.g. teleporting mouse).
- It **can export screenshots and videos/gifs**. They can be leveraged for some forms of testing, but also to generate assets for documentation, or notify teams of certain changes. Assets that often need to be updated are best generated from a script inside of manually recreated/cropped/exported.
- You can use it to program high-level commands e.g. `MenuCheck("Edit/Options/Enable Grid")` or run more programmatic queries ("list openable items in that section, then open them all"). So from your POV it could be used for simple smoke testing ("open all our tools") or for more elaborate testing ("interact with xxx and xxx, check result").
- It **can be used as a form of "live tutorial / demo"** where a script can run on an actual user application to showcase features.
- It includes a performance tool and viewer which we used to record/compare performances between builds and branches (optional, requires ImPlot).

## Status

- It is currently a C++ API but down the line we can expect that the commands will be better standardized, stored in data files, called from other languages.
- It has been in use and development since 2018 and only made public at the end of 2022. We'll provide best effort to make it suitable for user needs.
- You will run into problems and shortcomings. We are happy to hear about them and improve the software and documentation accordingly.

## Documentation

See [Wiki](https://github.com/ocornut/imgui_test_engine/wiki) sections:
- [Overview](https://github.com/ocornut/imgui_test_engine/wiki/Overview)
- [Setting Up](https://github.com/ocornut/imgui_test_engine/wiki/Setting-Up)
- [Automation API](https://github.com/ocornut/imgui_test_engine/wiki/Automation-API) (ImGuiTestContext interface)
- [Named References](https://github.com/ocornut/imgui_test_engine/wiki/Named-References) (all nice uses of ImGuiTestRef)
- [Exporting Results](https://github.com/ocornut/imgui_test_engine/wiki/Exporting-Results)
- [Screen & Video Captures](https://github.com/ocornut/imgui_test_engine/wiki/Screen-and-Video-Captures)

## Licenses

- The [imgui_test_engine/](https://github.com/ocornut/imgui_test_engine/tree/main/imgui_test_engine) folder is under the [Dear ImGui Test Engine License](https://github.com/ocornut/imgui_test_engine/blob/main/imgui_test_engine/LICENSE.txt).<BR>TL;DR: free for individuals, educational, open-source and small businesses uses. Paid for larger businesses. Read license for details. License sales to larger businesses are used to fund and sustain the development of Dear ImGui.
- The [imgui_test_suite/](https://github.com/ocornut/imgui_test_engine/tree/main/imgui_test_suite) folder and all other folders are all under the [MIT License](https://github.com/ocornut/imgui_test_engine/blob/main/imgui_test_suite/LICENSE.txt).

