#pragma once

#include <hex.hpp>

#include <hex/api/localization_manager.hpp>

#include <string>
#include <list>
#include <variant>

#include <hex/ui/imgui_imhex_extensions.h>

namespace hex {

    class TutorialManager {
    public:
        enum class Position : u8 {
            None    = 0,
            Top     = 1,
            Bottom  = 2,
            Left    = 4,
            Right   = 8
        };

        struct Tutorial {
            Tutorial() = delete;
            Tutorial(const std::string &unlocalizedName, const std::string &unlocalizedDescription) :
                m_unlocalizedName(unlocalizedName),
                m_unlocalizedDescription(unlocalizedDescription) { }

            struct Step {
                explicit Step(Tutorial *parent) : m_parent(parent) { }

                /**
                 * @brief Adds a highlighting with text to a specific element
                 * @param unlocalizedText Unlocalized text to display next to the highlighting
                 * @param ids ID of the element to highlight
                 * @return Current step
                 */
                Step& addHighlight(const std::string &unlocalizedText, std::initializer_list<std::variant<Lang, std::string, int>> &&ids);

                /**
                 * @brief Adds a highlighting to a specific element
                 * @param ids ID of the element to highlight
                 * @return Current step
                 */
                Step& addHighlight(std::initializer_list<std::variant<Lang, std::string, int>> &&ids);

                /**
                 * @brief Sets the text that will be displayed in the tutorial message box
                 * @param unlocalizedTitle Title of the message box
                 * @param unlocalizedMessage Main message of the message box
                 * @param position Position of the message box
                 * @return Current step
                 */
                Step& setMessage(const std::string &unlocalizedTitle, const std::string &unlocalizedMessage, Position position = Position::None);

                /**
                 * @brief Allows this step to be skipped by clicking on the advance button
                 * @return Current step
                 */
                Step& allowSkip();


                /**
                 * @brief Checks if this step is the current step
                 * @return True if this step is the current step
                 */
                bool isCurrent() const;

                /**
                 * @brief Completes this step if it is the current step
                 */
                void complete() const;

            private:
                struct Highlight {
                    std::string unlocalizedText;
                    ImGuiID highlightId;
                };

                struct Message {
                    Position position;
                    std::string unlocalizedTitle;
                    std::string unlocalizedMessage;
                    bool allowSkip;
                };

            private:
                void addHighlights() const;
                void removeHighlights() const;

                void advance(i32 steps = 1) const;

                friend class TutorialManager;

                Tutorial *m_parent;
                std::vector<Highlight> m_highlights;
                std::optional<Message> m_message;
            };

            Step& addStep();

        private:
            friend class TutorialManager;

            void start();

            std::string m_unlocalizedName;
            std::string m_unlocalizedDescription;
            std::list<Step> m_steps;
            decltype(m_steps)::iterator m_currentStep, m_latestStep;
        };


        /**
         * @brief Creates a new tutorial that can be started later
         * @param unlocalizedName Name of the tutorial
         * @param unlocalizedDescription
         * @return Reference to created tutorial
         */
        static Tutorial& createTutorial(const std::string &unlocalizedName, const std::string &unlocalizedDescription);

        /**
         * @brief Starts the tutorial with the given name
         * @param unlocalizedName Name of tutorial to start
         */
        static void startTutorial(const std::string &unlocalizedName);


        /**
         * @brief Draws the tutorial
         * @note This function should only be called by the main GUI
         */
        static void drawTutorial();

        /**
         * @brief Resets the tutorial manager
         */
        static void reset();

    private:
        TutorialManager() = delete;

        static void drawHighlights();
        static void drawMessageBox(std::optional<Tutorial::Step::Message> message);
    };

    inline TutorialManager::Position operator|(TutorialManager::Position a, TutorialManager::Position b) {
        return static_cast<TutorialManager::Position>(static_cast<u8>(a) | static_cast<u8>(b));
    }

    inline TutorialManager::Position operator&(TutorialManager::Position a, TutorialManager::Position b) {
        return static_cast<TutorialManager::Position>(static_cast<u8>(a) & static_cast<u8>(b));
    }

}