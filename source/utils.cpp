#include "utils.hpp"

namespace hex {

    std::optional<std::string> openFileDialog() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (SUCCEEDED(hr)) {
            IFileOpenDialog *pFileOpen;

            hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

            if (SUCCEEDED(hr)) {
                hr = pFileOpen->Show(nullptr);

                if (SUCCEEDED(hr)) {
                    IShellItem *pItem;
                    hr = pFileOpen->GetResult(&pItem);

                    if (SUCCEEDED(hr)) {
                        PWSTR pszFilePath;
                        hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

                        if (SUCCEEDED(hr)) {
                            std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;

                            std::string result = converter.to_bytes(pszFilePath);

                            CoTaskMemFree(pszFilePath);

                            return result;
                        }
                        pItem->Release();
                    }
                }
                pFileOpen->Release();
            }
            CoUninitialize();
        }

        return { };
    }

}