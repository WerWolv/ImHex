#pragma once

#if !defined(OS_WEB)

    #include <string>
    #include <future>
    #include <mutex>

    #include <hex/helpers/logger.hpp>
    #include <hex/helpers/fmt.hpp>

    #include <wolv/utils/string.hpp>

    namespace hex {

        namespace impl {

            void setWriteFunctions(CURL *curl, wolv::io::File &file);
            void setWriteFunctions(CURL *curl, std::vector<u8> &data);
            void setupFileUpload(CURL *curl, wolv::io::File &file, const std::string &fileName, const std::string &mimeName);
            void setupFileUpload(CURL *curl, const std::vector<u8> &data, const std::fs::path &fileName, const std::string &mimeName);
            int executeCurl(CURL *curl, const std::string &url, const std::string &method, const std::string &body, std::map<std::string, std::string> &headers);
            long getStatusCode(CURL *curl);
            std::string getStatusText(int result);

        }


        template<typename T>
        std::future<HttpRequest::Result<T>> HttpRequest::downloadFile(const std::fs::path &path) {
            return std::async(std::launch::async, [this, path] {
                std::vector<u8> response;

                wolv::io::File file(path, wolv::io::File::Mode::Create);
                impl::setWriteFunctions(m_curl, file);

                return this->executeImpl<T>(response);
            });
        }

        template<typename T>
        std::future<HttpRequest::Result<T>> HttpRequest::uploadFile(const std::fs::path &path, const std::string &mimeName) {
            return std::async(std::launch::async, [this, path, mimeName]{
                auto fileName = wolv::util::toUTF8String(path.filename());
                wolv::io::File file(path, wolv::io::File::Mode::Read);

                impl::setupFileUpload(m_curl, file, fileName, mimeName);

                std::vector<u8> responseData;
                impl::setWriteFunctions(m_curl, responseData);

                return this->executeImpl<T>(responseData);
            });
        }

        template<typename T>
        std::future<HttpRequest::Result<T>> HttpRequest::uploadFile(std::vector<u8> data, const std::string &mimeName, const std::fs::path &fileName) {
            return std::async(std::launch::async, [this, data = std::move(data), mimeName, fileName]{
                impl::setupFileUpload(m_curl, data, fileName, mimeName);

                std::vector<u8> responseData;
                impl::setWriteFunctions(m_curl, responseData);

                return this->executeImpl<T>(responseData);
            });
        }

        template<typename T>
        std::future<HttpRequest::Result<T>> HttpRequest::execute() {
            return std::async(std::launch::async, [this] {

                std::vector<u8> responseData;
                impl::setWriteFunctions(m_curl, responseData);

                return this->executeImpl<T>(responseData);
            });
        }

        template<typename T>
        HttpRequest::Result<T> HttpRequest::executeImpl(std::vector<u8> &data) {
            setDefaultConfig();

            std::scoped_lock lock(m_transmissionMutex);

            if (auto result = impl::executeCurl(m_curl, m_url, m_method, m_body, m_headers); result != 0) {
                log::error("Http request '{0} {1}' failed with error {2}: '{3}'", m_method, m_url, u32(result), impl::getStatusText(result));
                checkProxyErrors();
            }

            return Result<T>(impl::getStatusCode(m_curl), { data.begin(), data.end() });
        }

    }

#endif