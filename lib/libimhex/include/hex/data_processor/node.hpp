#pragma once
#include <hex.hpp>

#include <hex/helpers/intrinsics.hpp>
#include <hex/data_processor/attribute.hpp>

#include <set>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>
#include <imgui.h>

namespace hex::prv {
    class Provider;
    class Overlay;
}

namespace hex::dp {

    class Node {
    public:
        Node(std::string unlocalizedTitle, std::vector<Attribute> attributes);

        virtual ~Node() = default;

        [[nodiscard]] int getId() const { return this->m_id; }
        void setId(int id) { this->m_id = id; }

        [[nodiscard]] const std::string &getUnlocalizedName() const { return this->m_unlocalizedName; }
        void setUnlocalizedName(const std::string &unlocalizedName) { this->m_unlocalizedName = unlocalizedName; }

        [[nodiscard]] const std::string &getUnlocalizedTitle() const { return this->m_unlocalizedTitle; }
        void setUnlocalizedTitle(std::string title) { this->m_unlocalizedTitle = std::move(title); }

        [[nodiscard]] std::vector<Attribute> &getAttributes() { return this->m_attributes; }
        [[nodiscard]] const std::vector<Attribute> &getAttributes() const { return this->m_attributes; }

        void setCurrentOverlay(prv::Overlay *overlay) {
            this->m_overlay = overlay;
        }

        virtual void drawNode() { }
        virtual void process() = 0;

        virtual void store(nlohmann::json &j) const { hex::unused(j); }
        virtual void load(const nlohmann::json &j) { hex::unused(j); }

        struct NodeError {
            Node *node;
            std::string message;
        };

        void resetOutputData() {
            for (auto &attribute : this->m_attributes)
                attribute.clearOutputData();
        }

        void resetProcessedInputs() {
            this->m_processedInputs.clear();
        }

        void setPosition(ImVec2 pos) {
            this->m_position = pos;
        }

        [[nodiscard]] ImVec2 getPosition() const {
            return this->m_position;
        }

        static void setIdCounter(int id) {
            if (id > Node::s_idCounter)
                Node::s_idCounter = id;
        }

        const std::vector<u8>& getBufferOnInput(u32 index);
        const i128& getIntegerOnInput(u32 index);
        const long double& getFloatOnInput(u32 index);

        void setBufferOnOutput(u32 index, const std::vector<u8> &data);
        void setIntegerOnOutput(u32 index, i128 integer);
        void setFloatOnOutput(u32 index, long double floatingPoint);

    private:
        int m_id;
        std::string m_unlocalizedTitle, m_unlocalizedName;
        std::vector<Attribute> m_attributes;
        std::set<u32> m_processedInputs;
        prv::Overlay *m_overlay = nullptr;
        ImVec2 m_position;

        static int s_idCounter;

        Attribute& getAttribute(u32 index) {
            if (index >= this->getAttributes().size())
                throw std::runtime_error("Attribute index out of bounds!");

            return this->getAttributes()[index];
        }

        Attribute *getConnectedInputAttribute(u32 index) {
            const auto &connectedAttribute = this->getAttribute(index).getConnectedAttributes();

            if (connectedAttribute.empty())
                return nullptr;

            return connectedAttribute.begin()->second;
        }

        void markInputProcessed(u32 index) {
            const auto &[iter, inserted] = this->m_processedInputs.insert(index);
            if (!inserted)
                throwNodeError("Recursion detected!");
        }

    protected:
        [[noreturn]] void throwNodeError(const std::string &message) {
            throw NodeError { this, message };
        }

        void setOverlayData(u64 address, const std::vector<u8> &data);

        void setAttributes(std::vector<Attribute> attributes) {
            this->m_attributes = std::move(attributes);

            for (auto &attr : this->m_attributes)
                attr.setParentNode(this);
        }
    };

}