// Str v0.33
// Simple C++ string type with an optional local buffer, by Omar Cornut
// https://github.com/ocornut/str

// LICENSE
//  This software is in the public domain. Where that dedication is not
//  recognized, you are granted a perpetual, irrevocable license to copy,
//  distribute, and modify this file as you see fit.

// USAGE
//  Include this file in whatever places need to refer to it.
//  In ONE .cpp file, write '#define STR_IMPLEMENTATION' before the #include of this file.
//  This expands out the actual implementation into that C/C++ file.


/*
- This isn't a fully featured string class.
- It is a simple, bearable replacement to std::string that isn't heap abusive nor bloated (can actually be debugged by humans).
- String are mutable. We don't maintain size so length() is not-constant time.
- Maximum string size currently limited to 2 MB (we allocate 21 bits to hold capacity).
- Local buffer size is currently limited to 1023 bytes (we allocate 10 bits to hold local buffer size).
- In "non-owned" mode for literals/reference we don't do any tracking/counting of references.
- Overhead is 8-bytes in 32-bits, 16-bytes in 64-bits (12 + alignment).
- This code hasn't been tested very much. it is probably incomplete or broken. Made it for my own use.

The idea is that you can provide an arbitrary sized local buffer if you expect string to fit
most of the time, and then you avoid using costly heap.

No local buffer, always use heap, sizeof()==8~16 (depends if your pointers are 32-bits or 64-bits)

   Str s = "hey";

With a local buffer of 16 bytes, sizeof() == 8~16 + 16 bytes.

   Str16 s = "filename.h"; // copy into local buffer
   Str16 s = "long_filename_not_very_long_but_longer_than_expected.h";   // use heap

With a local buffer of 256 bytes, sizeof() == 8~16 + 256 bytes.

   Str256 s = "long_filename_not_very_long_but_longer_than_expected.h";  // copy into local buffer

Common sizes are defined at the bottom of Str.h, you may define your own.

Functions:

   Str256 s;
   s.set("hello sailor");                   // set (copy)
   s.setf("%s/%s.tmp", folder, filename);   // set (w/format)
   s.append("hello");                       // append. cost a length() calculation!
   s.appendf("hello %d", 42);               // append (w/format). cost a length() calculation!
   s.set_ref("Hey!");                       // set (literal/reference, just copy pointer, no tracking)

Constructor helper for format string: add a trailing 'f' to the type. Underlying type is the same.

   Str256f filename("%s/%s.tmp", folder, filename);             // construct (w/format)
   fopen(Str256f("%s/%s.tmp, folder, filename).c_str(), "rb");  // construct (w/format), use as function param, destruct

Constructor helper for reference/literal:

   StrRef ref("literal");                   // copy pointer, no allocation, no string copy
   StrRef ref2(GetDebugName());             // copy pointer. no tracking of anything whatsoever, know what you are doing!

All StrXXX types derives from Str and instance hold the local buffer capacity. So you can pass e.g. Str256* to a function taking base type Str* and it will be functional.

   void MyFunc(Str& s) { s = "Hello"; }     // will use local buffer if available in Str instance

(Using a template e.g. Str<N> we could remove the LocalBufSize storage but it would make passing typed Str<> to functions tricky.
 Instead we don't use template so you can pass them around as the base type Str*. Also, templates are ugly.)
*/

