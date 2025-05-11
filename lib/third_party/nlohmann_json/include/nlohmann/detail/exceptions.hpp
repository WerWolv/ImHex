//     __ _____ _____ _____
//  __|  |   __|     |   | |  JSON for Modern C++
// |  |  |__   |  |  | | | |  version 3.12.0
// |_____|_____|_____|_|___|  https://github.com/nlohmann/json
//
// SPDX-FileCopyrightText: 2013 - 2025 Niels Lohmann <https://nlohmann.me>
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef> // nullptr_t
#include <exception> // exception
#if JSON_DIAGNOSTICS
    #include <numeric> // accumulate
#endif
#include <stdexcept> // runtime_error
#include <string> // to_string
#include <vector> // vector

#include <nlohmann/detail/value_t.hpp>
#include <nlohmann/detail/string_escape.hpp>
#include <nlohmann/detail/input/position_t.hpp>
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/detail/meta/cpp_future.hpp>
#include <nlohmann/detail/meta/type_traits.hpp>
#include <nlohmann/detail/string_concat.hpp>

// With -Wweak-vtables, Clang will complain about the exception classes as they
// have no out-of-line virtual method definitions and their vtable will be
// emitted in every translation unit. This issue cannot be fixed with a
// header-only library as there is no implementation file to move these
// functions to. As a result, we suppress this warning here to avoid client
// code to stumble over this. See https://github.com/nlohmann/json/issues/4087
// for a discussion.
#if defined(__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wweak-vtables"
#endif

NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{

////////////////
// exceptions //
////////////////

/// @brief general exception of the @ref basic_json class
/// @sa https://json.nlohmann.me/api/basic_json/exception/
class exception : public std::exception
{
  public:
    /// returns the explanatory string
    const char* what() const noexcept override
    {
        return m.what();
    }

    /// the id of the exception
    const int id; // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

  protected:
    JSON_HEDLEY_NON_NULL(3)
    exception(int id_, const char* what_arg) : id(id_), m(what_arg) {} // NOLINT(bugprone-throw-keyword-missing)

    static std::string name(const std::string& ename, int id_)
    {
        return concat("[json.exception.", ename, '.', std::to_string(id_), "] ");
    }

    static std::string diagnostics(std::nullptr_t /*leaf_element*/)
    {
        return "";
    }

    template<typename BasicJsonType>
    static std::string diagnostics(const BasicJsonType* leaf_element)
    {
#if JSON_DIAGNOSTICS
        std::vector<std::string> tokens;
        for (const auto* current = leaf_element; current != nullptr && current->m_parent != nullptr; current = current->m_parent)
        {
            switch (current->m_parent->type())
            {
                case value_t::array:
                {
                    for (std::size_t i = 0; i < current->m_parent->m_data.m_value.array->size(); ++i)
                    {
                        if (&current->m_parent->m_data.m_value.array->operator[](i) == current)
                        {
                            tokens.emplace_back(std::to_string(i));
                            break;
                        }
                    }
                    break;
                }

                case value_t::object:
                {
                    for (const auto& element : *current->m_parent->m_data.m_value.object)
                    {
                        if (&element.second == current)
                        {
                            tokens.emplace_back(element.first.c_str());
                            break;
                        }
                    }
                    break;
                }

                case value_t::null: // LCOV_EXCL_LINE
                case value_t::string: // LCOV_EXCL_LINE
                case value_t::boolean: // LCOV_EXCL_LINE
                case value_t::number_integer: // LCOV_EXCL_LINE
                case value_t::number_unsigned: // LCOV_EXCL_LINE
                case value_t::number_float: // LCOV_EXCL_LINE
                case value_t::binary: // LCOV_EXCL_LINE
                case value_t::discarded: // LCOV_EXCL_LINE
                default:   // LCOV_EXCL_LINE
                    break; // LCOV_EXCL_LINE
            }
        }

        if (tokens.empty())
        {
            return "";
        }

        auto str = std::accumulate(tokens.rbegin(), tokens.rend(), std::string{},
                                   [](const std::string & a, const std::string & b)
        {
            return concat(a, '/', detail::escape(b));
        });

        return concat('(', str, ") ", get_byte_positions(leaf_element));
#else
        return get_byte_positions(leaf_element);
#endif
    }

  private:
    /// an exception object as storage for error messages
    std::runtime_error m;
#if JSON_DIAGNOSTIC_POSITIONS
    template<typename BasicJsonType>
    static std::string get_byte_positions(const BasicJsonType* leaf_element)
    {
        if ((leaf_element->start_pos() != std::string::npos) && (leaf_element->end_pos() != std::string::npos))
        {
            return concat("(bytes ", std::to_string(leaf_element->start_pos()), "-", std::to_string(leaf_element->end_pos()), ") ");
        }
        return "";
    }
#else
    template<typename BasicJsonType>
    static std::string get_byte_positions(const BasicJsonType* leaf_element)
    {
        static_cast<void>(leaf_element);
        return "";
    }
#endif
};

/// @brief exception indicating a parse error
/// @sa https://json.nlohmann.me/api/basic_json/parse_error/
class parse_error : public exception
{
  public:
    /*!
    @brief create a parse error exception
    @param[in] id_       the id of the exception
    @param[in] pos       the position where the error occurred (or with
                         chars_read_total=0 if the position cannot be
                         determined)
    @param[in] what_arg  the explanatory string
    @return parse_error object
    */
    template<typename BasicJsonContext, enable_if_t<is_basic_json_context<BasicJsonContext>::value, int> = 0>
    static parse_error create(int id_, const position_t& pos, const std::string& what_arg, BasicJsonContext context)
    {
        const std::string w = concat(exception::name("parse_error", id_), "parse error",
                                     position_string(pos), ": ", exception::diagnostics(context), what_arg);
        return {id_, pos.chars_read_total, w.c_str()};
    }

    template<typename BasicJsonContext, enable_if_t<is_basic_json_context<BasicJsonContext>::value, int> = 0>
    static parse_error create(int id_, std::size_t byte_, const std::string& what_arg, BasicJsonContext context)
    {
        const std::string w = concat(exception::name("parse_error", id_), "parse error",
                                     (byte_ != 0 ? (concat(" at byte ", std::to_string(byte_))) : ""),
                                     ": ", exception::diagnostics(context), what_arg);
        return {id_, byte_, w.c_str()};
    }

    /*!
    @brief byte index of the parse error

    The byte index of the last read character in the input file.

    @note For an input with n bytes, 1 is the index of the first character and
          n+1 is the index of the terminating null byte or the end of file.
          This also holds true when reading a byte vector (CBOR or MessagePack).
    */
    const std::size_t byte;

  private:
    parse_error(int id_, std::size_t byte_, const char* what_arg)
        : exception(id_, what_arg), byte(byte_) {}

    static std::string position_string(const position_t& pos)
    {
        return concat(" at line ", std::to_string(pos.lines_read + 1),
                      ", column ", std::to_string(pos.chars_read_current_line));
    }
};

/// @brief exception indicating errors with iterators
/// @sa https://json.nlohmann.me/api/basic_json/invalid_iterator/
class invalid_iterator : public exception
{
  public:
    template<typename BasicJsonContext, enable_if_t<is_basic_json_context<BasicJsonContext>::value, int> = 0>
    static invalid_iterator create(int id_, const std::string& what_arg, BasicJsonContext context)
    {
        const std::string w = concat(exception::name("invalid_iterator", id_), exception::diagnostics(context), what_arg);
        return {id_, w.c_str()};
    }

  private:
    JSON_HEDLEY_NON_NULL(3)
    invalid_iterator(int id_, const char* what_arg)
        : exception(id_, what_arg) {}
};

/// @brief exception indicating executing a member function with a wrong type
/// @sa https://json.nlohmann.me/api/basic_json/type_error/
class type_error : public exception
{
  public:
    template<typename BasicJsonContext, enable_if_t<is_basic_json_context<BasicJsonContext>::value, int> = 0>
    static type_error create(int id_, const std::string& what_arg, BasicJsonContext context)
    {
        const std::string w = concat(exception::name("type_error", id_), exception::diagnostics(context), what_arg);
        return {id_, w.c_str()};
    }

  private:
    JSON_HEDLEY_NON_NULL(3)
    type_error(int id_, const char* what_arg) : exception(id_, what_arg) {}
};

/// @brief exception indicating access out of the defined range
/// @sa https://json.nlohmann.me/api/basic_json/out_of_range/
class out_of_range : public exception
{
  public:
    template<typename BasicJsonContext, enable_if_t<is_basic_json_context<BasicJsonContext>::value, int> = 0>
    static out_of_range create(int id_, const std::string& what_arg, BasicJsonContext context)
    {
        const std::string w = concat(exception::name("out_of_range", id_), exception::diagnostics(context), what_arg);
        return {id_, w.c_str()};
    }

  private:
    JSON_HEDLEY_NON_NULL(3)
    out_of_range(int id_, const char* what_arg) : exception(id_, what_arg) {}
};

/// @brief exception indicating other library errors
/// @sa https://json.nlohmann.me/api/basic_json/other_error/
class other_error : public exception
{
  public:
    template<typename BasicJsonContext, enable_if_t<is_basic_json_context<BasicJsonContext>::value, int> = 0>
    static other_error create(int id_, const std::string& what_arg, BasicJsonContext context)
    {
        const std::string w = concat(exception::name("other_error", id_), exception::diagnostics(context), what_arg);
        return {id_, w.c_str()};
    }

  private:
    JSON_HEDLEY_NON_NULL(3)
    other_error(int id_, const char* what_arg) : exception(id_, what_arg) {}
};

}  // namespace detail
NLOHMANN_JSON_NAMESPACE_END

#if defined(__clang__)
    #pragma clang diagnostic pop
#endif
