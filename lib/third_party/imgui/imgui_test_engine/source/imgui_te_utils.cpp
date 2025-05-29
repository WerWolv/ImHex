// dear imgui test engine
// (helpers/utilities. do NOT use this as a general purpose library)

// This file is governed by the "Dear ImGui Test Engine License".
// Details of the license are provided in the LICENSE.txt file in the same directory.

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "imgui_te_utils.h"
#include "imgui.h"
#include "imgui_internal.h"
#define STR_IMPLEMENTATION
#include "thirdparty/Str/Str.h"

#if defined(_WIN32)
#if !defined(_WINDOWS_)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <shellapi.h>   // ShellExecuteA()
#include <stdio.h>
#else
#include <errno.h>
#include <unistd.h>
#endif
#ifndef _MSC_VER
#include <sys/types.h>
#include <sys/stat.h>   // stat()
#endif
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#if defined(__linux) || defined(__linux__) || defined(__MACH__) || defined(__MSL__) || defined(__MINGW32__)
#include <pthread.h>    // pthread_setname_np()
#endif
#include <chrono>       // high_resolution_clock::now()
#include <thread>       // this_thread::sleep_for()

//-----------------------------------------------------------------------------
// Hashing Helpers
//-----------------------------------------------------------------------------
// - ImHashDecoratedPathParseLiteral() [internal]
// - ImHashDecoratedPath()
// - ImFindNextDecoratedPartInPath()
//-----------------------------------------------------------------------------

// - Parse literals encoded as "$$xxxx/" and incorporate into our hash based on type.
// - $$ not passed by caller.
static ImGuiID ImHashDecoratedPathParseLiteral(ImGuiID crc, const unsigned char* str, const unsigned char* str_end, const unsigned char** out_str_remaining)
{
    // Parse type (default to int)
    ImGuiDataType type = ImGuiDataType_S32;
    if (*str == '(')
    {
        // "$$(int)????" where ???? is s32 or u32
        if (str + 5 < str_end && memcmp(str, "(int)", 5) == 0)
        {
            type = ImGuiDataType_S32;
            str += 5;
        }
        // "$$(ptr)0x????" where ???? is ptr size
        else if (str + 7 < str_end && memcmp(str, "(ptr)0x", 7) == 0)
        {
            type = ImGuiDataType_Pointer;
            str += 7;
        }
    }

    // Parse value
    switch (type)
    {
    case ImGuiDataType_S32:
    {
        // e.g. "$$(int)123" for s32/u32/ImGuiID, same as PushID(int)
        int v = 0;
        {
            int negative = 0;
            if (str < str_end && *str == '-') { negative = 1; str++; }
            if (str < str_end && *str == '+') { str++; }
            for (char c = *str; str < str_end; c = *(++str))
            {
                if (c >= '0' && c <= '9') { v = (v * 10) + (c - '0'); }
                else break;
            }
            if (negative)
                v = -v;
        }
        crc = ~ImHashData(&v, sizeof(int), ~crc);
        break;
    }
    case ImGuiDataType_Pointer:
    {
        // e.g. "$$(ptr)0x1234FFFF" for pointers, same as PushID(void*)
        intptr_t v = 0;
        {
            for (char c = *str; str < str_end; c = *(++str))
            {
                if (c >= '0' && c <= '9')       { v = (v << 4) + (c - '0'); }
                else if (c >= 'A' && c <= 'F')  { v = (v << 4) + 10 + (c - 'A'); }
                else if (c >= 'a' && c <= 'f')  { v = (v << 4) + 10 + (c - 'a'); }
                else break;
            }
        }
        crc = ~ImHashData(&v, sizeof(void*), ~crc);
        break;
    }
    }

    // "$$xxxx" must always be either end of string, either leading to a next section e.g. "$$xxxx/"
    IM_ASSERT(str == str_end || *str == '/');

    *out_str_remaining = str;
    return crc;
}

