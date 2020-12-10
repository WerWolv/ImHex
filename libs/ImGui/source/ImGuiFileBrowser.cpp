#include "ImGuiFileBrowser.h"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui_internal.h"

#include <iostream>
#include <functional>
#include <climits>
#include <string.h>
#include <sstream>
#include <cwchar>
#include <cctype>
#include <algorithm>
#include <cmath>
#if defined (WIN32) || defined (_WIN32) || defined (__WIN32)
#define OSWIN
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include "Dirent/dirent.h"
#include <windows.h>
#else
#include <dirent.h>
#endif // defined (WIN32) || defined (_WIN32)

namespace imgui_addons
{
    ImGuiFileBrowser::ImGuiFileBrowser()
    {
        filter_mode = FilterMode_Files | FilterMode_Dirs;

        show_inputbar_combobox = false;
        validate_file = false;
        show_hidden = false;
        is_dir = false;
        filter_dirty = true;
        is_appearing = true;
        path_input_enabled = false;

        col_items_limit = 12;
        selected_idx = -1;
        selected_ext_idx = 0;
        ext_box_width = -1.0f;
        col_width = 280.0f;

        invfile_modal_id = "Invalid File!";
        repfile_modal_id = "Replace File?";
        selected_fn = "";
        selected_path = "";
        input_fn[0] = '\0';
        temp_dir_input[0] = '\0';

        #ifdef OSWIN
        current_path = "./";
        #else
        initCurrentPath();
        #endif
    }

    ImGuiFileBrowser::~ImGuiFileBrowser()
    {

    }

    void ImGuiFileBrowser::clearFileList()
    {
        //Clear pointer references to subdirs and subfiles
        filtered_dirs.clear();
        filtered_files.clear();
        inputcb_filter_files.clear();

        //Now clear subdirs and subfiles
        subdirs.clear();
        subfiles.clear();
        filter_dirty = true;
        selected_idx = -1;
    }

    void ImGuiFileBrowser::closeDialog()
    {
        valid_types = "";
        valid_exts.clear();
        selected_ext_idx = 0;
        selected_idx = -1;

        input_fn[0] = '\0';  //Hide any text in Input bar for the next time save dialog is opened.
        filter.Clear();     //Clear Filter for the next time open dialog is called.

        show_inputbar_combobox = false;
        validate_file = false;
        show_hidden = false;
        is_dir = false;
        filter_dirty = true;
        is_appearing = true;

        //Clear pointer references to subdirs and subfiles
        filtered_dirs.clear();
        filtered_files.clear();
        inputcb_filter_files.clear();

        //Now clear subdirs and subfiles
        subdirs.clear();
        subfiles.clear();

        ImGui::CloseCurrentPopup();
    }