/*
 CHANGELOG
  0.33 - fixed capacity() return value to match standard. e.g. a Str256's capacity() now returns 255, not 256.
  0.32 - added owned() accessor.
  0.31 - fixed various warnings.
  0.30 - turned into a single header file, removed Str.cpp.
  0.29 - fixed bug when calling reserve on non-owned strings (ie. when using StrRef or set_ref), and fixed <string> include.
  0.28 - breaking change: replaced Str32 by Str30 to avoid collision with Str32 from MacTypes.h .
  0.27 - added STR_API and basic .natvis file.
  0.26 - fixed set(cont char* src, const char* src_end) writing null terminator to the wrong position.
  0.25 - allow set(const char* NULL) or operator= NULL to clear the string. note that set() from range or other types are not allowed.
  0.24 - allow set_ref(const char* NULL) to clear the string. include fixes for linux.
  0.23 - added append(char). added append_from(int idx, XXX) functions. fixed some compilers warnings.
  0.22 - documentation improvements, comments. fixes for some compilers.
  0.21 - added StrXXXf() constructor to construct directly from a format string.
*/

/*
TODO
- Since we lose 4-bytes of padding on 64-bits architecture, perhaps just spread the header to 8-bytes and lift size limits?
- More functions/helpers.
*/

#ifndef STR_INCLUDED
#define STR_INCLUDED

//-------------------------------------------------------------------------
// CONFIGURATION
//-------------------------------------------------------------------------

#ifndef STR_MEMALLOC
#define STR_MEMALLOC  malloc
#include <stdlib.h>
#endif
#ifndef STR_MEMFREE
#define STR_MEMFREE   free
#include <stdlib.h>
#endif
#ifndef STR_ASSERT
#define STR_ASSERT    assert
#include <assert.h>
#endif
#ifndef STR_API
#define STR_API
#endif
#include <stdarg.h>   // for va_list
#include <string.h>   // for strlen, strcmp, memcpy, etc.

// Configuration: #define STR_SUPPORT_STD_STRING 0 to disable setters variants using const std::string& (on by default)
#ifndef STR_SUPPORT_STD_STRING
#define STR_SUPPORT_STD_STRING  1
#endif

// Configuration: #define STR_DEFINE_STR32 1 to keep defining Str32/Str32f, but be warned: on macOS/iOS, MacTypes.h also defines a type named Str32.
#ifndef STR_DEFINE_STR32
#define STR_DEFINE_STR32 0
#endif

#if STR_SUPPORT_STD_STRING
#include <string>
#endif

//-------------------------------------------------------------------------
// HEADERS
//-------------------------------------------------------------------------

// This is the base class that you can pass around
// Footprint is 8-bytes (32-bits arch) or 16-bytes (64-bits arch)
class STR_API Str
{
    char*               Data;                   // Point to LocalBuf() or heap allocated
    int                 Capacity : 21;          // Max 2 MB. Exclude zero terminator.
    int                 LocalBufSize : 10;      // Max 1023 bytes
    unsigned int        Owned : 1;              // Set when we have ownership of the pointed data (most common, unless using set_ref() method or StrRef constructor)

public:
    inline char*        c_str()                                 { return Data; }
    inline const char*  c_str() const                           { return Data; }
    inline bool         empty() const                           { return Data[0] == 0; }
    inline int          length() const                          { return (int)strlen(Data); }    // by design, allow user to write into the buffer at any time
    inline int          capacity() const                        { return Capacity; }
    inline bool         owned() const                           { return Owned ? true : false; }

    inline void         set_ref(const char* src);
    int                 setf(const char* fmt, ...);
    int                 setfv(const char* fmt, va_list args);
    int                 setf_nogrow(const char* fmt, ...);
    int                 setfv_nogrow(const char* fmt, va_list args);
    int                 append(char c);
    int                 append(const char* s, const char* s_end = NULL);
    int                 appendf(const char* fmt, ...);
    int                 appendfv(const char* fmt, va_list args);
    int                 append_from(int idx, char c);
    int                 append_from(int idx, const char* s, const char* s_end = NULL);		// If you know the string length or want to append from a certain point
    int                 appendf_from(int idx, const char* fmt, ...);
    int                 appendfv_from(int idx, const char* fmt, va_list args);

    void                clear();
    void                reserve(int cap);
    void                reserve_discard(int cap);
    void                shrink_to_fit();

