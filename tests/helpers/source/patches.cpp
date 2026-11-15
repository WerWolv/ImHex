#include <hex/test/tests.hpp>
#include <hex/test/test_provider.hpp>

#include <hex/helpers/patches.hpp>
#include <hex/providers/undo_redo/operations/operation.hpp>

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

    // A multi-byte write, as recorded by pasting, filling or editing a value in the data inspector
    class WriteOperation : public hex::prv::undo::Operation {
    public:
        WriteOperation(u64 offset, std::vector<u8> data) : m_offset(offset), m_data(std::move(data)) { }

        void undo(hex::prv::Provider *) override { }
        void redo(hex::prv::Provider *provider) override {
            provider->writeRaw(m_offset, m_data.data(), m_data.size());
        }

        [[nodiscard]] hex::Region getRegion() const override { return { m_offset, m_data.size() }; }
        [[nodiscard]] std::string format() const override { return "write"; }

        [[nodiscard]] std::unique_ptr<Operation> clone() const override {
            return std::make_unique<WriteOperation>(*this);
        }

    private:
        u64 m_offset;
        std::vector<u8> m_data;
    };

    std::vector<u8> bytesOf(std::string_view text) {
        return { text.begin(), text.end() };
    }

}

TEST_SEQUENCE("PatchesFromProviderKeepsMultiByteWrites") {
    std::vector<u8> data(16, 0x00);
    hex::test::TestProvider provider(&data);

    provider.getUndoStack().add<WriteOperation>(4, std::vector<u8>{ 0xAA, 0xBB, 0xCC });

    auto patches = hex::Patches::fromProvider(&provider);
    TEST_ASSERT(patches.has_value());
    TEST_ASSERT(patches->get() == (std::map<u64, u8>{ { 4, 0xAA }, { 5, 0xBB }, { 6, 0xCC } }));

    TEST_SUCCESS();
};

TEST_SEQUENCE("IPSPatchRLERecord") {
    // Offset 0x000010, size 0 (RLE), run length 0x0003, value 0x7F
    auto ips = bytesOf("PATCH");
    ips.insert(ips.end(), { 0x00, 0x00, 0x10,  0x00, 0x00,  0x00, 0x03,  0x7F });
    std::ranges::copy(bytesOf("EOF"), std::back_inserter(ips));

    auto patches = hex::Patches::fromIPSPatch(ips);
    TEST_ASSERT(patches.has_value());
    TEST_ASSERT(patches->get() == (std::map<u64, u8>{ { 0x10, 0x7F }, { 0x11, 0x7F }, { 0x12, 0x7F } }));

    TEST_SUCCESS();
};

TEST_SEQUENCE("IPS32PatchRLERecord") {
    // Offset 0x01000010, size 0 (RLE), run length 0x0003, value 0x7F
    auto ips = bytesOf("IPS32");
    ips.insert(ips.end(), { 0x01, 0x00, 0x00, 0x10,  0x00, 0x00,  0x00, 0x03,  0x7F });
    std::ranges::copy(bytesOf("EEOF"), std::back_inserter(ips));

    auto patches = hex::Patches::fromIPS32Patch(ips);
    TEST_ASSERT(patches.has_value());
    TEST_ASSERT(patches->get() == (std::map<u64, u8>{ { 0x0100'0010, 0x7F }, { 0x0100'0011, 0x7F }, { 0x0100'0012, 0x7F } }));

    TEST_SUCCESS();
};

TEST_SEQUENCE("IPSPatchRejectsAddressesAbove24Bits") {
    // IPS records only have room for a 24 bit offset
    hex::Patches patches(std::map<u64, u8>{ { 0x0100'0000, 0x22 } });

    auto ips = patches.toIPSPatch();
    TEST_ASSERT(!ips.has_value());
    TEST_ASSERT(ips.error() == hex::IPSError::AddressOutOfRange);

    hex::Patches inRange(std::map<u64, u8>{ { 0x00FF'FFFF, 0x11 } });
    auto roundTrip = hex::Patches::fromIPSPatch(inRange.toIPSPatch().value());
    TEST_ASSERT(roundTrip.has_value());
    TEST_ASSERT(roundTrip->get() == inRange.get());

    TEST_SUCCESS();
};
