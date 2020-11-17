#ifndef IMGUIFILEBROWSER_H
#define IMGUIFILEBROWSER_H

#include <imgui.h>
#include <string>
#include <vector>

namespace imgui_addons
{
    class ImGuiFileBrowser
    {
        public:
            ImGuiFileBrowser();
            ~ImGuiFileBrowser();

            enum class DialogMode
            {
                SELECT, //Select Directory Mode
                OPEN,   //Open File mode
                SAVE    //Save File mode.
            };

            /* Use this to show an open file dialog. The function takes label for the window,
             * the size, a DialogMode enum value defining in which mode the dialog should operate and optionally the extensions that are valid for opening.
             * Note that the select directory mode doesn't need any extensions.
             */
            bool showFileDialog(const std::string& label, const DialogMode mode, const ImVec2& sz_xy = ImVec2(0,0), const std::string& valid_types = "*.*");

            /* Store the opened/saved file name or dir name (incase of selectDirectoryDialog) and the absolute path to the selection
             * Should only be accessed when above functions return true else may contain garbage.
             */
            std::string selected_fn;
            std::string selected_path;
            std::string ext;    // Store the saved file extension


        private:
            struct Info
            {
                Info(std::string name, bool is_hidden) : name(name), is_hidden(is_hidden)
                {
                }
                std::string name;
                bool is_hidden;
            };

            //Enum used as bit flags.
            enum FilterMode
            {
                FilterMode_Files = 0x01,
                FilterMode_Dirs = 0x02
            };

            //Helper Functions
            static std::string wStringToString(const wchar_t* wchar_arr);
            static bool alphaSortComparator(const Info& a, const Info& b);
            ImVec2 getButtonSize(std::string button_text);

            /* Helper Functions that render secondary modals
             * and help in validating file extensions and for filtering, parsing top navigation bar.
             */
            void setValidExtTypes(const std::string& valid_types_string);
            bool validateFile();
            void showErrorModal();
            void showInvalidFileModal();
            bool showReplaceFileModal();
            void showHelpMarker(std::string desc);
            void parsePathTabs(std::string str);
            void filterFiles(int filter_mode);

            /* Core Functions that render the 4 different regions making up
             * a simple file dialog
             */
            bool renderNavAndSearchBarRegion();
            bool renderFileListRegion();
            bool renderInputTextAndExtRegion();
            bool renderButtonsAndCheckboxRegion();
            bool renderInputComboBox();
            void renderExtBox();

            /* Core Functions that handle navigation and
             * reading directories/files
             */
            bool readDIR(std::string path);
            bool onNavigationButtonClick(int idx);
            bool onDirClick(int idx);

            // Functions that reset state and/or clear file list when reading new directory
            void clearFileList();
            void closeDialog();

            #if defined (WIN32) || defined (_WIN32) || defined (__WIN32)
            bool loadWindowsDrives(); // Helper Function for Windows to load Drive Letters.
            #endif

            #if defined(unix) || defined(__unix__) || defined(__unix) || defined(__APPLE__)
            void initCurrentPath();   // Helper function for UNIX based system to load Absolute path using realpath
            #endif

            ImVec2 min_size, max_size, input_combobox_pos, input_combobox_sz;
            DialogMode dialog_mode;
            int filter_mode, col_items_limit, selected_idx, selected_ext_idx;
            float col_width, ext_box_width;
            bool show_hidden, show_inputbar_combobox, is_dir, is_appearing, filter_dirty, validate_file, path_input_enabled;
            char input_fn[256];
            char temp_dir_input[256];

            std::vector<std::string> valid_exts;
            std::vector<std::string> current_dirlist;
            std::vector<Info> subdirs;
            std::vector<Info> subfiles;
            std::string current_path, error_msg, error_title, invfile_modal_id, repfile_modal_id;

            ImGuiTextFilter filter;
            std::string valid_types;
            std::vector<const Info*> filtered_dirs; // Note: We don't need to call delete. It's just for storing filtered items from subdirs and subfiles so we don't use PassFilter every frame.
            std::vector<const Info*> filtered_files;
            std::vector< std::reference_wrapper<std::string> > inputcb_filter_files;
    };
}


#endif // IMGUIFILEBROWSER_H