// Hash "hello/world" as if it was "helloworld"
// To hash a forward slash we need to use "hello\\/world"
//   IM_ASSERT(ImHashDecoratedPath("Hello/world")   == ImHashStr("Helloworld", 0));
//   IM_ASSERT(ImHashDecoratedPath("Hello\\/world") == ImHashStr("Hello/world", 0));
//   IM_ASSERT(ImHashDecoratedPath("$$1")           == (n = 1, ImHashData(&n, sizeof(int))));
// Adapted from ImHash(). Not particularly fast!
static const ImU32 GImGuiTestEngineCrc32LookupTable[256] =
{
#if (IMGUI_VERSION_NUM < 19152) || defined(IMGUI_USE_LEGACY_CRC32_ADLER)
    0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
    0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
    0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
    0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
    0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
    0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
    0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
    0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
    0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
    0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
    0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
    0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
    0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
    0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
    0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
    0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D,
#else
    0x00000000,0xF26B8303,0xE13B70F7,0x1350F3F4,0xC79A971F,0x35F1141C,0x26A1E7E8,0xD4CA64EB,0x8AD958CF,0x78B2DBCC,0x6BE22838,0x9989AB3B,0x4D43CFD0,0xBF284CD3,0xAC78BF27,0x5E133C24,
    0x105EC76F,0xE235446C,0xF165B798,0x030E349B,0xD7C45070,0x25AFD373,0x36FF2087,0xC494A384,0x9A879FA0,0x68EC1CA3,0x7BBCEF57,0x89D76C54,0x5D1D08BF,0xAF768BBC,0xBC267848,0x4E4DFB4B,
    0x20BD8EDE,0xD2D60DDD,0xC186FE29,0x33ED7D2A,0xE72719C1,0x154C9AC2,0x061C6936,0xF477EA35,0xAA64D611,0x580F5512,0x4B5FA6E6,0xB93425E5,0x6DFE410E,0x9F95C20D,0x8CC531F9,0x7EAEB2FA,
    0x30E349B1,0xC288CAB2,0xD1D83946,0x23B3BA45,0xF779DEAE,0x05125DAD,0x1642AE59,0xE4292D5A,0xBA3A117E,0x4851927D,0x5B016189,0xA96AE28A,0x7DA08661,0x8FCB0562,0x9C9BF696,0x6EF07595,
    0x417B1DBC,0xB3109EBF,0xA0406D4B,0x522BEE48,0x86E18AA3,0x748A09A0,0x67DAFA54,0x95B17957,0xCBA24573,0x39C9C670,0x2A993584,0xD8F2B687,0x0C38D26C,0xFE53516F,0xED03A29B,0x1F682198,
    0x5125DAD3,0xA34E59D0,0xB01EAA24,0x42752927,0x96BF4DCC,0x64D4CECF,0x77843D3B,0x85EFBE38,0xDBFC821C,0x2997011F,0x3AC7F2EB,0xC8AC71E8,0x1C661503,0xEE0D9600,0xFD5D65F4,0x0F36E6F7,
    0x61C69362,0x93AD1061,0x80FDE395,0x72966096,0xA65C047D,0x5437877E,0x4767748A,0xB50CF789,0xEB1FCBAD,0x197448AE,0x0A24BB5A,0xF84F3859,0x2C855CB2,0xDEEEDFB1,0xCDBE2C45,0x3FD5AF46,
    0x7198540D,0x83F3D70E,0x90A324FA,0x62C8A7F9,0xB602C312,0x44694011,0x5739B3E5,0xA55230E6,0xFB410CC2,0x092A8FC1,0x1A7A7C35,0xE811FF36,0x3CDB9BDD,0xCEB018DE,0xDDE0EB2A,0x2F8B6829,
    0x82F63B78,0x709DB87B,0x63CD4B8F,0x91A6C88C,0x456CAC67,0xB7072F64,0xA457DC90,0x563C5F93,0x082F63B7,0xFA44E0B4,0xE9141340,0x1B7F9043,0xCFB5F4A8,0x3DDE77AB,0x2E8E845F,0xDCE5075C,
    0x92A8FC17,0x60C37F14,0x73938CE0,0x81F80FE3,0x55326B08,0xA759E80B,0xB4091BFF,0x466298FC,0x1871A4D8,0xEA1A27DB,0xF94AD42F,0x0B21572C,0xDFEB33C7,0x2D80B0C4,0x3ED04330,0xCCBBC033,
    0xA24BB5A6,0x502036A5,0x4370C551,0xB11B4652,0x65D122B9,0x97BAA1BA,0x84EA524E,0x7681D14D,0x2892ED69,0xDAF96E6A,0xC9A99D9E,0x3BC21E9D,0xEF087A76,0x1D63F975,0x0E330A81,0xFC588982,
    0xB21572C9,0x407EF1CA,0x532E023E,0xA145813D,0x758FE5D6,0x87E466D5,0x94B49521,0x66DF1622,0x38CC2A06,0xCAA7A905,0xD9F75AF1,0x2B9CD9F2,0xFF56BD19,0x0D3D3E1A,0x1E6DCDEE,0xEC064EED,
    0xC38D26C4,0x31E6A5C7,0x22B65633,0xD0DDD530,0x0417B1DB,0xF67C32D8,0xE52CC12C,0x1747422F,0x49547E0B,0xBB3FFD08,0xA86F0EFC,0x5A048DFF,0x8ECEE914,0x7CA56A17,0x6FF599E3,0x9D9E1AE0,
    0xD3D3E1AB,0x21B862A8,0x32E8915C,0xC083125F,0x144976B4,0xE622F5B7,0xF5720643,0x07198540,0x590AB964,0xAB613A67,0xB831C993,0x4A5A4A90,0x9E902E7B,0x6CFBAD78,0x7FAB5E8C,0x8DC0DD8F,
    0xE330A81A,0x115B2B19,0x020BD8ED,0xF0605BEE,0x24AA3F05,0xD6C1BC06,0xC5914FF2,0x37FACCF1,0x69E9F0D5,0x9B8273D6,0x88D28022,0x7AB90321,0xAE7367CA,0x5C18E4C9,0x4F48173D,0xBD23943E,
    0xF36E6F75,0x0105EC76,0x12551F82,0xE03E9C81,0x34F4F86A,0xC69F7B69,0xD5CF889D,0x27A40B9E,0x79B737BA,0x8BDCB4B9,0x988C474D,0x6AE7C44E,0xBE2DA0A5,0x4C4623A6,0x5F16D052,0xAD7D5351
#endif
};

ImGuiID ImHashDecoratedPath(const char* str, const char* str_end, ImGuiID seed)
{
    const ImU32* crc32_lut = GImGuiTestEngineCrc32LookupTable;

    // Prefixing the string with / ignore the seed
    if (str != str_end && str[0] == '/')
        seed = 0;

    seed = ~seed;
    ImU32 crc = seed;

    // Focus for non-zero terminated string for consistency
    if (str_end == nullptr)
        str_end = str + strlen(str);

    bool inhibit_one = false;
    bool new_section = true;
    const unsigned char* current = (const unsigned char*)str;
    while (current < (const unsigned char*)str_end)
    {
        const unsigned char c = *current++;

        // Backslash to inhibit special behavior of following character
        if (c == '\\' && !inhibit_one)
        {
            inhibit_one = true;
            continue;
        }

        // Forward slashes are ignored unless prefixed with a backward slash
        if (c == '/' && !inhibit_one)
        {
            inhibit_one = false;
            new_section = true;
            seed = crc; // Set seed to the new path
            continue;
        }

        // $$ at the beginning of a section to encode literals.
        // - Currently: "$$????" = hash of 1 as int
        // - May add pointers and other types.
        if (c == '$' && current[0] == '$' && !inhibit_one && new_section)
        {
            crc = ImHashDecoratedPathParseLiteral(crc, current + 1, (const unsigned char*)str_end, &current);
            continue;
        }

        // Reset the hash when encountering ###
        if (c == '#' && current[0] == '#' && current[1] == '#')
            crc = seed;

        // Hash byte
        crc = (crc >> 8) ^ crc32_lut[(crc & 0xFF) ^ c];

        inhibit_one = new_section = false;
    }
    return ~crc;
}

// Returns a next element of decorated hash path.
//    "//hello/world/child" --> "world/child"
//    "world/child"         --> "child"
// This is a helper for code needing to do some parsing of individual nodes in a path.
// Note: we need the (unsigned char*) stuff in order to keep code similar to ImHashDecoratedPath(). They are not really necessary in this function tho.
const char* ImFindNextDecoratedPartInPath(const char* str, const char* str_end)
{
    const unsigned char* current = (const unsigned char*)str;
    while (*current == '/')
        current++;

    bool inhibit_one = false;
    while (true)
    {
        if (str_end != nullptr && current == (const unsigned char*)str_end)
            break;

        const unsigned char c = *current++;
        if (c == 0)
            break;
        if (c == '\\' && !inhibit_one)
        {
            inhibit_one = true;
            continue;
        }

        // Forward slashes are ignored unless prefixed with a backward slash
        if (c == '/' && !inhibit_one)
            return (const char*)current;

        inhibit_one = false;
    }
    return nullptr;
}

//-----------------------------------------------------------------------------
// File/Directory Helpers
//-----------------------------------------------------------------------------
// - ImFileExist()
// - ImFileCreateDirectoryChain()
// - ImFileFindInParents()
// - ImFileLoadSourceBlurb()
//-----------------------------------------------------------------------------

