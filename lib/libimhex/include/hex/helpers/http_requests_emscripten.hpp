#pragma once

#if defined(OS_WEB)

    #include <future>

    #include <emscripten/fetch.h>

    namespace hex {
        template<typename T>
        std::future<HttpRequest::Result<T>> HttpRequest::downloadFile(const std::fs::path &path) {
            return std::async(std::launch::async, [this, path] {
                std::vector<u8> response;

                // Execute the request
                auto result = this->executeImpl<T>(response);

                // Write the result to the file
                wolv::io::File file(path, wolv::io::File::Mode::Create);
                file.writeBuffer(reinterpret_cast<const u8*>(result.getData().data()), result.getData().size());

                return result;
            });
        }

        template<typename T>
        std::future<HttpRequest::Result<T>> HttpRequest::uploadFile(const std::fs::path &path, const std::string &mimeName) {
            std::ignore = path;
            std::ignore = mimeName;
            throw std::logic_error("Not implemented");
        }

        template<typename T>
        std::future<HttpRequest::Result<T>> HttpRequest::uploadFile(std::vector<u8> data, const std::string &mimeName, const std::fs::path &fileName) {
            std::ignore = data;
            std::ignore = mimeName;
            std::ignore = fileName;
            throw std::logic_error("Not implemented");
        }

        template<typename T>
        std::future<HttpRequest::Result<T>> HttpRequest::execute() {
            return std::async(std::launch::async, [this] {
                std::vector<u8> responseData;

                return this->executeImpl<T>(responseData);
            });
        }

        template<typename T>
        HttpRequest::Result<T> HttpRequest::executeImpl(std::vector<u8> &data) {
            strcpy(m_attr.requestMethod, m_method.c_str());
            m_attr.attributes = EMSCRIPTEN_FETCH_SYNCHRONOUS | EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

            if (!m_body.empty()) {
                m_attr.requestData = m_body.c_str();
                m_attr.requestDataSize = m_body.size();
            }

            std::vector<const char*> headers;
            for (auto it = m_headers.begin(); it != m_headers.end(); it++) {
                headers.push_back(it->first.c_str());
                headers.push_back(it->second.c_str());
            }
            headers.push_back(nullptr);
            m_attr.requestHeaders = headers.data();

            // Send request
            emscripten_fetch_t* fetch = emscripten_fetch(&m_attr, m_url.c_str());

            data.resize(fetch->numBytes);
            std::copy(fetch->data, fetch->data + fetch->numBytes, data.begin());

            return Result<T>(fetch->status, { data.begin(), data.end() });
        }

    }

#endif