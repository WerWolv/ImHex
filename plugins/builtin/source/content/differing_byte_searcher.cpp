#include <content/differing_byte_searcher.hpp>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/imhex_api/hex_editor.hpp>

namespace hex::plugin::builtin {

    void findNextDifferingByte(
        const std::function< u64(prv::Provider*) >& lastValidAddressProvider,
        const std::function< bool(u64, u64) >& addressComparator,
        const std::function< void(u64*) >& addressStepper,
        bool *didFindNextValue,
        bool *didReachEndAddress,
        u64* foundAddress
    ) {
        auto provider = ImHexApi::Provider::get();
        if (provider == nullptr)
            return;
        const auto selection  = ImHexApi::HexEditor::getSelection();
        if (!selection.has_value())
            return;
        if (selection->getSize() != 1)
            return;

        auto currentAddress = selection->getStartAddress();

        u8 givenValue = 0;
        provider->read(currentAddress, &givenValue, 1);

        u8 currentValue = 0;

        *didFindNextValue = false;
        *didReachEndAddress = false;

        auto endAddress = lastValidAddressProvider(provider);

        while (addressComparator(currentAddress, endAddress)) {
            addressStepper(&currentAddress);
            if (currentAddress == endAddress) {
                *didReachEndAddress = true;
            }
            provider->read(currentAddress, &currentValue, 1);
            if (currentValue != givenValue) {
                *didFindNextValue = true;
                *foundAddress = currentAddress;
                break;
            }
        }
    }

    bool canSearchForDifferingByte() {
        return ImHexApi::Provider::isValid() && ImHexApi::HexEditor::isSelectionValid() && ImHexApi::HexEditor::getSelection()->getSize() == 1;
    }
}