#if _WIN32
static const char IM_DIR_SEPARATOR = '\\';

static void ImUtf8ToWideChar(const char* multi_byte, ImVector<wchar_t>* buf)
{
    const int wsize = ::MultiByteToWideChar(CP_UTF8, 0, multi_byte, -1, nullptr, 0);
    buf->resize(wsize);
    ::MultiByteToWideChar(CP_UTF8, 0, multi_byte, -1, (wchar_t*)buf->Data, wsize);
}
#else
static const char IM_DIR_SEPARATOR = '/';
#endif

bool ImFileExist(const char* filename)
{
    struct stat dir_stat;
    int ret = stat(filename, &dir_stat);
    return (ret == 0);
}

bool ImFileDelete(const char* filename)
{
#if _WIN32
    ImVector<wchar_t> buf;
    ImUtf8ToWideChar(filename, &buf);
    return ::DeleteFileW(&buf[0]) == TRUE;
#else
    unlink(filename);
#endif
    return false;
}

// Create directories for specified path. Slashes will be replaced with platform directory separators.
// e.g. ImFileCreateDirectoryChain("aaaa/bbbb/cccc.png")
// will try to create "aaaa/" then "aaaa/bbbb/".
bool ImFileCreateDirectoryChain(const char* path, const char* path_end)
{
    IM_ASSERT(path != nullptr);
    IM_ASSERT(path[0] != 0);

    if (path_end == nullptr)
        path_end = path + strlen(path);

    // Copy in a local, zero-terminated buffer
    size_t path_len = (size_t)(path_end - path);
    char* path_local = (char*)IM_ALLOC(path_len + 1);
    memcpy(path_local, path, path_len);
    path_local[path_len] = 0;

#if defined(_WIN32)
    ImVector<wchar_t> buf;
#endif
    // Modification of passed file_name allows us to avoid extra temporary memory allocation.
    // strtok() pokes \0 into places where slashes are, we create a directory using directory_name and restore slash.
    for (char* token = strtok(path_local, "\\/"); token != nullptr; token = strtok(nullptr, "\\/"))
    {
        // strtok() replaces slashes with NULLs. Overwrite removed slashes here with the type of slashes the OS needs (win32 functions need backslashes).
        if (token != path_local)
            *(token - 1) = IM_DIR_SEPARATOR;

#if defined(_WIN32)
        // Use ::CreateDirectoryW() because ::CreateDirectoryA() treat filenames in the local code-page instead of UTF-8
        // We cannot use ImWchar, which can be 32bits if IMGUI_USE_WCHAR32 (and CreateDirectoryW require 16bits wchar)
        int filename_wsize = MultiByteToWideChar(CP_UTF8, 0, path_local, -1, nullptr, 0);
        buf.resize(filename_wsize);
        MultiByteToWideChar(CP_UTF8, 0, path_local, -1, &buf[0], filename_wsize);
        if (!::CreateDirectoryW((wchar_t*)&buf[0], nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
#else
        if (mkdir(path_local, S_IRWXU) != 0 && errno != EEXIST)
#endif
        {
            IM_FREE(path_local);
            return false;
        }
    }
    IM_FREE(path_local);
    return true;
}

bool ImFileFindInParents(const char* sub_path, int max_parent_count, Str* output)
{
    IM_ASSERT(sub_path != nullptr);
    IM_ASSERT(output != nullptr);
    for (int parent_level = 0; parent_level < max_parent_count; parent_level++)
    {
        output->clear();
        for (int j = 0; j < parent_level; j++)
            output->append("../");
        output->append(sub_path);
        if (ImFileExist(output->c_str()))
            return true;
    }
    output->clear();
    return false;
}

bool ImFileLoadSourceBlurb(const char* file_name, int line_no_start, int line_no_end, ImGuiTextBuffer* out_buf)
{
    size_t file_size = 0;
    char* file_begin = (char*)ImFileLoadToMemory(file_name, "rb", &file_size, 1);
    if (file_begin == nullptr)
        return false;

    char* file_end = file_begin + file_size;
    int line_no = 0;
    const char* test_src_begin = nullptr;
    const char* test_src_end = nullptr;
    for (const char* p = file_begin; p < file_end; )
    {
        line_no++;
        const char* line_begin = p;
        const char* line_end = ImStrchrRange(line_begin + 1, file_end, '\n');
        if (line_end == nullptr)
            line_end = file_end;
        if (line_no >= line_no_start && line_no <= line_no_end)
        {
            if (test_src_begin == nullptr)
                test_src_begin = line_begin;
            test_src_end = ImMax(test_src_end, line_end);
        }
        p = line_end + 1;
    }

    if (test_src_begin != nullptr)
        out_buf->append(test_src_begin, test_src_end);
    else
        out_buf->clear();

    ImGui::MemFree(file_begin);
    return true;
}

//-----------------------------------------------------------------------------
// Path Helpers
//-----------------------------------------------------------------------------
// - ImPathFindFilename()
// - ImPathFindFileExt()
// - ImPathFixSeparatorsForCurrentOS()
//-----------------------------------------------------------------------------

const char* ImPathFindFilename(const char* path, const char* path_end)
{
    IM_ASSERT(path != nullptr);
    if (!path_end)
        path_end = path + strlen(path);
    const char* p = path_end;
    while (p > path)
    {
        if (p[-1] == '/' || p[-1] == '\\')
            break;
        p--;
    }
    return p;
}

// "folder/filename" -> return pointer to "" (end of string)
// "folder/filename.png" -> return pointer to ".png"
// "folder/filename.png.bak" -> return pointer to ".png.bak"
const char* ImPathFindExtension(const char* path, const char* path_end)
{
    if (!path_end)
        path_end = path + strlen(path);
    const char* filename = ImPathFindFilename(path, path_end);
    const char* p = filename;
    while (p < path_end)
    {
        if (p[0] == '.')
            break;
        p++;
    }
    return p;
}

void ImPathFixSeparatorsForCurrentOS(char* buf)
{
#ifdef _WIN32
    for (char* p = buf; *p != 0; p++)
        if (*p == '/')
            *p = '\\';
#else
    for (char* p = buf; *p != 0; p++)
        if (*p == '\\')
            *p = '/';
#endif
}

//-----------------------------------------------------------------------------
// String Helpers
//-----------------------------------------------------------------------------

static const char* ImStrStr(const char* haystack, size_t hlen, const char* needle, int nlen)
{
    const char* end = haystack + hlen;
    const char* p = haystack;
    while ((p = (const char*)memchr(p, *needle, end - p)) != nullptr)
    {
        if (end - p < nlen)
            return nullptr;
        if (memcmp(p, needle, nlen) == 0)
            return p;
        p++;
    }
    return nullptr;
}

void ImStrReplace(Str* s, const char* find, const char* repl)
{
    IM_ASSERT(find != nullptr && *find);
    IM_ASSERT(repl != nullptr);
    int find_len = (int)strlen(find);
    int repl_len = (int)strlen(repl);
    int repl_diff = repl_len - find_len;

    // Estimate required length of new buffer if string size increases.
    int need_capacity = s->capacity();
    int num_matches = INT_MAX;
    if (repl_diff > 0)
    {
        num_matches = 0;
        need_capacity = s->length();
        for (char* p = s->c_str(), *end = s->c_str() + s->length(); p != nullptr && p < end;)
        {
            p = (char*)ImStrStr(p, end - p, find, find_len);
            if (p)
            {
                need_capacity += repl_diff;
                p += find_len;
                num_matches++;
            }
        }
    }

    if (num_matches == 0)
        return;

    const char* not_owned_data = s->owned() ? nullptr : s->c_str();
    if (!s->owned() || need_capacity > s->capacity())
        s->reserve(need_capacity);
    if (not_owned_data != nullptr)
        s->set(not_owned_data);

    // Replace data.
    for (char* p = s->c_str(), *end = s->c_str() + s->length(); p != nullptr && p < end && num_matches--;)
    {
        p = (char*)ImStrStr(p, end - p, find, find_len);
        if (p)
        {
            memmove(p + repl_len, p + find_len, end - p - find_len + 1);
            memcpy(p, repl, repl_len);
            p += repl_len;
            end += repl_diff;
        }
    }
}

const char* ImStrchrRangeWithEscaping(const char* str, const char* str_end, char find_c)
{
    while (str < str_end)
    {
        const char c = *str;
        if (c == '\\')
        {
            str += 2;
            continue;
        }
        if (c == find_c)
            return str;
        str++;
    }
    return nullptr;
}

// Suboptimal but ok for the data size we are dealing with (see commit on 2022/08/22 for a faster and more complicated version)
void ImStrXmlEscape(Str* s)
{
    ImStrReplace(s, "&", "&amp;");
    ImStrReplace(s, "<", "&lt;");
    ImStrReplace(s, ">", "&gt;");
    ImStrReplace(s, "\"", "&quot;");
    ImStrReplace(s, "\'", "&apos;");
}

// Based on code from https://github.com/EddieBreeg/C_b64 by @EddieBreeg.
int ImStrBase64Encode(const unsigned char* src, char* dst, int length)
{
    static const char* b64Table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
    int i, j, k, l, encoded_len = 0;

    while (length > 0)
    {
        switch (length)
        {
        case 1:
            i = src[0] >> 2;
            j = (src[0] & 3) << 4;
            k = 64;
            l = 64;
            break;
        case 2:
            i = src[0] >> 2;
            j = ((src[0] & 3) << 4) | (src[1] >> 4);
            k = (src[1] & 15) << 2;
            l = 64;
            break;
        default:
            i = src[0] >> 2;
            j = ((src[0] & 3) << 4) | (src[1] >> 4);
            k = ((src[1] & 0xf) << 2) | (src[2] >> 6 & 3);
            l = src[2] & 0x3f;
            break;
        }
        dst[0] = b64Table[i];
        dst[1] = b64Table[j];
        dst[2] = b64Table[k];
        dst[3] = b64Table[l];
        src += 3;
        dst += 4;
        length -= 3;
        encoded_len += 4;
    }
    return encoded_len;
}

//-----------------------------------------------------------------------------
// Parsing Helpers
//-----------------------------------------------------------------------------
// - ImParseSplitCommandLine()
// - ImParseFindIniSection()
//-----------------------------------------------------------------------------

void    ImParseExtractArgcArgvFromCommandLine(int* out_argc, char const*** out_argv, const char* cmd_line)
{
    size_t cmd_line_len = strlen(cmd_line);

    int n = 1;
    {
        const char* p = cmd_line;
        while (*p != 0)
        {
            const char* arg = p;
            while (*arg == ' ')
                arg++;
            const char* arg_end = strchr(arg, ' ');
            if (arg_end == nullptr)
                p = arg_end = cmd_line + cmd_line_len;
            else
                p = arg_end + 1;
            n++;
        }
    }

    int argc = n;
    char const** argv = (char const**)malloc(sizeof(char*) * ((size_t)argc + 1) + (cmd_line_len + 1));
    IM_ASSERT(argv != nullptr);
    char* cmd_line_dup = (char*)argv + sizeof(char*) * ((size_t)argc + 1);
    strcpy(cmd_line_dup, cmd_line);

    {
        argv[0] = "main.exe";
        argv[argc] = nullptr;

        char* p = cmd_line_dup;
        for (n = 1; n < argc; n++)
        {
            char* arg = p;
            char* arg_end = strchr(arg, ' ');
            if (arg_end == nullptr)
                p = arg_end = cmd_line_dup + cmd_line_len;
            else
                p = arg_end + 1;
            argv[n] = arg;
            arg_end[0] = 0;
        }
    }

    *out_argc = argc;
    *out_argv = argv;
}

bool    ImParseFindIniSection(const char* ini_config, const char* header, ImVector<char>* result)
{
    IM_ASSERT(ini_config != nullptr);
    IM_ASSERT(header != nullptr);
    IM_ASSERT(result != nullptr);

    size_t ini_len = strlen(ini_config);
    size_t header_len = strlen(header);

    IM_ASSERT(header_len > 0);

    if (ini_len == 0)
        return false;

    const char* section_start = strstr(ini_config, header);
    if (section_start == nullptr)
        return false;

    const char* section_end = strstr(section_start + header_len, "\n[");
    if (section_end == nullptr)
        section_end = section_start + ini_len;

    // "\n[" matches next header start on all platforms, but it cuts new line marker in half on windows.
    if (*(section_end - 1) == '\r')
        --section_end;

    size_t section_len = (size_t)(section_end - section_start);
    result->resize((int)section_len + 1);
    ImStrncpy(result->Data, section_start, section_len);

    return true;
}

//-----------------------------------------------------------------------------
// Time Helpers
//-----------------------------------------------------------------------------
// - ImTimeGetInMicroseconds()
// - ImTimestampToISO8601()
//-----------------------------------------------------------------------------

uint64_t ImTimeGetInMicroseconds()
{
    // Trying std::chrono out of unfettered optimism that it may actually work..
    using namespace std;
    chrono::microseconds ms = chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch());
    return (uint64_t)ms.count();
}

void ImTimestampToISO8601(uint64_t timestamp, Str* out_date)
{
    time_t unix_time = (time_t)(timestamp / 1000000); // Convert to seconds.
    tm* time = gmtime(&unix_time);
    const char* time_format = "%Y-%m-%dT%H:%M:%S";

    // max_size "maximum number of characters to be copied to ptr, including the terminating null-character."
    // return   "returns the total number of characters copied to ptr (not including the terminating null-character)"
    size_t size_req = strftime(out_date->c_str(), out_date->capacity() + 1, time_format, time);
    if (size_req > (size_t)out_date->capacity())
    {
        out_date->reserve((int)size_req);
        strftime(out_date->c_str(), out_date->capacity() + 1, time_format, time);
    }
}

//-----------------------------------------------------------------------------
// Threading Helpers
//-----------------------------------------------------------------------------
// - ImThreadSleepInMilliseconds()
// - ImThreadSetCurrentThreadDescription()
//-----------------------------------------------------------------------------

void ImThreadSleepInMilliseconds(int ms)
{
    using namespace std;
    this_thread::sleep_for(chrono::milliseconds(ms));
}

#if defined(_MSC_VER)
// Helper function for setting thread name on Win32
// This is a separate function because __try cannot coexist with local objects that need destructors called on stack unwind
static void ImThreadSetCurrentThreadDescriptionWin32OldStyle(const char* description)
{
    // Old-style Win32 thread name setting method
    // See https://docs.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code
    const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push,8)
    typedef struct tagTHREADNAME_INFO
    {
        DWORD dwType; // Must be 0x1000.
        LPCSTR szName; // Pointer to name (in user addr space).
        DWORD dwThreadID; // Thread ID (-1=caller thread).
        DWORD dwFlags; // Reserved for future use, must be zero.
    } THREADNAME_INFO;
#pragma pack(pop)

    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = description;
    info.dwThreadID = (DWORD)-1;
    info.dwFlags = 0;
#pragma warning(push)
#pragma warning(disable: 6320 6322)
    __try
    {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
#pragma warning(pop)
}
#endif // #ifdef _WIN32

// Set the description (name) of the current thread for debugging purposes
void ImThreadSetCurrentThreadDescription(const char* description)
{
#if defined(_MSC_VER) // Windows + Visual Studio
    // New-style thread name setting
    // Only supported from Win 10 version 1607/Server 2016 onwards, hence the need for dynamic linking

    typedef HRESULT(WINAPI* SetThreadDescriptionFunc)(HANDLE hThread, PCWSTR lpThreadDescription);

    SetThreadDescriptionFunc set_thread_description = (SetThreadDescriptionFunc)::GetProcAddress(GetModuleHandleA("Kernel32.dll"), "SetThreadDescription");
    if (set_thread_description)
    {
        ImVector<ImWchar> buf;
        const int description_wsize = ImTextCountCharsFromUtf8(description, nullptr) + 1;
        buf.resize(description_wsize);
        ImTextStrFromUtf8(&buf[0], description_wsize, description, nullptr);
        set_thread_description(::GetCurrentThread(), (wchar_t*)&buf[0]);
    }

    // Also do the old-style method too even if the new-style one worked, as the two work in slightly different sets of circumstances
    ImThreadSetCurrentThreadDescriptionWin32OldStyle(description);
#elif defined(__linux) || defined(__linux__) || defined(__MINGW32__) // Linux or MingW
    pthread_setname_np(pthread_self(), description);
#elif defined(__MACH__) || defined(__MSL__) // OSX
    pthread_setname_np(description);
#else
    // This is a nice-to-have rather than critical functionality, so fail silently if we don't support this platform
#endif
}

//-----------------------------------------------------------------------------
// Build info helpers
//-----------------------------------------------------------------------------
// - ImBuildGetCompilationInfo()
// - ImBuildGetGitBranchName()
//-----------------------------------------------------------------------------

// Turn __DATE__ "Jan 10 2019" into "2019-01-10"
static void ImBuildParseDateFromCompilerIntoYMD(const char* in_date, char* out_buf, size_t out_buf_size)
{
    char month_str[5];
    int year, month, day;
    sscanf(in_date, "%3s %d %d", month_str, &day, &year);
    const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char* p = strstr(month_names, month_str);
    month = p ? (int)(1 + (p - month_names) / 3) : 0;
    ImFormatString(out_buf, out_buf_size, "%04d-%02d-%02d", year, month, day);
}

// Those strings are used to output easily identifiable markers in compare logs. We only need to support what we use for testing.
// We can probably grab info in eaplatform.h/eacompiler.h etc. in EASTL
const ImBuildInfo* ImBuildGetCompilationInfo()
{
    static ImBuildInfo build_info;

    if (build_info.Type[0] == '\0')
    {
        // Build Type
#if defined(DEBUG) || defined(_DEBUG)
        build_info.Type = "Debug";
#else
        build_info.Type = "Release";
#endif

        // CPU
#if defined(_M_X86) || defined(_M_IX86) || defined(__i386) || defined(__i386__) || defined(_X86_) || defined(_M_AMD64) || defined(_AMD64_) || defined(__x86_64__)
        build_info.Cpu = (sizeof(size_t) == 4) ? "X86" : "X64";
#elif defined(__aarch64__) || (defined(_M_ARM64) && defined(_WIN64))
        build_info.Cpu = "ARM64";
#elif defined(__EMSCRIPTEN__)
        build_info.Cpu = "WebAsm";
#else
        build_info.Cpu = (sizeof(size_t) == 4) ? "Unknown32" : "Unknown64";
#endif

        // Platform/OS
#if defined(_WIN32)
        build_info.OS = "Windows";
#elif defined(__linux) || defined(__linux__)
        build_info.OS = "Linux";
#elif defined(__MACH__) || defined(__MSL__)
        build_info.OS = "OSX";
#elif defined(__ORBIS__)
        build_info.OS = "PS4";
#elif defined(__PROSPERO__)
        build_info.OS = "PS5";
#elif defined(_DURANGO)
        build_info.OS = "XboxOne";
#else
        build_info.OS = "Unknown";
#endif

        // Compiler
#if defined(_MSC_VER)
        build_info.Compiler = "MSVC";
#elif defined(__clang__)
        build_info.Compiler = "Clang";
#elif defined(__GNUC__)
        build_info.Compiler = "GCC";
#else
        build_info.Compiler = "Unknown";
#endif

        // Date/Time
        ImBuildParseDateFromCompilerIntoYMD(__DATE__, build_info.Date, IM_ARRAYSIZE(build_info.Date));
        build_info.Time = __TIME__;
    }

    return &build_info;
}

bool ImBuildFindGitBranchName(const char* git_repo_path, Str* branch_name)
{
    IM_ASSERT(git_repo_path != nullptr);
    IM_ASSERT(branch_name != nullptr);
    Str256f head_path("%s/.git/HEAD", git_repo_path);
    size_t head_size = 0;
    bool result = false;
    if (char* git_head = (char*)ImFileLoadToMemory(head_path.c_str(), "r", &head_size, 1))
    {
        const char prefix[] = "ref: refs/heads/";       // Branch name is prefixed with this in HEAD file.
        const int prefix_length = IM_ARRAYSIZE(prefix) - 1;
        strtok(git_head, "\r\n");                       // Trim new line
        if (head_size > prefix_length && strncmp(git_head, prefix, prefix_length) == 0)
        {
            // "ref: refs/heads/master" -> "master"
            branch_name->set(git_head + prefix_length);
        }
        else
        {
            // Should be git hash, keep first 8 characters (see #42)
            branch_name->setf("%.8s", git_head);
        }
        result = true;
        IM_FREE(git_head);
    }
    return result;
}

//-----------------------------------------------------------------------------
// Operating System Helpers
//-----------------------------------------------------------------------------
// - ImOsCreateProcess()
// - ImOsPOpen()
// - ImOsPClose()
// - ImOsOpenInShell()
// - ImOsConsoleSetTextColor()
// - ImOsIsDebuggerPresent()
//-----------------------------------------------------------------------------

bool    ImOsCreateProcess(const char* cmd_line)
{
#if defined(_WIN32) && !IMGUI_TEST_ENGINE_IS_GAME_CONSOLE
    STARTUPINFOA siStartInfo;
    PROCESS_INFORMATION piProcInfo;
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
    char* cmd_line_copy = ImStrdup(cmd_line);
    BOOL ret = ::CreateProcessA(nullptr, cmd_line_copy, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &siStartInfo, &piProcInfo);
    free(cmd_line_copy);
    ::CloseHandle(siStartInfo.hStdInput);
    ::CloseHandle(siStartInfo.hStdOutput);
    ::CloseHandle(siStartInfo.hStdError);
    ::CloseHandle(piProcInfo.hProcess);
    ::CloseHandle(piProcInfo.hThread);
    return ret != 0;
#else
    IM_UNUSED(cmd_line);
    IM_ASSERT(0);
    return false;
#endif
}

FILE*       ImOsPOpen(const char* cmd_line, const char* mode)
{
    IM_ASSERT(cmd_line != nullptr && *cmd_line);
    IM_ASSERT(mode != nullptr && *mode);
#if defined(_WIN32) && !IMGUI_TEST_ENGINE_IS_GAME_CONSOLE
    ImVector<wchar_t> w_cmd_line;
    ImVector<wchar_t> w_mode;
    ImUtf8ToWideChar(cmd_line, &w_cmd_line);
    ImUtf8ToWideChar(mode, &w_mode);
    w_mode.resize(w_mode.Size + 1);
    wcscat(w_mode.Data, L"b");   // Windows requires 'b' mode while unixes do not support it and default to binary.
    return _wpopen(w_cmd_line.Data, w_mode.Data);
#elif !IMGUI_TEST_ENGINE_IS_GAME_CONSOLE
    return popen(cmd_line, mode);
#else
    IM_ASSERT(0);
    return NULL;
#endif
}

void        ImOsPClose(FILE* fp)
{
    IM_ASSERT(fp != nullptr);
#if defined(_WIN32) && !IMGUI_TEST_ENGINE_IS_GAME_CONSOLE
    _pclose(fp);
#elif !IMGUI_TEST_ENGINE_IS_GAME_CONSOLE
    pclose(fp);
#else
    IM_ASSERT(0);
#endif
}

void    ImOsOpenInShell(const char* path)
{
    Str256 command(path);
#if defined(_WIN32) && !IMGUI_TEST_ENGINE_IS_GAME_CONSOLE
    ImPathFixSeparatorsForCurrentOS(command.c_str());
    ::ShellExecuteA(nullptr, "open", command.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
#elif !IMGUI_TEST_ENGINE_IS_GAME_CONSOLE
#if __APPLE__
    const char* open_executable = "open";
#else
    const char* open_executable = "xdg-open";
#endif
    command.setf("%s \"%s\"", open_executable, path);
    ImPathFixSeparatorsForCurrentOS(command.c_str());
    system(command.c_str());
#else
    IM_UNUSED(path);
    IM_ASSERT(0);
#endif
}

void    ImOsConsoleSetTextColor(ImOsConsoleStream stream, ImOsConsoleTextColor color)
{
#if defined(_WIN32) && !IMGUI_TEST_ENGINE_IS_GAME_CONSOLE
    HANDLE hConsole = 0;
    switch (stream)
    {
    case ImOsConsoleStream_StandardOutput: hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE); break;
    case ImOsConsoleStream_StandardError:  hConsole = ::GetStdHandle(STD_ERROR_HANDLE);  break;
    }
    WORD wAttributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    switch (color)
    {
    case ImOsConsoleTextColor_Black:        wAttributes = 0x00; break;
    case ImOsConsoleTextColor_White:        wAttributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; break;
    case ImOsConsoleTextColor_BrightWhite:  wAttributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;
    case ImOsConsoleTextColor_BrightRed:    wAttributes = FOREGROUND_RED | FOREGROUND_INTENSITY; break;
    case ImOsConsoleTextColor_BrightGreen:  wAttributes = FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
    case ImOsConsoleTextColor_BrightBlue:   wAttributes = FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;
    case ImOsConsoleTextColor_BrightYellow: wAttributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
    default: IM_ASSERT(0);
    }
    ::SetConsoleTextAttribute(hConsole, wAttributes);
#elif defined(__linux) || defined(__linux__) || defined(__MACH__) || defined(__MSL__)
    // FIXME: check system capabilities (with environment variable TERM)
    FILE* handle = 0;
    switch (stream)
    {
    case ImOsConsoleStream_StandardOutput: handle = stdout; break;
    case ImOsConsoleStream_StandardError:  handle = stderr; break;
    }

    const char* modifier = "";
    switch (color)
    {
    case ImOsConsoleTextColor_Black:        modifier = "\033[30m";   break;
    case ImOsConsoleTextColor_White:        modifier = "\033[0m";    break;
    case ImOsConsoleTextColor_BrightWhite:  modifier = "\033[1;37m"; break;
    case ImOsConsoleTextColor_BrightRed:    modifier = "\033[1;31m"; break;
    case ImOsConsoleTextColor_BrightGreen:  modifier = "\033[1;32m"; break;
    case ImOsConsoleTextColor_BrightBlue:   modifier = "\033[1;34m"; break;
    case ImOsConsoleTextColor_BrightYellow: modifier = "\033[1;33m"; break;
    default: IM_ASSERT(0);
    }

    fprintf(handle, "%s", modifier);
#else
    IM_UNUSED(stream);
    IM_UNUSED(color);
#endif
}

bool    ImOsIsDebuggerPresent()
{
#ifdef _WIN32
    return ::IsDebuggerPresent() != 0;
#elif defined(__linux__)
    int debugger_pid = 0;
    char buf[2048];                                 // TracerPid is located near the start of the file. If end of the buffer gets cut off thats fine.
    FILE* fp = fopen("/proc/self/status", "rb");    // Can not use ImFileLoadToMemory because size detection of /proc/self/status would fail.
    if (fp == nullptr)
        return false;
    fread(buf, 1, IM_ARRAYSIZE(buf), fp);
    fclose(fp);
    buf[IM_ARRAYSIZE(buf) - 1] = 0;
    if (char* tracer_pid = strstr(buf, "TracerPid:"))
    {
        tracer_pid += 10;   // Skip label
        while (isspace(*tracer_pid))
            tracer_pid++;
        debugger_pid = atoi(tracer_pid);
    }
    return debugger_pid != 0;
#elif defined(__APPLE__)
    // https://stackoverflow.com/questions/2200277/detecting-debugger-on-mac-os-x
    int                 junk;
    int                 mib[4];
    struct kinfo_proc   info;
    size_t              size;

    // Initialize mib, which tells sysctl the info we want, in this case
    // we're looking for information about a specific process ID.
    info.kp_proc.p_flag = 0;
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = getpid();

    size = sizeof(info);
    junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, nullptr, 0);
    IM_ASSERT(junk == 0);

    // We're being debugged if the P_TRACED flag is set.
    return (info.kp_proc.p_flag & P_TRACED) != 0;
#else
    // FIXME
    return false;
#endif
}

