#pragma once

#include <content/providers/intel_hex_provider.hpp>

namespace hex::plugin::builtin::prv {

    class MotorolaSRECProvider : public IntelHexProvider {
    public:
        MotorolaSRECProvider() = default;
        ~MotorolaSRECProvider() override = default;

        bool open() override;
        void close() override;

        [[nodiscard]] std::string getName() const override;

        [[nodiscard]] std::string getTypeName() const override {
            return "hex.builtin.provider.motorola_srec";
        }

        bool handleFilePicker() override;

    private:

    };

}