    inline char&        operator[](size_t i)                    { return Data[i]; }
    inline char         operator[](size_t i) const              { return Data[i]; }
    //explicit operator const char*() const{ return Data; }

    inline Str();
    inline Str(const char* rhs);
    inline void         set(const char* src);
    inline void         set(const char* src, const char* src_end);
    inline Str&         operator=(const char* rhs)              { set(rhs); return *this; }
    inline bool         operator==(const char* rhs) const       { return strcmp(c_str(), rhs) == 0; }

    inline Str(const Str& rhs);
    inline void         set(const Str& src);
    inline Str&         operator=(const Str& rhs)               { set(rhs); return *this; }
    inline bool         operator==(const Str& rhs) const        { return strcmp(c_str(), rhs.c_str()) == 0; }

#if STR_SUPPORT_STD_STRING
    inline Str(const std::string& rhs);
    inline void         set(const std::string& src);
    inline Str&         operator=(const std::string& rhs)       { set(rhs); return *this; }
    inline bool         operator==(const std::string& rhs)const { return strcmp(c_str(), rhs.c_str()) == 0; }
#endif

    // Destructor for all variants
    inline ~Str()
    {
        if (Owned && !is_using_local_buf())
            STR_MEMFREE(Data);
    }

    static char*        EmptyBuffer;

protected:
    inline char*        local_buf()                             { return (char*)this + sizeof(Str); }
    inline const char*  local_buf() const                       { return (char*)this + sizeof(Str); }
    inline bool         is_using_local_buf() const              { return Data == local_buf() && LocalBufSize != 0; }

    // Constructor for StrXXX variants with local buffer
    Str(unsigned short local_buf_size)
    {
        STR_ASSERT(local_buf_size < 1024);
        Data = local_buf();
        Data[0] = '\0';
        Capacity = local_buf_size ? local_buf_size - 1 : 0;
        LocalBufSize = local_buf_size;
        Owned = 1;
    }
};

void    Str::set(const char* src)
{
    // We allow set(NULL) or via = operator to clear the string.
    if (src == NULL)
    {
        clear();
        return;
    }
    int buf_len = (int)strlen(src);
    if (Capacity < buf_len)
        reserve_discard(buf_len);
    memcpy(Data, src, (size_t)(buf_len + 1));
    Owned = 1;
}

void    Str::set(const char* src, const char* src_end)
{
    STR_ASSERT(src != NULL && src_end >= src);
    int buf_len = (int)(src_end - src);
    if ((int)Capacity < buf_len)
        reserve_discard(buf_len);
    memcpy(Data, src, (size_t)buf_len);
    Data[buf_len] = 0;
    Owned = 1;
}

void    Str::set(const Str& src)
{
    int buf_len = (int)strlen(src.c_str());
    if ((int)Capacity < buf_len)
        reserve_discard(buf_len);
    memcpy(Data, src.c_str(), (size_t)(buf_len + 1));
    Owned = 1;
}

#if STR_SUPPORT_STD_STRING
void    Str::set(const std::string& src)
{
    int buf_len = (int)src.length();
    if ((int)Capacity < buf_len)
        reserve_discard(buf_len);
    memcpy(Data, src.c_str(), (size_t)(buf_len + 1));
    Owned = 1;
}
#endif

inline void Str::set_ref(const char* src)
{
    if (Owned && !is_using_local_buf())
        STR_MEMFREE(Data);
    Data = src ? (char*)src : EmptyBuffer;
    Capacity = 0;
    Owned = 0;
}

Str::Str()
{
    Data = EmptyBuffer;      // Shared READ-ONLY initial buffer for 0 capacity
    Capacity = 0;
    LocalBufSize = 0;
    Owned = 0;
}

Str::Str(const Str& rhs) : Str()
{
    set(rhs);
}

Str::Str(const char* rhs) : Str()
{
    set(rhs);
}