void    ImOsOutputDebugString(const char* message)
{
#ifdef _WIN32
    OutputDebugStringA(message);
#else
    IM_UNUSED(message);
#endif
}

//-----------------------------------------------------------------------------
// Str.h + InputText bindings
//-----------------------------------------------------------------------------

struct InputTextCallbackStr_UserData
{
    Str*                    StrObj;
    ImGuiInputTextCallback  ChainCallback;
    void*                   ChainCallbackUserData;
};

static int InputTextCallbackStr(ImGuiInputTextCallbackData* data)
{
    InputTextCallbackStr_UserData* user_data = (InputTextCallbackStr_UserData*)data->UserData;
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
        // Resize string callback
        // If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
        Str* str = user_data->StrObj;
        IM_ASSERT(data->Buf == str->c_str());
        str->reserve(data->BufTextLen);
        data->Buf = (char*)str->c_str();
    }
    else if (user_data->ChainCallback)
    {
        // Forward to user callback, if any
        data->UserData = user_data->ChainCallbackUserData;
        return user_data->ChainCallback(data);
    }
    return 0;
}

// Draw an extra colored frame over the previous item
// Similar to DebugDrawItemRect() but use Max(1.0f, FrameBorderSize)
void ImGui::ItemErrorFrame(ImU32 col)
{
    ImGuiContext& g = *GetCurrentContext();
    ImDrawList* drawlist = GetWindowDrawList();
    ImGuiStyle& style = GetStyle();
    // FIXME: GetItemRectMin() / GetItemRectMax() will include label. NavRect is not probably defined :(
    drawlist->AddRect(g.LastItemData.NavRect.Min, g.LastItemData.NavRect.Max, GetColorU32(col), style.FrameRounding, ImDrawFlags_None, ImMax(1.0f, style.FrameBorderSize));
}

