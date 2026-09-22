#pragma once

#include <hex/providers/provider.hpp>

#include <initializer_list>
#include <vector>

namespace hex::prv {

    /**
     * A non-owning, read-only view that concatenates regions from other providers.
     * The source providers must outlive this provider.
     */
    class ConcatenatedProvider : public Provider {
    public:
        struct Segment {
            Provider *provider;
            Region region;
        };

        ConcatenatedProvider() = default;
        explicit ConcatenatedProvider(std::vector<Segment> segments);
        ConcatenatedProvider(std::initializer_list<Segment> segments);

        void add(Provider *provider, const Region &region);
        void add(Provider *provider);

        [[nodiscard]] bool isAvailable() const override;
        [[nodiscard]] bool isReadable() const override;
        [[nodiscard]] bool isWritable() const override { return false; }
        [[nodiscard]] bool isResizable() const override { return false; }
        [[nodiscard]] bool isSavable() const override { return false; }
        [[nodiscard]] bool isSavableAsRecent() const override { return false; }

        [[nodiscard]] OpenResult open() override { return {}; }
        void close() override { }

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64, const void*, size_t) override { }
        [[nodiscard]] u64 getActualSize() const override { return m_size; }

        [[nodiscard]] std::string getName() const override { return "ConcatenatedProvider"; }
        [[nodiscard]] UnlocalizedString getTypeName() const override { return "ConcatenatedProvider"_untranslated; }
        [[nodiscard]] const char *getIcon() const override { return ""; }

    private:
        std::vector<Segment> m_segments;
        u64 m_size = 0;
    };

}
