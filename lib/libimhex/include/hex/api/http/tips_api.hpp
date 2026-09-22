#pragma once

#include <hex.hpp>

#include <future>
#include <string>
#include <utility>

EXPORT_MODULE namespace hex {

    class TipsApi {
    public:
        class Result {
        public:
            enum class Status {
                Success,
                NetworkError
            };

            explicit Result(Status status, std::string data = { }, std::string errorMessage = { }) :
                m_status(status), m_data(std::move(data)), m_errorMessage(std::move(errorMessage)) { }

            [[nodiscard]] bool isSuccess() const { return m_status == Status::Success; }
            [[nodiscard]] Status getStatus() const { return m_status; }
            [[nodiscard]] const std::string& getData() const { return m_data; }
            [[nodiscard]] const std::string& getErrorMessage() const { return m_errorMessage; }

        private:
            Status m_status;
            std::string m_data;
            std::string m_errorMessage;
        };

        using Request = std::shared_future<Result>;

        static Request get();
        static Request refresh();

    private:
        TipsApi() = delete;
    };

}