bool ImGui::InputText(const char* label, Str* str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCallbackStr_UserData cb_user_data;
    cb_user_data.StrObj = str;
    cb_user_data.ChainCallback = callback;
    cb_user_data.ChainCallbackUserData = user_data;
    return InputText(label, (char*)str->c_str(), (size_t)str->capacity() + 1, flags, InputTextCallbackStr, &cb_user_data);
}

bool ImGui::InputTextWithHint(const char* label, const char* hint, Str* str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCallbackStr_UserData cb_user_data;
    cb_user_data.StrObj = str;
    cb_user_data.ChainCallback = callback;
    cb_user_data.ChainCallbackUserData = user_data;
    return InputTextWithHint(label, hint, (char*)str->c_str(), (size_t)str->capacity() + 1, flags, InputTextCallbackStr, &cb_user_data);
}

bool ImGui::InputTextMultiline(const char* label, Str* str, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCallbackStr_UserData cb_user_data;
    cb_user_data.StrObj = str;
    cb_user_data.ChainCallback = callback;
    cb_user_data.ChainCallbackUserData = user_data;
    return InputTextMultiline(label, (char*)str->c_str(), (size_t)str->capacity() + 1, size, flags, InputTextCallbackStr, &cb_user_data);
}

// anchor parameter indicates which split would retain it's constant size.
// anchor = 0 - both splits resize when parent container size changes. Both value_1 and value_2 should be persistent.
// anchor = -1 - top/left split would have a constant size. bottom/right split would resize when parent container size changes. value_1 should be persistent, value_2 will always be recalculated from value_1.
// anchor = +1 - bottom/right split would have a constant size. top/left split would resize when parent container size changes. value_2 should be persistent, value_1 will always be recalculated from value_2.
bool ImGui::Splitter(const char* id, float* value_1, float* value_2, int axis, int anchor, float min_size_0, float min_size_1)
{
    // FIXME-DOGFOODING: This needs further refining.
    // FIXME-SCROLL: When resizing either we'd like to keep scroll focus on something (e.g. last clicked item for list, bottom for log)
    // See https://github.com/ocornut/imgui/issues/319
    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = g.Style;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (min_size_0 < 0)
        min_size_0 = ImGui::GetFrameHeight();
    if (min_size_1)
        min_size_1 = ImGui::GetFrameHeight();

    IM_ASSERT(axis == ImGuiAxis_X || axis == ImGuiAxis_Y);

    float& v_1 = *value_1;
    float& v_2 = *value_2;
    ImRect splitter_bb;
    const float avail = axis == ImGuiAxis_X ? ImGui::GetContentRegionAvail().x - style.ItemSpacing.x : ImGui::GetContentRegionAvail().y - style.ItemSpacing.y;
    if (anchor < 0)
    {
        v_2 = ImMax(avail - v_1, min_size_1);   // First split is constant size.
    }
    else if (anchor > 0)
    {
        v_1 = ImMax(avail - v_2, min_size_0);   // Second split is constant size.
    }
    else
    {
        float r = v_1 / (v_1 + v_2);            // Both splits maintain same relative size to parent.
        v_1 = IM_ROUND(avail * r) - 1;
        v_2 = IM_ROUND(avail * (1.0f - r)) - 1;
    }
    if (axis == ImGuiAxis_X)
    {
        float x = window->DC.CursorPos.x + v_1 + IM_ROUND(style.ItemSpacing.x * 0.5f);
        splitter_bb = ImRect(x - 1, window->WorkRect.Min.y, x + 1, window->WorkRect.Max.y);
    }
    else if (axis == ImGuiAxis_Y)
    {
        float y = window->DC.CursorPos.y + v_1 + IM_ROUND(style.ItemSpacing.y * 0.5f);
        splitter_bb = ImRect(window->WorkRect.Min.x, y - 1, window->WorkRect.Max.x, y + 1);
    }
    return ImGui::SplitterBehavior(splitter_bb, ImGui::GetID(id), (ImGuiAxis)axis, &v_1, &v_2, min_size_0, min_size_1, 3.0f);
}