#if STR_SUPPORT_STD_STRING
Str::Str(const std::string& rhs) : Str()
{
    set(rhs);
}
#endif

// Literal/reference string
class StrRef : public Str
{
public:
    StrRef(const char* s) : Str() { set_ref(s); }
};

// Types embedding a local buffer
// NB: we need to override the constructor and = operator for both Str& and TYPENAME (without the later compiler will call a default copy operator)
#if STR_SUPPORT_STD_STRING

#define STR_DEFINETYPE(TYPENAME, LOCALBUFSIZE)                                      \
class TYPENAME : public Str                                                         \
{                                                                                   \
    char local_buf[LOCALBUFSIZE];                                                   \
public:                                                                             \
    TYPENAME() : Str(LOCALBUFSIZE) {}                                               \
    TYPENAME(const Str& rhs) : Str(LOCALBUFSIZE) { set(rhs); }                      \
    TYPENAME(const char* rhs) : Str(LOCALBUFSIZE) { set(rhs); }                     \
    TYPENAME(const TYPENAME& rhs) : Str(LOCALBUFSIZE) { set(rhs); }                 \
    TYPENAME(const std::string& rhs) : Str(LOCALBUFSIZE) { set(rhs); }              \
    TYPENAME&   operator=(const char* rhs)          { set(rhs); return *this; }     \
    TYPENAME&   operator=(const Str& rhs)           { set(rhs); return *this; }     \
    TYPENAME&   operator=(const TYPENAME& rhs)      { set(rhs); return *this; }     \
    TYPENAME&   operator=(const std::string& rhs)   { set(rhs); return *this; }     \
};

#else

#define STR_DEFINETYPE(TYPENAME, LOCALBUFSIZE)                                      \
class TYPENAME : public Str                                                         \
{                                                                                   \
    char local_buf[LOCALBUFSIZE];                                                   \
public:                                                                             \
    TYPENAME() : Str(LOCALBUFSIZE) {}                                               \
    TYPENAME(const Str& rhs) : Str(LOCALBUFSIZE) { set(rhs); }                      \
    TYPENAME(const char* rhs) : Str(LOCALBUFSIZE) { set(rhs); }                     \
    TYPENAME(const TYPENAME& rhs) : Str(LOCALBUFSIZE) { set(rhs); }                 \
    TYPENAME&   operator=(const char* rhs)          { set(rhs); return *this; }     \
    TYPENAME&   operator=(const Str& rhs)           { set(rhs); return *this; }     \
    TYPENAME&   operator=(const TYPENAME& rhs)      { set(rhs); return *this; }     \
};

#endif

// Disable PVS-Studio warning V730: Not all members of a class are initialized inside the constructor (local_buf is not initialized and that is fine)
// -V:STR_DEFINETYPE:730

// Helper to define StrXXXf constructors
#define STR_DEFINETYPE_F(TYPENAME, TYPENAME_F)                                      \
class TYPENAME_F : public TYPENAME                                                  \
{                                                                                   \
public:                                                                             \
    TYPENAME_F(const char* fmt, ...) : TYPENAME() { va_list args; va_start(args, fmt); setfv(fmt, args); va_end(args); } \
};

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"         // warning : private field 'local_buf' is not used
#endif

// Declaring types for common sizes here
STR_DEFINETYPE(Str16, 16)
STR_DEFINETYPE(Str30, 30)
STR_DEFINETYPE(Str64, 64)
STR_DEFINETYPE(Str128, 128)
STR_DEFINETYPE(Str256, 256)
STR_DEFINETYPE(Str512, 512)

// Declaring helper constructors to pass in format strings in one statement
STR_DEFINETYPE_F(Str16, Str16f)
STR_DEFINETYPE_F(Str30, Str30f)
STR_DEFINETYPE_F(Str64, Str64f)
STR_DEFINETYPE_F(Str128, Str128f)
STR_DEFINETYPE_F(Str256, Str256f)
STR_DEFINETYPE_F(Str512, Str512f)

