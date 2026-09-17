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

    // A round trip leaves the line as it was.
    TEST_ASSERT(toggle(toggle({ "u8 a;" })) == Lines{ "u8 a;" });

    // A token with no space after it still uncomments.
    TEST_ASSERT(toggle({ "//u8 a;" }) == Lines{ "u8 a;" });

    // Only the one space the comment step adds comes back off. The rest of the
    // indentation belongs to the line.
    TEST_ASSERT(toggle({ "//    u8 a;" }) == Lines{ "   u8 a;" });

    TEST_SUCCESS();
};

TEST_SEQUENCE("ToggleLineCommentsUsesTheShallowestIndent") {
    const Lines nested = { "    u8 a;", "        u8 b;", "    u8 c;" };
    const Lines commented = { "    // u8 a;", "    //     u8 b;", "    // u8 c;" };

    // The token lands at the shallowest indent in the run, so the block keeps
    // its shape instead of every line moving to column 0.
    TEST_ASSERT(toggle(nested) == commented);
    TEST_ASSERT(toggle(commented) == nested);

    TEST_SUCCESS();
};

TEST_SEQUENCE("ToggleLineCommentsTreatsARunAsAWhole") {
    // A partly commented run comments the whole way, rather than inverting each
    // line on its own. One more press then clears it.
    const Lines mixed = { "u8 a;", "// u8 b;" };
    const Lines allCommented = { "// u8 a;", "// // u8 b;" };

    TEST_ASSERT(toggle(mixed) == allCommented);
    TEST_ASSERT(toggle(allCommented) == mixed);

    // Every line commented means uncomment.
    TEST_ASSERT(toggle({ "// u8 a;", "// u8 b;" }) == (Lines{ "u8 a;", "u8 b;" }));

    TEST_SUCCESS();
};

TEST_SEQUENCE("ToggleLineCommentsLeavesBlankLinesAlone") {
    // A blank line takes no token, and does not make the run comment a second
    // time when the lines around it are already commented.
    TEST_ASSERT(toggle({ "// u8 a;", "", "// u8 b;" }) == (Lines{ "u8 a;", "", "u8 b;" }));
    TEST_ASSERT(toggle({ "u8 a;", "   ", "u8 b;" }) == (Lines{ "// u8 a;", "   ", "// u8 b;" }));

    // Nothing to comment at all.
    TEST_ASSERT(toggle({ "", "  " }) == (Lines{ "", "  " }));
    TEST_ASSERT(toggle({}) == Lines{});

    TEST_SUCCESS();
};

TEST_SEQUENCE("ToggleLineCommentsNeedsAToken") {
    // A language with no single line comment token leaves the lines untouched
    // rather than inserting a bare space.
    TEST_ASSERT(toggle({ "u8 a;" }, "") == Lines{ "u8 a;" });

    // The token is the language's own, not a hard coded "//".
    TEST_ASSERT(toggle({ "print 1" }, "--") == Lines{ "-- print 1" });
    TEST_ASSERT(toggle({ "-- print 1" }, "--") == Lines{ "print 1" });

    TEST_SUCCESS();
};