// FIXME-TESTS: Should eventually remove.
ImFont* ImGui::FindFontByPrefix(const char* prefix)
{
    ImGuiContext& g = *GImGui;
    for (ImFont* font : g.IO.Fonts->Fonts)
#if IMGUI_VERSION_NUM < 19184
        if (strncmp(font->ConfigData->Name, prefix, strlen(prefix)) == 0)
#elif defined(IMGUI_HAS_TEXTURES)
        if (strncmp(font->Sources[0]->Name, prefix, strlen(prefix)) == 0)
#else
        if (strncmp(font->Sources[0].Name, prefix, strlen(prefix)) == 0)
#endif
            return font;
    return nullptr;
}

// Legacy version support
#if IMGUI_VERSION_NUM < 18924
const char* ImGui::TabBarGetTabName(ImGuiTabBar* tab_bar, ImGuiTabItem* tab)
{
    return tab_bar->GetTabName(tab);
}
#endif

#if IMGUI_VERSION_NUM < 18927
ImGuiID ImGui::TableGetInstanceID(ImGuiTable* table, int instance_no)
{
    // Changed in #6140
    return table->ID + instance_no;
}
#endif

ImGuiID TableGetHeaderID(ImGuiTable* table, const char* column, int instance_no)
{
    IM_ASSERT(table != nullptr);
    int column_n = -1;
    for (int n = 0; n < table->Columns.size() && column_n < 0; n++)
        if (strcmp(ImGui::TableGetColumnName(table, n), column) == 0)
            column_n = n;
    IM_ASSERT(column_n != -1);
    return TableGetHeaderID(table, column_n, instance_no);
}

