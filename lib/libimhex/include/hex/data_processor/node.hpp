#pragma once

#include <hex.hpp>
#include <hex/api/localization_manager.hpp>

#include <hex/data_processor/attribute.hpp>

#include <set>
#include <span>
#include <vector>

#include <nlohmann/json_fwd.hpp>
#include <imgui.h>
#include <hex/providers/provider_data.hpp>

namespace hex::prv {
    class Provider;
    class Overlay;
}

namespace hex::dp {

    class Node {
    public:
        Node(UnlocalizedString unlocalizedTitle, std::vector<Attribute> attributes);

        virtual ~Node() = default;

        [[nodiscard]] int getId() const { return m_id; }
        void setId(int id) { m_id = id; }

        [[nodiscard]] const UnlocalizedString &getUnlocalizedName() const { return m_unlocalizedName; }
        void setUnlocalizedName(const UnlocalizedString &unlocalizedName) { m_unlocalizedName = unlocalizedName; }

        [[nodiscard]] const UnlocalizedString &getUnlocalizedTitle() const { return m_unlocalizedTitle; }
        void setUnlocalizedTitle(std::string title) { m_unlocalizedTitle = std::move(title); }

        [[nodiscard]] std::vector<Attribute> &getAttributes() { return m_attributes; }
        [[nodiscard]] const std::vector<Attribute> &getAttributes() const { return m_attributes; }

        void setCurrentOverlay(prv::Overlay *overlay) {
            m_overlay = overlay;
        }

        void draw();
        virtual void process() = 0;
        virtual void reset() { }

        virtual void store(nlohmann::json &j) const { std::ignore = j; }
        virtual void load(const nlohmann::json &j) { std::ignore = j; }

        struct NodeError {
            Node *node;
            std::string message;
        };

        void resetOutputData() {
            for (auto &attribute : m_attributes)
                attribute.clearOutputData();
        }

        void resetProcessedInputs() {
            m_processedInputs.clear();
        }

        void setPosition(ImVec2 pos) {
            m_position = pos;
        }

        [[nodiscard]] ImVec2 getPosition() const {
            return m_position;
        }

        static void setIdCounter(int id);

        const std::vector<u8>& getBufferOnInput(u32 index);
        const i128& getIntegerOnInput(u32 index);
        const double& getFloatOnInput(u32 index);

        void setBufferOnOutput(u32 index, std::span<const u8> data);
        void setIntegerOnOutput(u32 index, i128 integer);
        void setFloatOnOutput(u32 index, double floatingPoint);

        static void interrupt();

    protected:
        virtual void drawNode() { }

    private:
        int m_id;
        UnlocalizedString m_unlocalizedTitle, m_unlocalizedName;
        std::vector<Attribute> m_attributes;
        std::set<u32> m_processedInputs;
        prv::Overlay *m_overlay = nullptr;
        ImVec2 m_position;

        static int s_idCounter;

        Attribute& getAttribute(u32 index);
        Attribute *getConnectedInputAttribute(u32 index);
        void markInputProcessed(u32 index);
        void unmarkInputProcessed(u32 index);

    protected:
        [[noreturn]] void throwNodeError(const std::string &message);

        void setOverlayData(u64 address, const std::vector<u8> &data);
        void setAttributes(std::vector<Attribute> attributes);
    };

}