#pragma once

#include <hex/helpers/encoding_file.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace hex::plugin::builtin {

    /**
     * @brief What the encoding chooser says about one table
     */
    struct EncodingChoice {
        std::string name;
        std::string description;

        // The file names of the tables that are only another name for this one.
        std::vector<std::string> aliases;
    };

    /**
     * @brief What the chooser lists, and what it says about each entry
     */
    struct EncodingChoices {
        std::vector<std::fs::path> files;
        std::map<std::fs::path, EncodingChoice> entries;
    };

    /**
     * @brief Reads what the chooser shows for every table among `paths`
     *
     * Reads only each table's header, so this costs the same however many entries the tables
     * hold. A table that is only another name for another one gets no entry of its own. It
     * gives its name to the table it links to instead, so the chooser lists each table once.
     *
     * @param paths The files the chooser could list
     * @return The files to list, and what to show for each table among them
     */
    EncodingChoices readEncodingChoices(const std::vector<std::fs::path> &paths);

    /**
     * @brief The two lines the chooser draws for one table
     */
    struct EncodingChoiceLines {
        std::string title;
        std::string subtitle;
    };

    /**
     * @brief Splits what a table says into the line to read first and the line below it
     *
     * The description reads first, since it says what the table is for. The name goes below it,
     * then the file's name when the two differ by more than case, then every other name the
     * table answers to. A table with no description puts its name first instead, so the first
     * line is never empty.
     *
     * @param choice What the table says about itself
     * @param fileName The table's file, without its extension
     * @return The two lines, where an empty subtitle means one line is enough
     */
    EncodingChoiceLines getEncodingChoiceLines(const EncodingChoice &choice, const std::string &fileName);

    /**
     * @brief Gets a table's file name, without its extension
     */
    std::string getEncodingFileName(const std::fs::path &adjustedPath);

}
