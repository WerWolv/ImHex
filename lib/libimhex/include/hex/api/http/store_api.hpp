#pragma once

#include <hex.hpp>

#include <future>
#include <map>
#include <string>
#include <utility>
#include <vector>

EXPORT_MODULE namespace hex {

    class StoreApi {
    public:
        struct Entry {
            std::string name;
            std::string description;
            std::vector<std::string> authors;
            std::string fileName;
            std::string link;
            std::string hash;
            bool isFolder;
        };

        using Categories = std::map<std::string, std::vector<Entry>>;
        using Pragmas = std::map<std::string, std::vector<std::string>>;

        struct Store {
            Categories categories;
            Pragmas pragmas;
        };

        class Result {
        public:
            enum class Status {
                Success,
                NetworkError,
                InvalidResponse
            };

            explicit Result(Status status, Store data = { }, std::string errorMessage = { }) :
                m_status(status), m_data(std::move(data)), m_errorMessage(std::move(errorMessage)) { }

            [[nodiscard]] bool isSuccess() const { return m_status == Status::Success; }
            [[nodiscard]] Status getStatus() const { return m_status; }
            [[nodiscard]] const Store& getData() const { return m_data; }
            [[nodiscard]] const std::string& getErrorMessage() const { return m_errorMessage; }

        private:
            Status m_status;
            Store m_data;
            std::string m_errorMessage;
        };

        using Request = std::shared_future<Result>;

        /**
         * Gets the store contents. Repeated calls return the cached request and result.
         */
        static Request get();

        /**
         * Refreshes the store contents. If a request is already running, it is reused.
         */
        static Request refresh();

    private:
        StoreApi() = delete;
    };

}