    bool ImGuiFileBrowser::showFileDialog(const std::string& label, const DialogMode mode, const ImVec2& sz_xy, const std::string& valid_types)
    {
        dialog_mode = mode;
        ImGuiIO& io = ImGui::GetIO();
        max_size = ImVec2(       io.DisplaySize.x,        io.DisplaySize.y);
        min_size = ImVec2(0.5f * io.DisplaySize.x, 0.5f * io.DisplaySize.y);


        ImGui::SetNextWindowSizeConstraints(min_size, max_size);
        ImGui::SetNextWindowPos(io.DisplaySize * 0.5f, ImGuiCond_Appearing, ImVec2(0.5f,0.5f));
        ImGui::SetNextWindowSize(ImVec2(std::max(sz_xy.x, min_size.x), std::max(sz_xy.y, min_size.y)), ImGuiCond_Appearing);

        //Set Proper Filter Mode.
        if(mode == DialogMode::SELECT)
            filter_mode = FilterMode_Dirs;
        else
            filter_mode = FilterMode_Files | FilterMode_Dirs;

        if (ImGui::BeginPopupModal(label.c_str(), nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            bool show_error = false;

            // If this is the initial run, read current directory and load data once.
            if(is_appearing)
            {
                selected_fn.clear();
                selected_path.clear();
                if(mode != DialogMode::SELECT)
                {
                    this->valid_types = valid_types;
                    setValidExtTypes(valid_types);
                }

                /* If current path is empty (can happen on Windows if user closes dialog while inside MyComputer.
                 * Since this is a virtual folder, path would be empty) load the drives on Windows else initialize the current path on Unix.
                 */
                if(current_path.empty())
                {
                    #ifdef OSWIN
                    show_error |= !(loadWindowsDrives());
                    #else
                    initCurrentPath();
                    show_error |= !(readDIR(current_path));
                    #endif // OSWIN
                }
                else
                    show_error |= !(readDIR(current_path));
                is_appearing = false;
            }

            show_error |= renderNavAndSearchBarRegion();
            show_error |= renderFileListRegion();
            show_error |= renderInputTextAndExtRegion();
            show_error |= renderButtonsAndCheckboxRegion();

            if(validate_file)
            {
                validate_file = false;
                bool check = validateFile();

                if(!check && dialog_mode == DialogMode::OPEN)
                {
                    ImGui::OpenPopup(invfile_modal_id.c_str());
                    selected_fn.clear();
                    selected_path.clear();
                }

                else if(!check && dialog_mode == DialogMode::SAVE)
                    ImGui::OpenPopup(repfile_modal_id.c_str());

                else if(!check && dialog_mode == DialogMode::SELECT)
                {
                    selected_fn.clear();
                    selected_path.clear();
                    show_error = true;
                    error_title = "Invalid Directory!";
                    error_msg = "Invalid Directory Selected. Please make sure the directory exists.";
                }

                //If selected file passes through validation check, set path to the file and close file dialog
                if(check)
                {
                    selected_path = current_path + selected_fn;

                    //Add a trailing "/" to emphasize its a directory not a file. If you want just the dir name it's accessible through "selected_fn"
                    if(dialog_mode == DialogMode::SELECT)
                        selected_path += "/";
                    closeDialog();
                }
            }

            // We don't need to check as the modals will only be shown if OpenPopup is called
            showInvalidFileModal();
            if(showReplaceFileModal())
                closeDialog();

            //Show Error Modal if there was an error opening any directory
            if(show_error)
                ImGui::OpenPopup(error_title.c_str());
            showErrorModal();

            ImGui::EndPopup();
            return (!selected_fn.empty() && !selected_path.empty());
        }
        else
            return false;
    }

    bool ImGuiFileBrowser::renderNavAndSearchBarRegion()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        bool show_error = false;
        float frame_height = ImGui::GetFrameHeight();
        float list_item_height = GImGui->FontSize + style.ItemSpacing.y;

        ImVec2 pw_content_size = ImGui::GetWindowSize() - style.WindowPadding * 2.0;
        ImVec2 sw_size = ImVec2(ImGui::CalcTextSize("Random").x + 140, style.WindowPadding.y * 2.0f + frame_height);
        ImVec2 sw_content_size = sw_size - style.WindowPadding * 2.0;
        ImVec2 nw_size = ImVec2(pw_content_size.x - style.ItemSpacing.x - sw_size.x, sw_size.y);

        ImGui::BeginChild("##NavigationWindow", nw_size, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);

        if (!this->path_input_enabled) {

            if (ImGui::Button("D")) {
                memcpy(temp_dir_input, this->current_path.c_str(), this->current_path.length());
                this->path_input_enabled = true;
            }
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.882f, 0.745f, 0.078f, 1.0f));
            for (int i = 0; i < current_dirlist.size(); i++) {
                if (ImGui::Button(current_dirlist[i].c_str())) {
                    //If last button clicked, nothing happens
                    if (i != current_dirlist.size() - 1)
                        show_error |= !(onNavigationButtonClick(i));
                }

                //Draw Arrow Buttons
                if (i != current_dirlist.size() - 1) {
                    ImGui::SameLine(0, 0);
                    float next_label_width = ImGui::CalcTextSize(current_dirlist[i + 1].c_str()).x;

                    if (i + 1 < current_dirlist.size() - 1)
                        next_label_width += frame_height + ImGui::CalcTextSize(">>").x;

                    if (ImGui::GetCursorPosX() + next_label_width >= (nw_size.x - style.WindowPadding.x * 3.0)) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 0.01f));
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

