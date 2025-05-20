/*
A standalone hexpat file parser to extract the ‘#pragma description’.
Used to speed up “Import Pattern File…” in the “Pattern Editor” by avoiding
having to preprocess every pattern using pattern_language to extract the it.
*/

#include <ctype.h>
#include <string>
#include <optional>

#include <content/helpers/pattern_description_parser.hpp>

static inline bool IsLineEndChar(char p)
{
    return p=='\r' || p=='\n';
}

// Safe to call if *p==0.
static inline void SkipLine(const char*& p)
{
    if (!*p)
        return;
    for (++p; *p && !IsLineEndChar(*p); ++p) {}
    if (*p)
        for (++p; *p && IsLineEndChar(*p); ++p) {}
}

// Skips comments.
// Also skips '/' (start of a comment) even if it's not a complete comment.
// We don't care. We're only interested in pragmas, and pragmas don't start
// with a '/'.
// NOT safe to call if *p=0!
static void SkipComment(const char*& p)
{
    if (*p != '/')
        return;
    ++p;
    if (!*p)
        return;

    if (*p=='/') 
        SkipLine(p); // single-line comment
    else if (*p=='*') { // multi-line comment
        for (;;) {
            for (++p; *p && *p!='*'; ++p) {}
            if (!*p)
                return;
            ++p;
            if (!*p)
                return;
            if (*p =='/') {
                ++p;
                return;
            }
            else {
                ++p;
                if (!*p)
                    return;
                continue;
            }
        }
    }
}

// Returns true if we skipped some spaces, else false.
// Safe to call if *p==0.
static inline bool SkipWS(const char*& p)
{
    const char* pOrg = p;
    for (; *p && isspace(*p); ++p) {}
    return p != pOrg;
}

// NOT safe to call if *p=0!
static inline void SkipCommentsAndWS(const char*& p)
{
    for (;;) {
        SkipComment(p);
        if (!SkipWS(p))
            break;
    }
}

// Returns true if we skipped some spaces, else false.
// Safe to call if *p==0.
static inline bool SkipSpacesOnLine(const char*& p)
{
    const char* pOrg = p;
    for (; *p && (*p==' ' || *p=='\t'); ++p) {}
    return p != pOrg;
}

// Retrurns true if word is found, else false.
// Any WS on the same line is also skipped (these must be some).
// If it returns false p is unaltered.
// Safe to call if *p==0.
static inline bool SkipWordIfPresent(const char*& p, const char* pStr)
{
    const char* pCopy = p;
    for (; *pCopy==*pStr; ++pCopy, ++pStr) {} // both pCopy and pStr are NULL terminated
    if (*pStr)
        return false;
    if (!SkipSpacesOnLine(pCopy))
        return false;

    p = pCopy;
    return true;
}

// Find the argument for the pragma.
// Returns the last char of the argument. Trailing WS is not included.
// Safe to call if *p==0.
static const char* FindEndOfPragmaArgument(const char* p)
{
    const char *pLastNonWS = p;
    for (; *p && !IsLineEndChar(*p); ++p) {
        if (!isspace(*p))
            pLastNonWS = p;
    }

    return pLastNonWS;
}

namespace hex::plugin::builtin {

std::optional<std::string> get_description(std::string buffer)
{
    const char *p = buffer.c_str();

    for (;;) {
        if (!*p)
            return std::nullopt;
        SkipCommentsAndWS(p);
        if (!*p)
            return std::nullopt;

        if (SkipWordIfPresent(p, "#pragma")) {
            if (SkipWordIfPresent(p, "description")) {
                if (!*p)
                    return std::nullopt;
                const char* pLast = FindEndOfPragmaArgument(p);
                return std::string(p, pLast+1);
            }
            else {
                SkipLine(p);
                if (!*p)
                    return std::nullopt;
            }
        }
        else {
            SkipLine(p);
        }
    }

    return std::nullopt;
}

} // namespace hex::plugin::builtin