ImGuiID TableGetHeaderID(ImGuiTable* table, int column_n, int instance_no)
{
    IM_ASSERT(column_n >= 0 && column_n < table->ColumnsCount);
    const ImGuiID table_instance_id = ImGui::TableGetInstanceID(table, instance_no);
    const char* column_name = ImGui::TableGetColumnName(table, column_n);
#if IMGUI_VERSION_NUM >= 18927
    const int column_id_differencier = column_n;
#else
    const int column_id_differencier = instance_no * table->ColumnsCount + column_n;
#endif
    const int column_id = ImHashData(&column_id_differencier, sizeof(column_id_differencier), table_instance_id);
    return ImHashData(column_name, strlen(column_name), column_id);
}

// FIXME: Could be moved to core as an internal function?
void TableDiscardInstanceAndSettings(ImGuiID table_id)
{
    ImGuiContext& g = *GImGui;
    IM_ASSERT(g.CurrentTable == nullptr);
    if (ImGuiTableSettings* settings = ImGui::TableSettingsFindByID(table_id))
        settings->ID = 0;

    if (ImGuiTable* table = ImGui::TableFindByID(table_id))
        ImGui::TableRemove(table);
    // FIXME-TABLE: We should be able to use TableResetSettings() instead of TableRemove()! Maybe less of a clean slate but would be good to check that it does the job
    //ImGui::TableResetSettings(table);
}

