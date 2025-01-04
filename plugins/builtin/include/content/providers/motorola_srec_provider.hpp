#pragma once

#include <content/providers/intel_hex_provider.hpp>

namespace hex::plugin::builtin {

    class MotorolaSRECProvider : public IntelHexProvider {
    public:
        MotorolaSRECProvider() = default;
        ~MotorolaSRECProvider() override = default;

        bool open() override;
        void close() override;

        [[nodiscard]] std::string getName() const override;
        std::vector<IntelHexProvider::Description> getDataDescription() const override;

        [[nodiscard]] UnlocalizedString getTypeName() const override {
            return "hex.builtin.provider.motorola_srec";
        }

        bool handleFilePicker() override;
    };

}