                        //Render a drop down of navigation items on button press
                        if (ImGui::Button(">>"))
                            ImGui::OpenPopup("##NavBarDropboxPopup");
                        if (ImGui::BeginPopup("##NavBarDropboxPopup")) {
                            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.125f, 0.125f, 0.125f, 1.0f));
                            if (ImGui::ListBoxHeader("##NavBarDropBox", ImVec2(0, list_item_height * 5))) {
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.882f, 0.745f, 0.078f, 1.0f));
                                for (int j = i + 1; j < current_dirlist.size(); j++) {
                                    if (ImGui::Selectable(current_dirlist[j].c_str(), false) &&
                                        j != current_dirlist.size() - 1) {
                                        show_error |= !(onNavigationButtonClick(j));
                                        ImGui::CloseCurrentPopup();
                                    }
                                }
                                ImGui::PopStyleColor();
                                ImGui::ListBoxFooter();
                            }
                            ImGui::PopStyleColor();
                            ImGui::EndPopup();
                        }
                        ImGui::PopStyleColor(2);
                        break;
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 0.01f));
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                        ImGui::ArrowButtonEx("##Right", ImGuiDir_Right, ImVec2(frame_height, frame_height),
                                             ImGuiButtonFlags_Disabled);
                        ImGui::SameLine(0, 0);
                        ImGui::PopStyleColor(2);
                    }
                }
            }

            ImGui::PopStyleColor();


        } else {
            ImGui::PushItemWidth(nw_size.x - 15);
            if (ImGui::InputText("##nolabel", temp_dir_input, 256, ImGuiInputTextFlags_EnterReturnsTrue)) {
                if(readDIR(std::string(temp_dir_input) + "/")) {
                    parsePathTabs(std::string(temp_dir_input) + "/");
                    current_path = std::string(temp_dir_input);
                    if (current_path.back() != '/')
                        current_path.push_back('/');
                    this->path_input_enabled = false;
                    memset(this->temp_dir_input, 0x00, 256);
                }
            }
            ImGui::PopItemWidth();
        }

        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("##SearchWindow", sw_size, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);

        //Render Search/Filter bar
        float marker_width = ImGui::CalcTextSize("(?)").x + style.ItemSpacing.x;
        if(filter.Draw("##SearchBar", sw_content_size.x - marker_width) || filter_dirty )
            filterFiles(filter_mode);

        //If filter bar was focused clear selection
        if(ImGui::GetFocusID() == ImGui::GetID("##SearchBar"))
            selected_idx = -1;

        ImGui::SameLine();
        showHelpMarker("Filter (inc, -exc)");

        ImGui::EndChild();
        return show_error;
    }

    bool ImGuiFileBrowser::renderFileListRegion()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec2 pw_size = ImGui::GetWindowSize();
        bool show_error = false;
        float list_item_height = ImGui::CalcTextSize("").y + style.ItemSpacing.y;
        float input_bar_ypos = pw_size.y - ImGui::GetFrameHeightWithSpacing() * 2.5f - style.WindowPadding.y;
        float window_height = input_bar_ypos - ImGui::GetCursorPosY() - style.ItemSpacing.y;
        float window_content_height = window_height - style.WindowPadding.y * 2.0f;
        float min_content_size = pw_size.x - style.WindowPadding.x * 4.0f;

        if(window_content_height <= 0.0f)
            return show_error;

        //Reinitialize the limit on number of selectables in one column based on height
        col_items_limit = static_cast<int>(std::max(1.0f, window_content_height/list_item_height));
        int num_cols = static_cast<int>(std::max(1.0f, std::ceil(static_cast<float>(filtered_dirs.size() + filtered_files.size()) / col_items_limit)));

        //Limitation by ImGUI in 1.75. If columns are greater than 64 readjust the limit on items per column and recalculate number of columns
        if(num_cols > 64)
        {
            int exceed_items_amount = (num_cols - 64) * col_items_limit;
            col_items_limit += static_cast<int>(std::ceil(exceed_items_amount/64.0));
            num_cols = static_cast<int>(std::max(1.0f, std::ceil(static_cast<float>(filtered_dirs.size() + filtered_files.size()) / col_items_limit)));
        }

        float content_width = num_cols * col_width;
        if(content_width < min_content_size)
            content_width = 0;

        ImGui::SetNextWindowContentSize(ImVec2(content_width, 0));
        ImGui::BeginChild("##ScrollingRegion", ImVec2(0, window_height), true, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::Columns(num_cols);

        //Output directories in yellow
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.882f, 0.745f, 0.078f,1.0f));
        int items = 0;
        for (int i = 0; i < filtered_dirs.size(); i++)
        {
            if(!filtered_dirs[i]->is_hidden || show_hidden)
            {
                items++;
                if(ImGui::Selectable(filtered_dirs[i]->name.c_str(), selected_idx == i && is_dir, ImGuiSelectableFlags_AllowDoubleClick))
                {
                    selected_idx = i;
                    is_dir = true;

                    // If dialog mode is SELECT then copy the selected dir name to the input text bar
                    if(dialog_mode == DialogMode::SELECT)
                        strcpy(input_fn, filtered_dirs[i]->name.c_str());

                    if (ImGui::IsMouseClicked(0))
                        path_input_enabled = false;

                    if(ImGui::IsMouseDoubleClicked(0))
                    {
                        show_error |= !(onDirClick(i));
                        break;
                    }
                }
                if( (items) % col_items_limit == 0)
                    ImGui::NextColumn();
            }
        }
        ImGui::PopStyleColor(1);

        //Output files
        for (int i = 0; i < filtered_files.size(); i++)
        {
            if(!filtered_files[i]->is_hidden || show_hidden)
            {
                items++;
                if(ImGui::Selectable(filtered_files[i]->name.c_str(), selected_idx == i && !is_dir, ImGuiSelectableFlags_AllowDoubleClick))
                {
                    //int len = filtered_files[i]->name.length();
                    selected_idx = i;
                    is_dir = false;

                    // If dialog mode is OPEN/SAVE then copy the selected file name to the input text bar
                    strcpy(input_fn, filtered_files[i]->name.c_str());

                    if(ImGui::IsMouseDoubleClicked(0))
                    {
                        selected_fn = filtered_files[i]->name;
                        validate_file = true;
                    }
                }
                if( (items) % col_items_limit == 0)
                    ImGui::NextColumn();
            }
        }
        ImGui::Columns(1);
        ImGui::EndChild();

        return show_error;
    }

    bool ImGuiFileBrowser::renderInputTextAndExtRegion()
    {
        std::string label = (dialog_mode == DialogMode::SAVE) ? "Save As:" : "Open:";
        ImGuiStyle& style = ImGui::GetStyle();
        ImGuiIO& io = ImGui::GetIO();

        ImVec2 pw_pos = ImGui::GetWindowPos();
        ImVec2 pw_content_sz = ImGui::GetWindowSize() - style.WindowPadding * 2.0;
        ImVec2 cursor_pos = ImGui::GetCursorPos();

        if(ext_box_width < 0.0)
            ext_box_width = ImGui::CalcTextSize(".abc").x + 100;
        float label_width = ImGui::CalcTextSize(label.c_str()).x + style.ItemSpacing.x;
        float frame_height_spacing = ImGui::GetFrameHeightWithSpacing();
        float input_bar_width = pw_content_sz.x - label_width;
        if(dialog_mode != DialogMode::SELECT)
            input_bar_width -= (ext_box_width + style.ItemSpacing.x);

        bool show_error = false;
        ImGui::SetCursorPosY(pw_content_sz.y - frame_height_spacing * 2.0f);

        //Render Input Text Bar label
        ImGui::Text("%s", label.c_str());
        ImGui::SameLine();

        //Render Input Text Bar
        input_combobox_pos = ImVec2(pw_pos + ImGui::GetCursorPos());
        input_combobox_sz = ImVec2(input_bar_width, 0);
        ImGui::PushItemWidth(input_bar_width);
        if(ImGui::InputTextWithHint("##FileNameInput", "Type a name...", &input_fn[0], 256, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
        {
            if(strlen(input_fn) > 0)
            {
                selected_fn = std::string(input_fn);
                validate_file = true;
            }
        }
        ImGui::PopItemWidth();

        //If input bar was focused clear selection
        if(ImGui::IsItemEdited())
            selected_idx = -1;

        // If Input Bar is edited show a list of files or dirs matching the input text.
        if(ImGui::IsItemEdited() || ImGui::IsItemActivated())
        {
            //If dialog_mode is OPEN/SAVE then filter from list of files..
            if(dialog_mode == DialogMode::OPEN || dialog_mode == DialogMode::SAVE)
            {
                inputcb_filter_files.clear();
                for(int i = 0; i < subfiles.size(); i++)
                {
                    if(ImStristr(subfiles[i].name.c_str(), nullptr, input_fn, nullptr) != nullptr)
                        inputcb_filter_files.push_back(std::ref(subfiles[i].name));
                }
            }

            //If dialog_mode == SELECT then filter from list of directories
            else if(dialog_mode == DialogMode::SELECT)
            {
                inputcb_filter_files.clear();
                for(int i = 0; i < subdirs.size(); i++)
                {
                    if(ImStristr(subdirs[i].name.c_str(), nullptr, input_fn, nullptr) != nullptr)
                        inputcb_filter_files.push_back(std::ref(subdirs[i].name));
                }
            }

            //If filtered list has any items show dropdown
            if(inputcb_filter_files.size() > 0)
                show_inputbar_combobox = true;
            else
                show_inputbar_combobox = false;
        }

        //Render Extensions and File Types DropDown
        if(dialog_mode != DialogMode::SELECT)
        {
            ImGui::SameLine();
            renderExtBox();
        }

        //Render a Drop Down of files/dirs (depending on mode) that have matching characters as the input text only.
        show_error |= renderInputComboBox();

        ImGui::SetCursorPos(cursor_pos);
        return show_error;
    }

    bool ImGuiFileBrowser::renderButtonsAndCheckboxRegion()
    {
        ImVec2 pw_size = ImGui::GetWindowSize();
        ImGuiStyle& style = ImGui::GetStyle();
        bool show_error = false;
        float frame_height = ImGui::GetFrameHeight();
        float frame_height_spacing = ImGui::GetFrameHeightWithSpacing();
        float opensave_btn_width = getButtonSize("Open").x;     // Since both Open/Save are 4 characters long, width gonna be same.
        float selcan_btn_width = getButtonSize("Cancel").x;     // Since both Cacnel/Select have same number of characters, so same width.
        float buttons_xpos;

        if (dialog_mode == DialogMode::SELECT)
            buttons_xpos = pw_size.x - opensave_btn_width - (2.0f * selcan_btn_width) - ( 2.0f * style.ItemSpacing.x) - style.WindowPadding.x;
        else
            buttons_xpos = pw_size.x - opensave_btn_width - selcan_btn_width - style.ItemSpacing.x - style.WindowPadding.x;

        ImGui::SetCursorPosY(pw_size.y - frame_height_spacing - style.WindowPadding.y);

        //Render Checkbox
        float label_width = ImGui::CalcTextSize("Show Hidden Files and Folders").x + ImGui::GetCursorPosX() + frame_height;
        bool show_marker = (label_width >= buttons_xpos);
        ImGui::Checkbox( (show_marker) ? "##showHiddenFiles" : "Show Hidden Files and Folders", &show_hidden);
        if(show_marker)
        {
            ImGui::SameLine();
            showHelpMarker("Show Hidden Files and Folders");
        }

        //Render an Open Button (in OPEN/SELECT dialog_mode) or Open/Save depending on what's selected in SAVE dialog_mode
        ImGui::SameLine();
        ImGui::SetCursorPosX(buttons_xpos);
        if(dialog_mode == DialogMode::SAVE)
        {
            // If directory selected and Input Text Bar doesn't have focus, render Open Button
            if(selected_idx != -1 && is_dir && ImGui::GetFocusID() != ImGui::GetID("##FileNameInput"))
            {
                if (ImGui::Button("Open"))
                    show_error |= !(onDirClick(selected_idx));
            }
            else if (ImGui::Button("Save") && strlen(input_fn) > 0)
            {
                selected_fn = std::string(input_fn);
                validate_file = true;
            }
        }
        else
        {
            if (ImGui::Button("Open"))
            {
                //It's possible for both to be true at once (user selected directory but input bar has some text. In this case we chose to open the directory instead of opening the file.
                //Also note that we don't need to access the selected file through "selected_idx" since the if a file is selected, input bar will get populated with that name.
                if(selected_idx >= 0 && is_dir)
                    show_error |= !(onDirClick(selected_idx));
                else if(strlen(input_fn) > 0)
                {
                    selected_fn = std::string(input_fn);
                    validate_file = true;
                }
            }

            //Render Select Button if in SELECT Mode
            if(dialog_mode == DialogMode::SELECT)
            {
                //Render Select Button
                ImGui::SameLine();
                if (ImGui::Button("Select"))
                {
                    if(strlen(input_fn) > 0)
                    {
                        selected_fn = std::string(input_fn);
                        validate_file = true;
                    }
                }
            }
        }

        //Render Cancel Button
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            closeDialog();

        return show_error;
    }

    bool ImGuiFileBrowser::renderInputComboBox()
    {
        bool show_error = false;
        ImGuiStyle& style = ImGui::GetStyle();
        ImGuiID input_id =  ImGui::GetID("##FileNameInput");
        ImGuiID focus_scope_id = ImGui::GetID("##InputBarComboBoxListScope");
        float frame_height = ImGui::GetFrameHeight();

        input_combobox_sz.y = std::min((inputcb_filter_files.size() + 1) * frame_height + style.WindowPadding.y *  2.0f,
                                        8 * ImGui::GetFrameHeight() + style.WindowPadding.y *  2.0f);

        if(show_inputbar_combobox && ( ImGui::GetFocusedFocusScope() == focus_scope_id || ImGui::GetCurrentContext()->ActiveIdIsAlive == input_id  ))
        {
            ImGuiWindowFlags popupFlags = ImGuiWindowFlags_NoTitleBar           |
                                          ImGuiWindowFlags_NoResize             |
                                          ImGuiWindowFlags_NoMove               |
                                          ImGuiWindowFlags_NoFocusOnAppearing   |
                                          ImGuiWindowFlags_NoScrollbar          |
                                          ImGuiWindowFlags_NoSavedSettings;


            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.125f, 0.125f, 0.125f, 1.0f));
            ImGui::SetNextWindowBgAlpha(1.0);
            ImGui::SetNextWindowPos(input_combobox_pos + ImVec2(0, ImGui::GetFrameHeightWithSpacing()));
            ImGui::PushClipRect(ImVec2(0,0), ImGui::GetIO().DisplaySize, false);

            ImGui::BeginChild("##InputBarComboBox", input_combobox_sz, true, popupFlags);

            ImVec2 listbox_size = input_combobox_sz - ImGui::GetStyle().WindowPadding * 2.0f;
            if(ImGui::ListBoxHeader("##InputBarComboBoxList", listbox_size))
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f,1.0f));
                ImGui::PushFocusScope(focus_scope_id);
                for(auto& element : inputcb_filter_files)
                {
                    if(ImGui::Selectable(element.get().c_str(), false, ImGuiSelectableFlags_NoHoldingActiveID | ImGuiSelectableFlags_SelectOnClick))
                    {
                        if(element.get().size() > 256)
                        {
                            error_title = "Error!";
                            error_msg = "Selected File Name is longer than 256 characters.";
                            show_error = true;
                        }
                        else
                        {
                            strcpy(input_fn, element.get().c_str());
                            show_inputbar_combobox = false;
                        }
                    }
                }
                ImGui::PopFocusScope();
                ImGui::PopStyleColor(1);
                ImGui::ListBoxFooter();
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PopClipRect();
        }
        return show_error;
    }

    void ImGuiFileBrowser::renderExtBox()
    {
        ImGui::PushItemWidth(ext_box_width);
        if(ImGui::BeginCombo("##FileTypes", valid_exts[selected_ext_idx].c_str()))
        {
            for(int i = 0; i < valid_exts.size(); i++)
            {
                if(ImGui::Selectable(valid_exts[i].c_str(), selected_ext_idx == i))
                {
                    selected_ext_idx = i;
                    if(dialog_mode == DialogMode::SAVE)
                    {
                        std::string name(input_fn);
                        size_t idx = name.find_last_of(".");
                        if(idx == std::string::npos)
                            idx = strlen(input_fn);
                        for(int j = 0; j < valid_exts[selected_ext_idx].size(); j++)
                            input_fn[idx++] = valid_exts[selected_ext_idx][j];
                        input_fn[idx++] = '\0';
                    }
                    filterFiles(FilterMode_Files);
                }
            }
            ImGui::EndCombo();
        }
        ext = valid_exts[selected_ext_idx];
        ImGui::PopItemWidth();
    }

    bool ImGuiFileBrowser::onNavigationButtonClick(int idx)
    {
        std::string new_path = "";

        //First Button corresponds to virtual folder Computer which lists all logical drives (hard disks and removables) and "/" on Unix
        if(idx == 0)
        {
            #ifdef OSWIN
            if(!loadWindowsDrives())
                return false;
            current_path.clear();
            current_dirlist.clear();
            current_dirlist.push_back("Computer");
            return true;
            #else
            new_path = "/";
            #endif // OSWIN
        }
        else
        {
            #ifdef OSWIN
            //Clicked on a drive letter?
            if(idx == 1)
                new_path = current_path.substr(0, 3);
            else
            {
                //Start from i=1 since at 0 lies "MyComputer" which is only virtual and shouldn't be read by readDIR
                for (int i = 1; i <= idx; i++)
                    new_path += current_dirlist[i] + "/";
            }
            #else
            //Since UNIX absolute paths start at "/", we handle this separately to avoid adding a double slash at the beginning
            new_path += current_dirlist[0];
            for (int i = 1; i <= idx; i++)
                new_path += current_dirlist[i] + "/";
            #endif
        }

        if(readDIR(new_path))
        {
            current_dirlist.erase(current_dirlist.begin()+idx+1, current_dirlist.end());
            current_path = new_path;
            return true;
        }
        else
            return false;
    }

    bool ImGuiFileBrowser::onDirClick(int idx)
    {
        std::string name;
        std::string new_path(current_path);
        bool drives_shown = false;

        #ifdef OSWIN
        drives_shown = (current_dirlist.size() == 1 && current_dirlist.back() == "Computer");
        #endif // OSWIN

        name = filtered_dirs[idx]->name;

        if(name == "..")
        {
            new_path.pop_back(); // Remove trailing '/'
            new_path = new_path.substr(0, new_path.find_last_of('/') + 1); // Also include a trailing '/'
        }
        else
        {
            //Remember we displayed drives on Windows as *Local/Removable Disk: X* hence we need last char only
            if(drives_shown)
                name = std::string(1, name.back()) + ":";
            new_path += name + "/";
        }

        if(readDIR(new_path))
        {
            if(name == "..")
                current_dirlist.pop_back();
            else
                current_dirlist.push_back(name);

             current_path = new_path;
             return true;
        }
        else
           return false;
    }

    bool ImGuiFileBrowser::readDIR(std::string pathdir)
    {
        DIR* dir;
        struct dirent *ent;

        /* If the current directory doesn't exist, and we are opening the dialog for the first time, reset to defaults to avoid looping of showing error modal.
         * An example case is when user closes the dialog in a folder. Then deletes the folder outside. On reopening the dialog the current path (previous) would be invalid.
         */
        dir = opendir(pathdir.c_str());
        if(dir == nullptr && is_appearing)
        {
            current_dirlist.clear();
            #ifdef OSWIN
            current_path = pathdir = "./";
            #else
            initCurrentPath();
            pathdir = current_path;
            #endif // OSWIN

            dir = opendir(pathdir.c_str());
        }

        if (dir != nullptr)
        {
            #ifdef OSWIN
            // If we are on Windows and current path is relative then get absolute path from dirent structure
            if(current_dirlist.empty() && pathdir == "./")
            {
                const wchar_t* absolute_path = dir->wdirp->patt;
                std::string current_directory = wStringToString(absolute_path);
                std::replace(current_directory.begin(), current_directory.end(), '\\', '/');

                //Remove trailing "*" returned by ** dir->wdirp->patt **
                current_directory.pop_back();
                current_path = current_directory;

                //Create a vector of each directory in the file path for the filepath bar. Not Necessary for linux as starting directory is "/"
                parsePathTabs(current_path);
            }
            #endif // OSWIN

            // store all the files and directories within directory and clear previous entries
            clearFileList();
            while ((ent = readdir (dir)) != nullptr)
            {
                bool is_hidden = false;
                std::string name(ent->d_name);

                //Ignore current directory
                if(name == ".")
                    continue;

                //Somehow there is a '..' present in root directory in linux.
                #ifndef OSWIN
                if(name == ".." && pathdir == "/")
                    continue;
                #endif // OSWIN

                if(name != "..")
                {
                    #ifdef OSWIN
                    std::string dir = pathdir + std::string(ent->d_name);
                    // IF system file skip it...
                    if (FILE_ATTRIBUTE_SYSTEM & GetFileAttributesA(dir.c_str()))
                        continue;
                    if (FILE_ATTRIBUTE_HIDDEN & GetFileAttributesA(dir.c_str()))
                        is_hidden = true;
                    #else
                    if(name[0] == '.')
                        is_hidden = true;
                    #endif // OSWIN
                }
                //Store directories and files in separate vectors
                if(ent->d_type == DT_DIR)
                    subdirs.push_back(Info(name, is_hidden));
                else if(ent->d_type == DT_REG && dialog_mode != DialogMode::SELECT)
                    subfiles.push_back(Info(name, is_hidden));
            }
            closedir (dir);
            std::sort(subdirs.begin(), subdirs.end(), alphaSortComparator);
            std::sort(subfiles.begin(), subfiles.end(), alphaSortComparator);

            //Initialize Filtered dirs and files
            filterFiles(filter_mode);
        }
        else
        {
            error_title = "Error!";
            error_msg = "Error opening directory! Make sure the directory exists and you have the proper rights to access the directory.";
            return false;
        }
        return true;
    }

    void ImGuiFileBrowser::filterFiles(int filter_mode)
    {
        filter_dirty = false;
        if(filter_mode | FilterMode_Dirs)
        {
            filtered_dirs.clear();
            for (size_t i = 0; i < subdirs.size(); ++i)
            {
                if(filter.PassFilter(subdirs[i].name.c_str()))
                    filtered_dirs.push_back(&subdirs[i]);
            }
        }
        if(filter_mode | FilterMode_Files)
        {
            filtered_files.clear();
            for (size_t i = 0; i < subfiles.size(); ++i)
            {
                if(valid_exts[selected_ext_idx] == "*.*")
                {
                    if(filter.PassFilter(subfiles[i].name.c_str()))
                        filtered_files.push_back(&subfiles[i]);
                }
                else
                {
                    if(filter.PassFilter(subfiles[i].name.c_str()) && (ImStristr(subfiles[i].name.c_str(), nullptr, valid_exts[selected_ext_idx].c_str(), nullptr)) != nullptr)
                        filtered_files.push_back(&subfiles[i]);
                }
            }
        }
    }

    void ImGuiFileBrowser::showHelpMarker(std::string desc)
    {
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    void ImGuiFileBrowser::showErrorModal()
    {
        ImVec2 window_size(260, 0);
        ImGui::SetNextWindowSize(window_size);

        if (ImGui::BeginPopupModal(error_title.c_str(), nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
        {
            ImGui::TextWrapped("%s", error_msg.c_str());

            ImGui::Separator();
            ImGui::SetCursorPosX(window_size.x/2.0f - getButtonSize("OK").x/2.0f);
            if (ImGui::Button("OK", getButtonSize("OK")))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    bool ImGuiFileBrowser::showReplaceFileModal()
    {
        ImVec2 window_size(250, 0);
        ImGui::SetNextWindowSize(window_size);
        bool ret_val = false;
        if (ImGui::BeginPopupModal(repfile_modal_id.c_str(), nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
        {
            float frame_height = ImGui::GetFrameHeightWithSpacing();

            std::string text = "A file with the following filename already exists. Are you sure you want to replace the existing file?";
            ImGui::TextWrapped("%s", text.c_str());

            ImGui::Separator();

            float buttons_width = getButtonSize("Yes").x + getButtonSize("No").x + ImGui::GetStyle().ItemSpacing.x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetWindowWidth()/2.0f - buttons_width/2.0f - ImGui::GetStyle().WindowPadding.x);

            if (ImGui::Button("Yes", getButtonSize("Yes")))
            {
                selected_path = current_path + selected_fn;
                ImGui::CloseCurrentPopup();
                ret_val = true;
            }

            ImGui::SameLine();
            if (ImGui::Button("No", getButtonSize("No")))
            {
                selected_fn.clear();
                selected_path.clear();
                ImGui::CloseCurrentPopup();
                ret_val = false;
            }
            ImGui::EndPopup();
        }
        return ret_val;
    }

    void ImGuiFileBrowser::showInvalidFileModal()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        std::string text = "Selected file either doesn't exist or is not supported. Please select a file with the following extensions...";
        ImVec2 text_size = ImGui::CalcTextSize(text.c_str(), nullptr, true, 350 - style.WindowPadding.x * 2.0f);
        ImVec2 button_size = getButtonSize("OK");

        float frame_height = ImGui::GetFrameHeightWithSpacing();
        float cw_content_height = valid_exts.size() * frame_height;
        float cw_height = std::min(4.0f * frame_height, cw_content_height);
        ImVec2 window_size(350, 0);
        ImGui::SetNextWindowSize(window_size);

        if (ImGui::BeginPopupModal(invfile_modal_id.c_str(), nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
        {

            ImGui::TextWrapped("%s", text.c_str());
            ImGui::BeginChild("##SupportedExts", ImVec2(0, cw_height), true);
            for(int i = 0; i < valid_exts.size(); i++)
                ImGui::BulletText("%s", valid_exts[i].c_str());
            ImGui::EndChild();

            ImGui::SetCursorPosX(window_size.x/2.0f - button_size.x/2.0f);
            if (ImGui::Button("OK", button_size))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    void ImGuiFileBrowser::setValidExtTypes(const std::string& valid_types_string)
    {
        /* Initialize a list of files extensions that are valid.
         * If the user chooses a file that doesn't match the extensions in the
         * list, we will show an error modal...
         */
        std::string max_str = "";
        valid_exts.clear();
        std::string extension = "";
        std::istringstream iss(valid_types_string);
        while(std::getline(iss, extension, ','))
        {
            if(!extension.empty())
            {
                if(max_str.size() < extension.size())
                    max_str = extension;
                valid_exts.push_back(extension);
            }
        }
        float min_width = ImGui::CalcTextSize(".abc").x + 100;
        ext_box_width = std::max(min_width, ImGui::CalcTextSize(max_str.c_str()).x);
    }

    bool ImGuiFileBrowser::validateFile()
    {
        bool match = false;

        //If there is an item selected, check if the selected file name (the input filename, in other words) matches the selection.
        if(selected_idx >= 0)
        {
            if(dialog_mode == DialogMode::SELECT)
                match = (filtered_dirs[selected_idx]->name == selected_fn);
            else
                match = (filtered_files[selected_idx]->name == selected_fn);
        }

        //If the input filename doesn't match we need to explicitly find the input filename..
        if(!match)
        {
            if(dialog_mode == DialogMode::SELECT)
            {
                for(int i = 0; i < subdirs.size(); i++)
                {
                    if(subdirs[i].name == selected_fn)
                    {
                        match = true;
                        break;
                    }
                }

            }
            else
            {
                for(int i = 0; i < subfiles.size(); i++)
                {
                    if(subfiles[i].name == selected_fn)
                    {
                        match = true;
                        break;
                    }
                }
            }
        }

        // If file doesn't match, return true on SAVE mode (since file doesn't exist, hence can be saved directly) and return false on other modes (since file doesn't exist so cant open/select)
        if(!match)
            return (dialog_mode == DialogMode::SAVE);

        // If file matches, return false on SAVE, we need to show a replace file modal
        if(dialog_mode == DialogMode::SAVE)
            return false;
        // Return true on SELECT, no need to validate extensions
        else if(dialog_mode == DialogMode::SELECT)
            return true;
        else
        {
            // If list of extensions has all types, no need to validate.
            for(auto ext : valid_exts)
            {
                if(ext == "*.*")
                    return true;
            }
            size_t idx = selected_fn.find_last_of('.');
            std::string file_ext = idx == std::string::npos ? "" : selected_fn.substr(idx, selected_fn.length() - idx);
            return (std::find(valid_exts.begin(), valid_exts.end(), file_ext) != valid_exts.end());
        }
    }

    ImVec2 ImGuiFileBrowser::getButtonSize(std::string button_text)
    {
        return (ImGui::CalcTextSize(button_text.c_str()) + ImGui::GetStyle().FramePadding * 2.0);
    }

    void ImGuiFileBrowser::parsePathTabs(std::string path)
    {
        std::string path_element = "";
        std::string root = "";

        current_dirlist.clear();

        #ifdef OSWIN
        current_dirlist.push_back("Computer");
        #else
        if(path[0] == '/')
            current_dirlist.push_back("/");
        #endif //OSWIN

        std::istringstream iss(path);
        while(std::getline(iss, path_element, '/'))
        {
            if(!path_element.empty())
                current_dirlist.push_back(path_element);
        }
    }

    std::string ImGuiFileBrowser::wStringToString(const wchar_t* wchar_arr)
    {
        std::mbstate_t state = std::mbstate_t();

         //MinGW bug (patched in mingw-w64), wcsrtombs doesn't ignore length parameter when dest = nullptr. Hence the large number.
        size_t len = 1 + std::wcsrtombs(nullptr, &(wchar_arr), 600000, &state);

        char* char_arr = new char[len];
        std::wcsrtombs(char_arr, &wchar_arr, len, &state);

        std::string ret_val(char_arr);

        delete[] char_arr;
        return ret_val;
    }

    bool ImGuiFileBrowser::alphaSortComparator(const Info& a, const Info& b)
    {
        const char* str1 = a.name.c_str();
        const char* str2 = b.name.c_str();
        int ca, cb;
        do
        {
            ca = (unsigned char) *str1++;
            cb = (unsigned char) *str2++;
            ca = std::tolower(std::toupper(ca));
            cb = std::tolower(std::toupper(cb));
        }
        while (ca == cb && ca != '\0');
        if(ca  < cb)
            return true;
        else
            return false;
    }

    //Windows Exclusive function
    #ifdef OSWIN
    bool ImGuiFileBrowser::loadWindowsDrives()
    {
        DWORD len = GetLogicalDriveStringsA(0,nullptr);
        char* drives = new char[len];
        if(!GetLogicalDriveStringsA(len,drives))
        {
            delete[] drives;
            return false;
        }

        clearFileList();
        char* temp = drives;
        for(char *drv = nullptr; *temp != '\0'; temp++)
        {
            drv = temp;
            if(DRIVE_REMOVABLE == GetDriveTypeA(drv))
                subdirs.push_back({"Removable Disk: " + std::string(1,drv[0]), false});
            else if(DRIVE_FIXED == GetDriveTypeA(drv))
                subdirs.push_back({"Local Disk: " + std::string(1,drv[0]), false});
            //Go to nullptr character
            while(*(++temp));
        }
        delete[] drives;
        return true;
    }
    #endif

    //Unix only
    #ifndef OSWIN
    void ImGuiFileBrowser::initCurrentPath()
    {
        bool path_max_def = false;

        #ifdef PATH_MAX
        path_max_def = true;
        #endif // PATH_MAX

        char* buffer = nullptr;

        //If PATH_MAX is defined deal with memory using new/delete. Else fallback to malloc'ed memory from `realpath()`
        if(path_max_def)
            buffer = new char[PATH_MAX];

        char* real_path = realpath("./", buffer);
        if (real_path == nullptr)
        {
            current_path = "/";
            current_dirlist.push_back("/");
        }
        else
        {
            current_path = std::string(real_path);
            current_path += "/";
            parsePathTabs(current_path);
        }

        if(path_max_def)
            delete[] buffer;
        else
            free(real_path);
    }
    #endif // OSWIN
}