// Helper to verify ImDrawData integrity of buffer count (broke before e.g. #6716)
void DrawDataVerifyMatchingBufferCount(ImDrawData* draw_data)
{
#if IMGUI_VERSION_NUM >= 18973
    int total_vtx_count = 0;
    int total_idx_count = 0;
    for (ImDrawList* draw_list : draw_data->CmdLists)
    {
        total_vtx_count += draw_list->VtxBuffer.Size;
        total_idx_count += draw_list->IdxBuffer.Size;
    }
    IM_UNUSED(total_vtx_count);
    IM_UNUSED(total_idx_count);
    IM_ASSERT(total_vtx_count == draw_data->TotalVtxCount);
    IM_ASSERT(total_idx_count == draw_data->TotalIdxCount);
#else
    IM_UNUSED(draw_data);
#endif
}

//-----------------------------------------------------------------------------
// Simple CSV parser
//-----------------------------------------------------------------------------

void ImGuiCsvParser::Clear()
{
    Rows = Columns = 0;
    if (_Data != nullptr)
        IM_FREE(_Data);
    _Data = nullptr;
    _Index.clear();
}

bool ImGuiCsvParser::Load(const char* filename)
{
    size_t len = 0;
    _Data = (char*)ImFileLoadToMemory(filename, "rb", &len, 1);
    if (_Data == nullptr)
        return false;

    int columns = 1;
    if (Columns > 0)
    {
        columns = Columns;                                          // User-provided expected column count.
    }
    else
    {
        for (const char* c = _Data; *c != '\n' && *c != '\0'; c++)  // Count columns. Quoted columns with commas are not supported.
            if (*c == ',')
                columns++;
    }

    // Count rows. Extra new lines anywhere in the file are ignored.
    int max_rows = 0;
    for (const char* c = _Data, *end = c + len; c < end; c++)
        if ((*c == '\n' && c[1] != '\r' && c[1] != '\n') || *c == '\0')
            max_rows++;

    if (columns == 0 || max_rows == 0)
        return false;

    // Create index
    _Index.resize(columns * max_rows);

    int col = 0;
    char* col_data = _Data;
    for (char* c = _Data; *c != '\0'; c++)
    {
        const bool is_comma = (*c == ',');
        const bool is_eol = (*c == '\n' || *c == '\r');
        const bool is_eof = (*c == '\0');
        if (is_comma || is_eol || is_eof)
        {
            _Index[Rows * columns + col] = col_data;
            col_data = c + 1;
            if (is_comma)
            {
                col++;
            }
            else
            {
                if (col + 1 == columns)
                    Rows++;
                else
                    fprintf(stderr, "%s: Unexpected number of columns on line %d, ignoring.\n", filename, Rows + 1); // FIXME
                col = 0;
            }
            *c = 0;
            if (is_eol)
                while (c[1] == '\r' || c[1] == '\n')
                    c++;
        }
    }

    Columns = columns;
    return true;
}

//-----------------------------------------------------------------------------
