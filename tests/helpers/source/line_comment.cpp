#include <hex/test/tests.hpp>

#include <ui/line_comment.hpp>

#include <string>
#include <vector>

using hex::ui::toggleLineComments;

namespace {

    using Lines = std::vector<std::string>;

    Lines toggle(const Lines &lines, std::string_view commentToken = "//") {
        return toggleLineComments(lines, commentToken);
    }

}

TEST_SEQUENCE("ToggleLineCommentsAddsAndRemoves") {
    TEST_ASSERT(toggle({ "u8 a;" }) == Lines{ "// u8 a;" });
    TEST_ASSERT(toggle({ "// u8 a;" }) == Lines{ "u8 a;" });

    // Round trip.
    TEST_ASSERT(toggle(toggle({ "u8 a;" })) == Lines{ "u8 a;" });

    // No space after the token.
    TEST_ASSERT(toggle({ "//u8 a;" }) == Lines{ "u8 a;" });

    // Removes only the space that commenting adds.
    TEST_ASSERT(toggle({ "//    u8 a;" }) == Lines{ "   u8 a;" });

    TEST_SUCCESS();
};

TEST_SEQUENCE("ToggleLineCommentsUsesTheShallowestIndent") {
    const Lines nested = { "    u8 a;", "        u8 b;", "    u8 c;" };
    const Lines commented = { "    // u8 a;", "    //     u8 b;", "    // u8 c;" };

    // Puts the token at the smallest indent.
    TEST_ASSERT(toggle(nested) == commented);
    TEST_ASSERT(toggle(commented) == nested);

    TEST_SUCCESS();
};

TEST_SEQUENCE("ToggleLineCommentsTreatsARunAsAWhole") {
    // Comments a partly commented run.
    const Lines mixed = { "u8 a;", "// u8 b;" };
    const Lines allCommented = { "// u8 a;", "// // u8 b;" };

    TEST_ASSERT(toggle(mixed) == allCommented);
    TEST_ASSERT(toggle(allCommented) == mixed);

    // Uncomments if all lines have a comment.
    TEST_ASSERT(toggle({ "// u8 a;", "// u8 b;" }) == (Lines{ "u8 a;", "u8 b;" }));

    TEST_SUCCESS();
};

TEST_SEQUENCE("ToggleLineCommentsLeavesBlankLinesAlone") {
    // Skips blank lines.
    TEST_ASSERT(toggle({ "// u8 a;", "", "// u8 b;" }) == (Lines{ "u8 a;", "", "u8 b;" }));
    TEST_ASSERT(toggle({ "u8 a;", "   ", "u8 b;" }) == (Lines{ "// u8 a;", "   ", "// u8 b;" }));

    // All blank.
    TEST_ASSERT(toggle({ "", "  " }) == (Lines{ "", "  " }));
    TEST_ASSERT(toggle({}) == Lines{});

    TEST_SUCCESS();
};

TEST_SEQUENCE("ToggleLineCommentsNeedsAToken") {
    // No token, no change.
    TEST_ASSERT(toggle({ "u8 a;" }, "") == Lines{ "u8 a;" });

    // Any token works.
    TEST_ASSERT(toggle({ "print 1" }, "--") == Lines{ "-- print 1" });
    TEST_ASSERT(toggle({ "-- print 1" }, "--") == Lines{ "print 1" });

    TEST_SUCCESS();
};
