namespace hex::plugin::builtin {

    /*void drawFileUploader() {
        struct UploadedFile {
            std::string fileName, link, size;
        };

        static HttpRequest request("POST", "https://api.anonfiles.com/upload");
        static std::future<HttpRequest::Result<std::string>> uploadProcess;
        static std::fs::path currFile;
        static std::vector<UploadedFile> links;

        bool uploading = uploadProcess.valid() && uploadProcess.wait_for(0s) != std::future_status::ready;

        ImGuiExt::Header("hex.builtin.tools.file_uploader.control"_lang, true);
        if (!uploading) {
            if (ImGui::Button("hex.builtin.tools.file_uploader.upload"_lang)) {
                fs::openFileBrowser(fs::DialogMode::Open, {}, [&](auto path) {
                    uploadProcess = request.uploadFile(path);
                    currFile      = path;
                });
            }
        } else {
            if (ImGui::Button("hex.ui.common.cancel"_lang)) {
                request.cancel();
            }
        }

        ImGui::SameLine();

        ImGui::ProgressBar(request.getProgress(), ImVec2(0, 0), uploading ? nullptr : "Done!");

        ImGuiExt::Header("hex.builtin.tools.file_uploader.recent"_lang);

        if (ImGui::BeginTable("##links", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg, ImVec2(0, 200))) {
            ImGui::TableSetupColumn("hex.ui.common.file"_lang);
            ImGui::TableSetupColumn("hex.ui.common.link"_lang);
            ImGui::TableSetupColumn("hex.ui.common.size"_lang);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin(links.size());

            while (clipper.Step()) {
                for (i32 i = clipper.DisplayEnd - 1; i >= clipper.DisplayStart; i--) {
                    auto &[fileName, link, size] = links[i];

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(fileName.c_str());

                    ImGui::TableNextColumn();
                    if (ImGuiExt::Hyperlink(link.c_str())) {
                        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
                            hex::openWebpage(link);
                        else
                            ImGui::SetClipboardText(link.c_str());
                    }

                    ImGuiExt::InfoTooltip("hex.builtin.tools.file_uploader.tooltip"_lang);

                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(size.c_str());
                }
            }

            clipper.End();

            ImGui::EndTable();
        }

        if (uploadProcess.valid() && uploadProcess.wait_for(0s) == std::future_status::ready) {
            auto response = uploadProcess.get();
            if (response.getStatusCode() == 200) {
                try {
                    auto json = nlohmann::json::parse(response.getData());
                    links.push_back({
                        wolv::util::toUTF8String(currFile.filename()),
                        json.at("data").at("file").at("url").at("short"),
                        json.at("data").at("file").at("metadata").at("size").at("readable")
                    });
                } catch (...) {
                    ui::PopupError::open("hex.builtin.tools.file_uploader.invalid_response"_lang);
                }
            } else if (response.getStatusCode() == 0) {
                // Canceled by user, no action needed
            } else ui::PopupError::open(hex::format("hex.builtin.tools.file_uploader.error"_lang, response.getStatusCode()));

            uploadProcess = {};
            currFile.clear();
        }
    }*/

}