#if STR_DEFINE_STR32
STR_DEFINETYPE(Str32, 32)
STR_DEFINETYPE_F(Str32, Str32f)
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif // #ifndef STR_INCLUDED

//-------------------------------------------------------------------------
// IMPLEMENTATION
//-------------------------------------------------------------------------

#ifdef STR_IMPLEMENTATION

#include <stdio.h> // for vsnprintf

// On some platform vsnprintf() takes va_list by reference and modifies it.
// va_copy is the 'correct' way to copy a va_list but Visual Studio prior to 2013 doesn't have it.
#ifndef va_copy
#define va_copy(dest, src) (dest = src)
#endif

// Static empty buffer we can point to for empty strings
// Pointing to a literal increases the like-hood of getting a crash if someone attempts to write in the empty string buffer.
char*   Str::EmptyBuffer = (char*)"\0NULL";

// Clear
void    Str::clear()
{
    if (Owned && !is_using_local_buf())
        STR_MEMFREE(Data);
    if (LocalBufSize)
    {
        Data = local_buf();
        Data[0] = '\0';
        Capacity = LocalBufSize - 1;
        Owned = 1;
    }
    else
    {
        Data = EmptyBuffer;
        Capacity = 0;
        Owned = 0;
    }
}

// Reserve memory, preserving the current of the buffer
// Capacity doesn't include the zero terminator, so reserve(5) is enough to store "hello".
void    Str::reserve(int new_capacity)
{
    if (new_capacity <= Capacity)
        return;

    char* new_data;
    if (new_capacity <= LocalBufSize - 1)
    {
        // Disowned -> LocalBuf
        new_data = local_buf();
        new_capacity = LocalBufSize - 1;
    }
    else
    {
        // Disowned or LocalBuf -> Heap
        new_data = (char*)STR_MEMALLOC((size_t)(new_capacity + 1) * sizeof(char));
    }

    // string in Data might be longer than new_capacity if it wasn't owned, don't copy too much
#ifdef _MSC_VER
    strncpy_s(new_data, (size_t)new_capacity + 1, Data, (size_t)new_capacity);
#else
    strncpy(new_data, Data, (size_t)new_capacity);
#endif
    new_data[new_capacity] = 0;

    if (Owned && !is_using_local_buf())
        STR_MEMFREE(Data);

    Data = new_data;
    Capacity = new_capacity;
    Owned = 1;
}

// Reserve memory, discarding the current of the buffer (if we expect to be fully rewritten)
void    Str::reserve_discard(int new_capacity)
{
    if (new_capacity <= Capacity)
        return;

    if (Owned && !is_using_local_buf())
        STR_MEMFREE(Data);

    if (new_capacity <= LocalBufSize - 1)
    {
        // Disowned -> LocalBuf
        Data = local_buf();
        Capacity = LocalBufSize - 1;
    }
    else
    {
        // Disowned or LocalBuf -> Heap
        Data = (char*)STR_MEMALLOC((size_t)(new_capacity + 1) * sizeof(char));
        Capacity = new_capacity;
    }
    Owned = 1;
}

void    Str::shrink_to_fit()
{
    if (!Owned || is_using_local_buf())
        return;
    int new_capacity = length();
    if (Capacity <= new_capacity)
        return;

    char* new_data = (char*)STR_MEMALLOC((size_t)(new_capacity + 1) * sizeof(char));
    memcpy(new_data, Data, (size_t)(new_capacity + 1));
    STR_MEMFREE(Data);
    Data = new_data;
    Capacity = new_capacity;
}

