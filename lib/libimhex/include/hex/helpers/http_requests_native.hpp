#pragma once

#include<string>
#include<future>

#include <curl/curl.h>

namespace hex {

    template<typename T>
    std::future<HttpRequest::Result<T>> HttpRequest::downloadFile(const std::fs::path &path) {
        return std::async(std::launch::async, [this, path] {
            std::vector<u8> response;

            wolv::io::File file(path, wolv::io::File::Mode::Create);
            curl_easy_setopt(this->m_curl, CURLOPT_WRITEFUNCTION, writeToFile);
            curl_easy_setopt(this->m_curl, CURLOPT_WRITEDATA, &file);

            return this->executeImpl<T>(response);
        });
    }

    template<typename T>
    std::future<HttpRequest::Result<T>> HttpRequest::uploadFile(const std::fs::path &path, const std::string &mimeName) {
        return std::async(std::launch::async, [this, path, mimeName]{
            auto fileName = wolv::util::toUTF8String(path.filename());

            curl_mime *mime     = curl_mime_init(this->m_curl);
            curl_mimepart *part = curl_mime_addpart(mime);

            wolv::io::File file(path, wolv::io::File::Mode::Read);

            curl_mime_data_cb(part, file.getSize(),
                                [](char *buffer, size_t size, size_t nitems, void *arg) -> size_t {
                                    auto file = static_cast<FILE*>(arg);

                                    return fread(buffer, size, nitems, file);
                                },
                                [](void *arg, curl_off_t offset, int origin) -> int {
                                    auto file = static_cast<FILE*>(arg);

                                    if (fseek(file, offset, origin) != 0)
                                        return CURL_SEEKFUNC_CANTSEEK;
                                    else
                                        return CURL_SEEKFUNC_OK;
                                },
                                [](void *arg) {
                                    auto file = static_cast<FILE*>(arg);

                                    fclose(file);
                                },
                                file.getHandle());
            curl_mime_filename(part, fileName.c_str());
            curl_mime_name(part, mimeName.c_str());

            curl_easy_setopt(this->m_curl, CURLOPT_MIMEPOST, mime);

            std::vector<u8> responseData;
            curl_easy_setopt(this->m_curl, CURLOPT_WRITEFUNCTION, writeToVector);
            curl_easy_setopt(this->m_curl, CURLOPT_WRITEDATA, &responseData);

            return this->executeImpl<T>(responseData);
        });
    }

    template<typename T>
    std::future<HttpRequest::Result<T>> HttpRequest::uploadFile(std::vector<u8> data, const std::string &mimeName, const std::fs::path &fileName) {
        return std::async(std::launch::async, [this, data = std::move(data), mimeName, fileName]{
            curl_mime *mime     = curl_mime_init(this->m_curl);
            curl_mimepart *part = curl_mime_addpart(mime);

            curl_mime_data(part, reinterpret_cast<const char *>(data.data()), data.size());
            auto fileNameStr = wolv::util::toUTF8String(fileName.filename());
            curl_mime_filename(part, fileNameStr.c_str());
            curl_mime_name(part, mimeName.c_str());

            curl_easy_setopt(this->m_curl, CURLOPT_MIMEPOST, mime);

            std::vector<u8> responseData;
            curl_easy_setopt(this->m_curl, CURLOPT_WRITEFUNCTION, writeToVector);
            curl_easy_setopt(this->m_curl, CURLOPT_WRITEDATA, &responseData);

            return this->executeImpl<T>(responseData);
        });
    }

    template<typename T>
    std::future<HttpRequest::Result<T>> HttpRequest::execute() {
        return std::async(std::launch::async, [this] {

            std::vector<u8> responseData;
            curl_easy_setopt(this->m_curl, CURLOPT_WRITEFUNCTION, writeToVector);
            curl_easy_setopt(this->m_curl, CURLOPT_WRITEDATA, &responseData);

            return this->executeImpl<T>(responseData);
        });
    }

    template<typename T>
    HttpRequest::Result<T> HttpRequest::executeImpl(std::vector<u8> &data) {
        curl_easy_setopt(this->m_curl, CURLOPT_URL, this->m_url.c_str());
        curl_easy_setopt(this->m_curl, CURLOPT_CUSTOMREQUEST, this->m_method.c_str());

        setDefaultConfig();

        if (!this->m_body.empty()) {
            curl_easy_setopt(this->m_curl, CURLOPT_POSTFIELDS, this->m_body.c_str());
        }

        curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "Cache-Control: no-cache");
        ON_SCOPE_EXIT { curl_slist_free_all(headers); };

        for (auto &[key, value] : this->m_headers) {
            std::string header = hex::format("{}: {}", key, value);
            headers = curl_slist_append(headers, header.c_str());
        }
        curl_easy_setopt(this->m_curl, CURLOPT_HTTPHEADER, headers);

        {
            std::scoped_lock lock(this->m_transmissionMutex);

            auto result = curl_easy_perform(this->m_curl);
            if (result != CURLE_OK){
                char *url = nullptr;
                curl_easy_getinfo(this->m_curl, CURLINFO_EFFECTIVE_URL, &url);
                log::error("Http request '{0} {1}' failed with error {2}: '{3}'", this->m_method, url, u32(result), curl_easy_strerror(result));
                checkProxyErrors();

                return { };
            }
        }

        long statusCode = 0;
        curl_easy_getinfo(this->m_curl, CURLINFO_RESPONSE_CODE, &statusCode);

        return Result<T>(statusCode, { data.begin(), data.end() });
    }

}