// FIXME: merge setfv() and appendfv()?
int     Str::setfv(const char* fmt, va_list args)
{
    // Needed for portability on platforms where va_list are passed by reference and modified by functions
    va_list args2;
    va_copy(args2, args);

    // MSVC returns -1 on overflow when writing, which forces us to do two passes
    // FIXME-OPT: Find a way around that.
#ifdef _MSC_VER
    int len = vsnprintf(NULL, 0, fmt, args);
    STR_ASSERT(len >= 0);

    if (Capacity < len)
        reserve_discard(len);
    len = vsnprintf(Data, (size_t)len + 1, fmt, args2);
#else
    // First try
    int len = vsnprintf(Owned ? Data : NULL, Owned ? (size_t)(Capacity + 1): 0, fmt, args);
    STR_ASSERT(len >= 0);

    if (Capacity < len)
    {
        reserve_discard(len);
        len = vsnprintf(Data, (size_t)len + 1, fmt, args2);
    }
#endif

    STR_ASSERT(Owned);
    return len;
}

int     Str::setf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = setfv(fmt, args);
    va_end(args);
    return len;
}

int     Str::setfv_nogrow(const char* fmt, va_list args)
{
    STR_ASSERT(Owned);

    if (Capacity == 0)
        return 0;

    int w = vsnprintf(Data, (size_t)(Capacity + 1), fmt, args);
    Data[Capacity] = 0;
    Owned = 1;
    return (w == -1) ? Capacity : w;
}

int     Str::setf_nogrow(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = setfv_nogrow(fmt, args);
    va_end(args);
    return len;
}

int     Str::append_from(int idx, char c)
{
    int add_len = 1;
    if (Capacity < idx + add_len)
        reserve(idx + add_len);
    Data[idx] = c;
    Data[idx + add_len] = 0;
    STR_ASSERT(Owned);
    return add_len;
}

int     Str::append_from(int idx, const char* s, const char* s_end)
{
    if (!s_end)
        s_end = s + strlen(s);
    int add_len = (int)(s_end - s);
    if (Capacity < idx + add_len)
        reserve(idx + add_len);
    memcpy(Data + idx, (const void*)s, (size_t)add_len);
    Data[idx + add_len] = 0; // Our source data isn't necessarily zero terminated
    STR_ASSERT(Owned);
    return add_len;
}

// FIXME: merge setfv() and appendfv()?
int     Str::appendfv_from(int idx, const char* fmt, va_list args)
{
    // Needed for portability on platforms where va_list are passed by reference and modified by functions
    va_list args2;
    va_copy(args2, args);

    // MSVC returns -1 on overflow when writing, which forces us to do two passes
    // FIXME-OPT: Find a way around that.
#ifdef _MSC_VER
    int add_len = vsnprintf(NULL, 0, fmt, args);
    STR_ASSERT(add_len >= 0);

    if (Capacity < idx + add_len)
        reserve(idx + add_len);
    add_len = vsnprintf(Data + idx, add_len + 1, fmt, args2);
#else
    // First try
    int add_len = vsnprintf(Owned ? Data + idx : NULL, Owned ? (size_t)(Capacity + 1 - idx) : 0, fmt, args);
    STR_ASSERT(add_len >= 0);

    if (Capacity < idx + add_len)
    {
        reserve(idx + add_len);
        add_len = vsnprintf(Data + idx, (size_t)add_len + 1, fmt, args2);
    }
#endif

    STR_ASSERT(Owned);
    return add_len;
}

int     Str::appendf_from(int idx, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = appendfv_from(idx, fmt, args);
    va_end(args);
    return len;
}

int     Str::append(char c)
{
    int cur_len = length();
    return append_from(cur_len, c);
}

int     Str::append(const char* s, const char* s_end)
{
    int cur_len = length();
    return append_from(cur_len, s, s_end);
}

int     Str::appendfv(const char* fmt, va_list args)
{
    int cur_len = length();
    return appendfv_from(cur_len, fmt, args);
}

int     Str::appendf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = appendfv(fmt, args);
    va_end(args);
    return len;
}

#endif // #define STR_IMPLEMENTATION

//-------------------------------------------------------------------------
