#ifndef CLARA_HPP
#define CLARA_HPP

#include <climits>
#include <format>
#include <regex>
#include <type_traits>
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <expected>
#include <functional>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include <sstream>
#include <string>
#include <string_view>


// Note: flags like --👨‍🚀 aren’t supported yet, but -🚀 works fine. 
// Why? the emoji 👨‍🚀 is a combination of two code points: 👨 (man) and 🚀 (rocket), 
// joined by a Zero-Width Joiner (ZWJ), and the current lexer/flag parser treats them as separate characters. 
// Same goes for accents like 'é' (e + accent) vs. 'é'. 
// For now, use single code point emoji like -🚀 or -é (precomposed). 
// Workaround: Use multi character emoji or accents as options (e.g., --man_rocket or --accented_e) 
// and optionally set an alias (e.g., -👨‍🚀 → --man_rocket or -é → --accented_e) for convenience.


// Clara will follow semantic versioning (https://semver.org/) starting at v0.1.0. 
// Until then, all updates are considered patches (e.g., v0.0.1, v0.0.2) as we refin 
// the API toward a stable foundation—expect changes!

#define CLARA_VERSION_MAJOR 0
#define CLARA_VERSION_MINOR 0
#define CLARA_VERSION_PATCH 1

#define CONCAT_V(mj, mi, pa) v_##mj_##mi_##pa

// using c++20 inline nested namespace extinsion.
#define CLARA_SET_NAMESPACE                                                    \
  namespace clara::inline CONCAT_V(CLARA_VERSION_MAJOR, CLARA_VERSION_MINOR,   \
                                   CLARA_VERSION_PATCH)

#define D_PRINT(x) std::cerr << x << "\n";


// I don't think we need this, but i'll keep it for now

#if defined(__linux__)
#if defined(__ANDROID__)
#define CLARA_ANDROID
#warning untested platform
#else
#define CLARA_LINUX
#endif // defined(__ANDROID__)
#elif defined(_WIN32)
#define CLARA_WINDOWS
#if defined(_WIN64)
#define CLARA_WINDOWS_64
#endif // defined(_WIN64)
#elif defined(__APPLE__) || defined(__MACH__)
#include <TargetConditionals.h>
#define CLARA_APPLE
#if TARGET_IPHONE_SIMULATOR == 1
#define CLARA_IPHONE_SIMULATOR
#warning untested platform "iphone simulator"
#elif TARGET_OS_IPHONE == 1
#define CLARA_IPHONE
#warning untested platform "iphone"
#elif TARGET_OS_MAC == 1
#define CLARA_MACOS
#warning untested platform "macos"
#else
#error unknown apple platform
#endif // TARGET_IPHONE_SIMULATOR == 1
#else
#define CLARA_PLATFORM_UNKNOWN
#error unknown platform utf-8 will be assumed
#endif // defined(__linux__)

#if not defined(CLARA_WINDOWS)

#define CLARA_UTF8 1

#else

#define CLARA_UTF8 0

#endif // not defined(CLARA_WINDOWS)

CLARA_SET_NAMESPACE
{
  namespace detail
  {
    inline std::string join_args(int argc, char** argv)
    {
      // seems rather unnatural but it's for the best!
      std::stringstream ss;
      for(auto i = 1; i < argc; i++) // no need for launch command name
      {
        ss << argv[i] << " ";
      }
      return ss.str();
    }
  } // namespace detail

  namespace utf8
  {
    using code_point = uint32_t;

    // constants for utf-8 bit patterns
    namespace detail
    {
      constexpr uint8_t ASCII_MASK = 0x80;
      constexpr uint8_t TWO_BYTE_MASK = 0xE0;
      constexpr uint8_t TWO_BYTE_SIG = 0xC0;
      constexpr uint8_t THREE_BYTE_MASK = 0xF0;
      constexpr uint8_t THREE_BYTE_SIG = 0xE0;
      constexpr uint8_t FOUR_BYTE_MASK = 0xF8;
      constexpr uint8_t FOUR_BYTE_SIG = 0xF0;
      constexpr uint8_t CONTINUATION_MASK = 0xC0;
      constexpr uint8_t CONTINUATION_SIG = 0x80;

      // get the expected byte count for a utf-8 sequence based on the first byte
      inline size_t get_sequence_length(uint8_t first_byte)
      {
        if((first_byte & ASCII_MASK) == 0)
          return 1;
        if((first_byte & TWO_BYTE_MASK) == TWO_BYTE_SIG)
          return 2;
        if((first_byte & THREE_BYTE_MASK) == THREE_BYTE_SIG)
          return 3;
        if((first_byte & FOUR_BYTE_MASK) == FOUR_BYTE_SIG)
          return 4;
        return 0; // invalid
      }

      // validate continuation bytes in a utf-8 sequence
      inline bool validate_continuation(std::string_view input, size_t pos,
                                        size_t count)
      {
        for(size_t i = 1; i < count; ++i)
        {
          if(pos + i >= input.size() || (static_cast<uint8_t>(input[pos + i]) &
                                         CONTINUATION_MASK) != CONTINUATION_SIG)
          {
            return false;
          }
        }
        return true;
      }
    } // namespace detail

    // check if the entire string is valid utf-8
    inline bool is_valid_utf8_v0(std::string_view input)
    {
      size_t pos = 0;
      while(pos < input.size())
      {
        uint8_t first_byte = static_cast<uint8_t>(input[pos]);
        size_t bytes = detail::get_sequence_length(first_byte);
        if(bytes == 0 || pos + bytes > input.size() ||
           !detail::validate_continuation(input, pos, bytes))
        {
          return false;
        }
        pos += bytes;
      }
      return true;
    }

    // advance position by one utf-8 character
    inline bool advance_one_char(std::string_view input, size_t& pos)
    {
      if(pos >= input.size())
        return false;

      uint8_t first_byte = static_cast<uint8_t>(input[pos]);
      size_t bytes = detail::get_sequence_length(first_byte);
      if(bytes == 0 || pos + bytes > input.size() ||
         !detail::validate_continuation(input, pos, bytes))
      {
        return false;
      }
      pos += bytes;
      return true;
    }

    // check and decode a single utf-8 code point at the current position
    inline std::optional<code_point> decode(std::string_view input, size_t& pos)
    {
      if(pos >= input.size())
      {
        return std::nullopt; // no data
      }

      uint8_t first_byte = static_cast<uint8_t>(input[pos]);
      size_t bytes = detail::get_sequence_length(first_byte);

      // invalid first byte or not enough bytes remaining
      if(bytes == 0 || pos + bytes > input.size())
      {
        return std::nullopt;
      }

      // validate continuation bytes
      if(bytes > 1 && !detail::validate_continuation(input, pos, bytes))
      {
        return std::nullopt;
      }

      // decode the code point
      code_point cp = 0;
      switch(bytes)
      {
        case 1:
          cp = first_byte;
          break;
        case 2:
          cp = ((first_byte & 0x1F) << 6) |
               (static_cast<uint8_t>(input[pos + 1]) & 0x3F);
          break;
        case 3:
          cp = ((first_byte & 0x0F) << 12) |
               ((static_cast<uint8_t>(input[pos + 1]) & 0x3F) << 6) |
               (static_cast<uint8_t>(input[pos + 2]) & 0x3F);
          break;
        case 4:
          cp = ((first_byte & 0x07) << 18) |
               ((static_cast<uint8_t>(input[pos + 1]) & 0x3F) << 12) |
               ((static_cast<uint8_t>(input[pos + 2]) & 0x3F) << 6) |
               (static_cast<uint8_t>(input[pos + 3]) & 0x3F);
          break;
      }

      // additional validation for overlong encodings and invalid ranges
      if((bytes == 2 && cp < 0x80) ||    // overlong 2 byte
         (bytes == 3 && cp < 0x800) ||   // overlong 3 byte
         (bytes == 4 && cp < 0x10000) || // overlong 4 byte
         (cp > 0x10FFFF) ||              // beyond unicode range
         (cp >= 0xD800 && cp <= 0xDFFF))
      { // surrogates
        return std::nullopt;
      }

      pos += bytes; // advance position
      return cp;
    }

    inline bool is_valid_utf8(std::string_view input)
    {
      size_t pos = 0;
      while(pos < input.size())
      {
        uint8_t first_byte = static_cast<uint8_t>(input[pos]);
        size_t bytes = detail::get_sequence_length(first_byte);
        if(bytes == 0 || pos + bytes > input.size() ||
           !detail::validate_continuation(input, pos, bytes))
        {
          return false;
        }
        pos += bytes;
      }
      return true;
    }

    // deprecated
    inline code_point decode_v0(std::string_view input, size_t& pos)
    {
      if(pos >= input.size())
        return 0;

      uint8_t byte = static_cast<uint8_t>(input[pos]);
      pos++;

      if(byte <= 0x7f) // ascii
        return byte;

      code_point cp = 0;
      int extra_bytes = 0;

      if((byte & 0xe0) == 0xc0) // 2 byte sequence
      {
        cp = byte & 0x1f;
        extra_bytes = 1;
      }
      else if((byte & 0xf0) == 0xe0) // 3 byte sequence
      {
        cp = byte & 0x0f;
        extra_bytes = 2;
      }
      else if((byte & 0xf8) == 0xf0) // 4 byte sequence
      {
        cp = byte & 0x07;
        extra_bytes = 3;
      }
      else
      {
        return U'?'; // invalid utf-8
      }

      for(auto i = 0; i < extra_bytes; i++)
      {
        if(pos >= input.size())
          return U'?'; // incomplete sequence
        byte = static_cast<uint8_t>(input[pos++]);
        if((byte & 0xc0) != 0x80)
          return U'?'; // invalid continuation byte
        cp = (cp << 6) | (byte & 0x3f);
      }
      return cp;
    }

    // encode a code point into utf-8
    inline std::string encode(code_point cp)
    {
      if(cp <= 0x7F)
        return {static_cast<char>(cp)};

      if(cp <= 0x7FF)
      {
        return {static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)),
                static_cast<char>(0x80 | (cp & 0x3F))};
      }

      if(cp <= 0xFFFF)
      {
        return {static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)),
                static_cast<char>(0x80 | ((cp >> 6) & 0x3F)),
                static_cast<char>(0x80 | (cp & 0x3F))};
      }

      if(cp <= 0x10FFFF)
      {
        return {static_cast<char>(0xF0 | ((cp >> 18) & 0x07)),
                static_cast<char>(0x80 | ((cp >> 12) & 0x3F)),
                static_cast<char>(0x80 | ((cp >> 6) & 0x3F)),
                static_cast<char>(0x80 | (cp & 0x3F))};
      }
      return "?"; // invalid code point
    }

    inline bool is_digit(code_point cp) { return cp >= U'0' && cp <= U'9'; }

    // check if a code point is a letter
    inline bool is_letter(code_point cp)
    {
      // ASCII letters and underscore (valid identifier start)
      if((cp >= U'a' && cp <= U'z') || (cp >= U'A' && cp <= U'Z') || cp == U'_')
      {
        return true;
      }

      // extended latin based alphabets
      if((cp >= 0x00C0 && cp <= 0x00FF) || // latin 1 supplement (accents)
         (cp >= 0x0100 && cp <= 0x017F) || // latin extended a
         (cp >= 0x0180 && cp <= 0x024F) || // latin extended b
         (cp >= 0x1E00 && cp <= 0x1EFF) || // latin extended additional
         (cp >= 0x2C60 && cp <= 0x2C7F))   // latin extended c
      {
        return true;
      }

      // other major writing systems
      if((cp >= 0x0370 && cp <= 0x03FF) || // greek and coptic
         (cp >= 0x0400 && cp <= 0x04FF) || // cyrillic
         (cp >= 0x0500 && cp <= 0x052F) || // cyrillic supplement
         (cp >= 0x0530 && cp <= 0x058F) || // armenian
         (cp >= 0x0590 && cp <= 0x05FF) || // hebrew
         (cp >= 0x0600 && cp <= 0x06FF) || // arabic
         (cp >= 0x0750 && cp <= 0x077F) || // arabic supplement
         (cp >= 0x08A0 && cp <= 0x08FF) || // arabic extended a
         (cp >= 0x0900 && cp <= 0x097F) || // devanagari
         (cp >= 0x0980 && cp <= 0x09FF))   // bengali
      {
        return true;
      }

      // arabic presentation forms
      if((cp >= 0xFB50 && cp <= 0xFDFF) || // forms a
         (cp >= 0xFE70 && cp <= 0xFEFF))   // forms b
      {
        return true;
      }

      // additional 
      if((cp >= 0x0A00 && cp <= 0x0A7F) || // gurmukhi
         (cp >= 0x0A80 && cp <= 0x0AFF) || // gujarati
         (cp >= 0x0B00 && cp <= 0x0B7F) || // oriya
         (cp >= 0x0B80 && cp <= 0x0BFF) || // tamil
         (cp >= 0x10A0 && cp <= 0x10FF) || // georgian
         (cp >= 0x1100 && cp <= 0x11FF))   // hangul jamo
      {
        return true;
      }

      // unknown
      return false;
    }

    // lookup table for emoji ranges
    struct unicode_range 
    {
      code_point start, end;
      bool contains(code_point cp) const 
      { 
        return cp >= start && cp <= end; 
      }
    };

    static const unicode_range emoji_ranges[] = 
    {
      { 0x1F300, 0x1F5FF }, // misc symbols and pictographs
      { 0x1F600, 0x1F64F }, // emoticons
      { 0x1F680, 0x1F6FF }, // transport and map symbols
      { 0x1F900, 0x1F9FF }, // supplemental symbols and pictographs
      { 0x1FA70, 0x1FAFF }, // symbols and pictographs extended a
      { 0x2600, 0x26FF   }, // miscellaneous symbols
      { 0x2700, 0x27BF   }, // dingbats
      { 0x2B50, 0x2B59   }, // stars and similar symbols
      { 0x1F000, 0x1F02F }, // mahjong tiles
      { 0x1F0A0, 0x1F0FF }, // playing cards
    };

    static const unicode_range modifier_ranges[] = 
    {
      { 0x1F3FB, 0x1F3FF }, // skin Tone modifiers
      { 0x1F1E6, 0x1F1FF }, // regional indicators (flags)
    };

    inline bool is_emoji(code_point cp)
    {
      for(const auto& range : emoji_ranges)
      {
        if(range.contains(cp))
          return true;
      }
      return false;
    }

    // deprecated
    // check if a code point is an emoji
    inline bool is_emoji_v0(code_point cp)
    {
      // Core emoji ranges
      if((cp >= 0x1F300 &&
          cp <= 0x1F5FF) || // Miscellaneous Symbols and Pictographs
         (cp >= 0x1F600 && cp <= 0x1F64F) || // Emoticons
         (cp >= 0x1F680 && cp <= 0x1F6FF) || // Transport and Map Symbols
         (cp >= 0x1F900 &&
          cp <= 0x1F9FF) || // Supplemental Symbols and Pictographs
         (cp >= 0x1FA70 && cp <= 0x1FAFF)) // Symbols and Pictographs Extended-A
      {
        return true;
      }

      // Modifiers and components
      if((cp >= 0x1F3FB &&
          cp <= 0x1F3FF) || // Skin Tone Modifiers (Fitzpatrick)
         (cp >= 0x1F1E6 && cp <= 0x1F1FF) || // Regional Indicators (Flags)
         (cp == 0x200D) ||                   // Zero Width Joiner
         (cp == 0xFE0F)) // Variation Selector-16 (emoji style)
      {
        return true;
      }

      // Additional emoji-related ranges
      if((cp >= 0x2600 && cp <= 0x26FF) || // Miscellaneous Symbols (some emoji)
         (cp >= 0x2700 && cp <= 0x27BF) || // Dingbats (some emoji)
         (cp >= 0x2B50 && cp <= 0x2B59) || // Stars and similar symbols
         (cp >= 0x1F000 && cp <= 0x1F02F) || // Mahjong Tiles
         (cp >= 0x1F0A0 && cp <= 0x1F0FF))   // Playing Cards
      {
        return true;
      }

      return false;
    }

    // check if string is a valid identifier
    inline bool is_identifier(code_point cp)
    {
      size_t pos = 0;
      // hmmm is this good?
      if(!is_letter(cp) && !is_emoji(cp) && !is_digit(cp))
      {
        return false;
      }
      return true; // TODO: optionally check remaining chars
    }

    // lookup table for Unicode whitespace ranges
    static const unicode_range whitespace_ranges[] = 
    {
      { 0x0009, 0x000D}, // horizontal tab, line feed, vertical tab, form feed, carriage return
      { 0x0020, 0x0020 }, // space
      { 0x0085, 0x0085 }, // next line
      { 0x00A0, 0x00A0 }, // non breaking space
      { 0x1680, 0x1680 }, // ogham space mark
      { 0x2000, 0x200A }, // en quad to hair space
      { 0x2028, 0x2029 }, // line separator, paragraph separator
      { 0x202F, 0x202F }, // narrow no break space
      { 0x205F, 0x205F }, // medium mathematical space
      { 0x3000, 0x3000 }, // ideographic space
    };

    inline bool is_whitespace(code_point cp)
    {
      for(const auto& range : whitespace_ranges)
      {
        if(range.contains(cp))
          return true;
      }
      return false;
    }

    // deprecated
    inline bool is_whitespace_v0(code_point cp)
    {
      return cp == U' ' || cp == U'\t' || cp == U'\n' || cp == U'\r' ||
             cp == 0x00A0;
    }

    // deprecated
    // count utf-8 characters up to a byte position
    inline size_t count_chars(std::string_view input, size_t byte_pos)
    {
      size_t pos = 0, char_count = 0;
      while(pos < byte_pos && pos < input.size())
      {
        if(decode(input, pos))
          char_count++;
        else
          pos++; // skip invalid bytes
      }
      return char_count;
    }

    // check if string is a single utf-8 character
    inline bool is_single_char(std::string_view str)
    {
      if(str.empty())
        return false;

      uint8_t first_byte = static_cast<uint8_t>(str[0]);
      size_t bytes = detail::get_sequence_length(first_byte);
      return (bytes != 0 && str.length() == bytes &&
              detail::validate_continuation(str, 0, bytes));
    }
  } // namespace utf8

  namespace detail
  {
    enum class token_type : uint8_t
    {
      eof,
      identifire,
      assign,
      string,
      space,
      delimiter,
      double_delimiter,
      error
    };

    struct token 
    {
      token() = default;
      token(token_type t, const char ch) // for ascii and tokens with single ascii char
        : type(t), literal(std::string(1, ch))
      {
      }

      token(token_type t, const std::string_view str) // utf-8 and tokens with muliple chars
        : type(t), literal(str)
      {
      }

      token_type type;
      std::string literal;
      size_t start_pos{ 0 }; // utf-8 character start position
      size_t end_pos{ 0 };   // utf-8 character end position
    };

    constexpr std::string_view token_type_to_string(const token_type tt)
    {
      using namespace std::string_view_literals;
      switch(tt)
      {
        case token_type::eof:
          return "eof"sv;
        case token_type::identifire:
          return "identifire"sv;
        case token_type::string:
          return "string"sv;
        case token_type::space:
          return "space"sv;
        case token_type::assign:
          return "assign"sv;
        case token_type::delimiter:
          return "delimiter"sv;
        case token_type::double_delimiter:
          return "double_delimiter"sv;
        case token_type::error:
          return "error"sv;
      }
      return "unknown";
    }

    class lexer
    {
    public:
      using input_type = std::string_view;
      using delimiter_set = std::unordered_set<std::string_view>;
    public:
      lexer() = default;
      lexer(const input_type input,
            const delimiter_set& ds = make_default_delimiters_set())
        : m_input(input), m_delimiters(ds)
      {
        m_current = read_char();
      }

      token advance()
      {
        using namespace std::string_view_literals;

        //if(utf8::is_whitespace(m_current))
        //{
        //  D_PRINT("current token: '" << utf8::encode(m_current) << "' peek: '"
        //                             << utf8::encode(peek_char()) << "'");
          consume_whitespace();
        //}
        token tok;
        tok.start_pos = m_char_counter;
        switch(m_current)
        {
          case U'=': 
          {
            tok = { token_type::assign, "=" };
            break;
          }
          case U'\'': 
          {
            tok.type = token_type::string;
            tok.literal = read_string(U'\'');
            break;
          }
          case U'"': 
          {
            tok.type = token_type::string;
            tok.literal = read_string(U'"');
            break;
          }
          // case U' ':
          // case U'\t':
          // case U'\n':
          //{
          //   tok.type = token_type::space;
          //   tok.literal = " ";
          //   break;
          // }
          case 0: 
          {
            tok = { token_type::eof, "" };
            break;
          }
          default: 
          {
            if(utf8::is_whitespace(m_current))
            {
              tok.type = token_type::space;
              tok.literal = " "; // do we need the exact literal?
              break;
            }
            // always return from here on, we don't need to read_char at the end
            else if(is_delimiter(m_current))
            {
              auto pc = peek_char();
              if(is_delimiter(pc))
              {
                tok.type = token_type::double_delimiter;
                tok.literal = utf8::encode(m_current) + utf8::encode(pc);
                // tok.literal = m_input.substr(m_position, m_read_position -
                // m_position + (peek_char_pos() - m_read_position));
                read_char(); // skip the next delimiter (it's part of double_delimiter)
              }
              else
              {
                tok.type = token_type::delimiter;
                tok.literal = m_input.substr(m_position, m_read_position - m_position);
                // tok.literal = utf8::encode(m_current);
              }
              m_current = read_char();
              return tok;
            }
            else if(utf8::is_identifier(m_current)) // only the first char needs to be is_identifire!
            {
              tok.literal = read_identifire();
              tok.type = token_type::identifire;
              tok.end_pos = utf8::count_chars(m_input, m_position);
              return tok;
            }
            else
            {
              // unknown treat as error
              tok.type = token_type::error;
              tok.literal = utf8::encode(m_current);
            }
          }
        }
        tok.end_pos = utf8::count_chars(m_input, m_read_position);
        m_current = read_char();
        if(tok.type != token_type::eof)
          m_char_counter++;
        return tok;
      }
    private:
      utf8::code_point read_char()
      {
        if(m_read_position >= m_input.size())
          return 0; // eof

        auto pos = m_read_position;
        auto chopt = utf8::decode(m_input, pos);
        m_position = m_read_position;
        m_read_position = pos;
        return chopt.value_or(U'?');
      }

      utf8::code_point peek_char() const
      {
        if(m_read_position >= m_input.size())
          return 0;
        auto pos = m_read_position;
        return utf8::decode(m_input, pos).value_or(U'?');
      }

      // all read_xxx functions return string_view because it's just reading
      // from m_input and there is no need for another copy (other than in
      // token)
      std::string_view read_identifire()
      {
        size_t pos = m_position;

        // allow identifires to have number in them but not the first char like:
        // foo_1 but not like: 1_foo (tricky to lex)
        // while(!std::isspace(m_current & 0xff) &&
        // utf8::is_valid_utf8(utf8::encode(m_current)))
        while(m_current != 0 && !utf8::is_whitespace(m_current) && !is_delimiter(m_current))
        {
          if(m_current == U'=')
          {
            break; // TODO: validate this pls
          }
          m_current = read_char();
          m_char_counter++;
        }
        const auto& sub = m_input.substr(pos, m_position - pos);
        return sub;
      }

      // std::string_view read_number()
      //{
      //   size_t pos = m_position;
      //   //TODO: add support for floats and sceintific notation and hex, oct,
      //   etc.. while(utf8::is_digit(m_current))
      //   {
      //     m_current = read_char();
      //   }
      //   return m_input.substr(pos, m_position - pos);
      // }

      // deprecated
      std::string_view read_string_v0(utf8::code_point open)
      {
        m_current = read_char(); // skip open
        size_t pos = m_position; // after open

        auto pc = peek_char();
        while(pc != 0)
        {
          // if(pc == open && m_current == U'\\')
          //{
          //   // TODO: ignore the backslash and consume only the string
          // }

          m_current = read_char();
          pc = peek_char();
          m_char_counter++;
        }
        auto result = m_input.substr(pos, m_position - pos);
        if(m_current == open)
          m_current = read_char();
        return result;
        // return m_input.substr(pos, m_position - pos);
      }

    std::string_view read_string(utf8::code_point quote) 
    {
      m_current = read_char(); // Skip opening quote
      size_t pos = m_position;
      while(m_current != quote && m_current != 0) 
      {
        m_current = read_char();
      }
      std::string_view result = m_input.substr(pos, m_position - pos);
      
      if(m_current == quote)
        m_current = read_char(); // skip closing quote
      
      return result;
    }

      // yeah and also don't forget that the shell (at least bash/zsh) don't
      // pass redundent whitespaces EVEN IN STRINGS unless it was escaped with
      // '\' and spare (A WHOLE DAY) debugging!
      void consume_whitespace()
      {
        D_PRINT("consume called!");
        // only redundent whitespace
        while(utf8::is_whitespace(m_current) && utf8::is_whitespace(peek_char()))
        {
          m_current = read_char();
          m_char_counter++;
          D_PRINT("consumed: whitespace" << "nnnn");
        }
      }

      bool is_delimiter(utf8::code_point cp)
      {
        auto r = m_delimiters.find(utf8::encode(cp)) != m_delimiters.end();
        return r;
      }

      static delimiter_set make_default_delimiters_set()
      {
        return delimiter_set{"-"};
      }

    private:
      input_type m_input;
      size_t m_position{0};
      size_t m_read_position{0};
      size_t m_char_counter{0};
      utf8::code_point m_current{0};
      delimiter_set m_delimiters;
    };
  } // namespace detail

  class option_builder;
  class command_builder;

  enum class convertion_error
  {
    cant_convert_type
  };

  template <typename T>
  concept is_expected_compatible = requires(T t) 
  {
    requires std::is_move_constructible_v<T>;
    requires std::is_move_assignable_v<T>;
    requires std::is_destructible_v<T>;
  };

  // base converter template
  template <typename T>
  requires std::is_default_constructible_v<T> && is_expected_compatible<T>
  struct converter
  {
    static std::expected<T, convertion_error> convert(const std::string& input)
    {
      T result{};
      std::stringstream ss(input);
      ss >> std::noskipws;
      if(!(ss >> result) || !ss.eof())
      {
        return std::unexpected(convertion_error::cant_convert_type);
      }
      return result;
    }
  };

  // customization point for type-specific conversion rules
  template <typename T> 
  struct conversion_traits 
  {
    static std::expected<T, convertion_error> convert(const std::string& input)
    {
      return converter<T>::convert(input); // Fallback to default converter
    }
  };

  // default bool specialization
  template <> 
  struct converter<bool> 
  {
    static std::expected<bool, convertion_error>
    convert(const std::string& input)
    {
      std::string lower = input;
      std::transform(lower.begin(), lower.end(), lower.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      if(lower == "true" || lower == "1" || lower == "yes" || lower == "y")
        return true;
      if(lower == "false" || lower == "0" || lower == "no" || lower == "n")
        return false;
      return std::unexpected(convertion_error::cant_convert_type);
    }
  };

  // convenience function to use the converter
  template <typename T>
  std::expected<T, convertion_error> get(const std::string& input)
  {
    return conversion_traits<T>::convert(input);
  }

  enum class convertion_state
  {
    not_convertable,
    no_value,
    not_found,
    missing_required,
    invalid_token
  };
/*
  class value_storage
  {
  public:
    // Constructors for different use cases
    value_storage() = default;
    explicit value_storage(std::string single_value)
      : values_{std::move(single_value)}
    {
    }

    explicit value_storage(std::vector<std::string> multi_values)
      : values_{std::move(multi_values)}
    {
    }

    explicit value_storage(std::vector<std::string> multi_values,
                           std::vector<std::string> defaults)
      : values_{std::move(multi_values)}, defaults_{std::move(defaults)}
    {
    }

    // Accessors
    bool has_value() const { return !values_.empty(); }

    const std::vector<std::string>& get_values() const { return values_; }

    const std::vector<std::string>& get_defaults() const { return defaults_; }

    std::string get_raw() const
    { // For backward compatibility or debugging
      std::stringstream ss;
      for(const auto& v : values_)
        ss << v << " ";
      auto s = ss.str();
      return s.empty() ? "" : s.substr(0, s.size() - 1);
    }

    // Conversion (post-parsing)
    template <typename T>
    std::expected<T, convertion_state> get() const
    {
      if(values_.empty())
      {
        if(!defaults_.empty())
          return convert<T>(defaults_[0]);
        return std::unexpected(convertion_state::no_value);
      }
      if(values_.size() > 1)
        return std::unexpected(convertion_state::not_convertable);
      return convert<T>(values_[0]);
    }

    template <typename T>
    std::expected<std::vector<T>, convertion_state> get_vector() const
    {
      std::vector<T> result;
      auto& source = values_.empty() ? defaults_ : values_;
      if(source.empty())
        return std::unexpected(convertion_state::no_value);
      for(const auto& v : source)
      {
        auto r = convert<T>(v);
        if(!r)
          return std::unexpected(convertion_state::not_convertable);
        result.push_back(*r);
      }
      return result;
    }

  private:
    template <typename T>
    std::expected<T, convertion_state> convert(const std::string& input) const
    {
      auto r = conversion_traits<T>::convert(input);
      if(!r)
        return std::unexpected(convertion_state::not_convertable);
      return *r;
    }

    std::vector<std::string> values_;   // Parsed values
    std::vector<std::string> defaults_; // Default values (if any)
  };
*/

  // represents a single argument specification
  struct argument_spec 
  {
    // no named arguments anymore!
    // std::string name; // e.g. "url", "path"
    bool required = false; // is this argument mandatory?
    std::string default_value; // default value if optional and not provided
  };

  enum class value_state 
  {
    present, // provided in input
    defaulted, // provided in defaults
    missing // no values or defaults
  };

  class value_storage 
  {
  public:
    struct value_entry
    {
      std::string value;
      value_state state = value_state::missing;
    };

    // default constructor
    value_storage() = default;

    // constructor with argument specs
    explicit value_storage(const std::vector<argument_spec>& specs)
        : m_specs(specs)
    {
      m_values.resize(m_specs.size());
      // populate the name to index map and initialize defaults
      for(size_t i = 0; i < m_specs.size(); ++i) 
      {
        if(!m_specs[i].default_value.empty()) 
        {
          m_values[i].value = m_specs[i].default_value;
          m_values[i].state = value_state::defaulted;
        }
      }
    }

    // set values from parsed input
    void set_values(const std::vector<std::string>& input_values)
    {
      for(auto i = 0; i < input_values.size(); ++i)
      {
        m_values[i].value = input_values[i];
        m_values[i].state = value_state::present;
      }
    }

    bool is_valid() const 
    {
      for(size_t i = 0; i < m_specs.size(); ++i) 
      {
        const auto& spec = m_specs[i];
        const auto& entry = m_values[i];
        if(spec.required && entry.state == value_state::missing) 
          return false;
      }
      return true;
    }

    // get value by position
    std::expected<std::string, convertion_state> get_raw(size_t pos) const
    {
      if(pos >= m_values.size()) 
      {
        return std::unexpected(convertion_state::not_found);
      }
      if(m_values[pos].state == value_state::missing)
      {
        return std::unexpected(convertion_state::no_value);
      }
      return m_values[pos].value;
    }

    // typed access
    template <typename T>
    std::expected<T, convertion_state> get(size_t pos) const 
    {
      auto raw = get_raw(pos);
      if(!raw) 
        return std::unexpected(raw.error());
      return conversion_traits<T>::convert(*raw);
    }

    // get all values as vector
    template <typename T> 
    std::expected<std::vector<T>, convertion_state> get_vector() const 
    {
      std::vector<T> res;
      for(auto i = 0; i < m_values.size(); ++i)
      {
        if(m_values[i].state != value_state::missing)
        {
          auto conv = conversion_traits<T>::convert(m_values[i].value);
          if(!conv)
            return std::unexpected(convertion_state::not_convertable);
          
          res.push_back(conv);
        }
      }
      return res;
    }

    bool has_value(size_t pos) const 
    {
      return pos < m_values.size() && m_values[pos].state != value_state::missing;
    }

    size_t value_count() const 
    {
      size_t count = 0;
      for(auto& entry : m_values)
        if(entry.state != value_state::missing)
          ++count;
      return count;
    }
  private:
    std::vector<argument_spec> m_specs; // argument definitions
    std::vector<value_entry> m_values;  // positional values
  };

  class option
  {
  public:
    option() = default;

    bool has_value(size_t pos) const 
    { 
      return m_value.has_value(pos); 
    }

    bool is_valid() const 
    {
      return m_value.is_valid();
    }

    size_t value_count() const 
    {
      return m_value.value_count();
    }

    std::expected<std::string, convertion_state> get_raw(size_t pos) const 
    { 
      // maybe default to pos = 0
      return m_value.get_raw(pos);
    }

    template <typename T>
    std::expected<T, convertion_state> get(size_t pos) const 
    {
      return m_value.get<T>(pos);
    }

    template <typename T>
    std::expected<std::vector<T>, convertion_state> get_vector() const 
    {
      return m_value.get_vector<T>();
    }
  private:
    value_storage m_value;
    friend class parser;
  };

  class flag
  {
  public:
    flag() = default;

  private:
    friend class parser;
  };

  class command
  {
  public:
    command() = default;

    bool has_value(size_t pos) const 
    { 
      return m_value.has_value(pos); 
    }

    bool is_valid() const 
    {
      return m_value.is_valid();
    }

    size_t value_count() const 
    {
      return m_value.value_count();
    }

    std::expected<std::string, convertion_state> get_raw(size_t pos) const 
    { 
      // maybe default to pos = 0
      return m_value.get_raw(pos);
    }

    template <typename T>
    std::expected<T, convertion_state> get(size_t pos) const 
    {
      return m_value.get<T>(pos);
    }

    template <typename T>
    std::expected<std::vector<T>, convertion_state> get_vector() const 
    {
      return m_value.get_vector<T>();
    }


    std::expected<std::reference_wrapper<command>, convertion_state> get_command(const std::string& name)
    {
      const auto cmd_it = m_commands.find(name);
      if(m_commands.end() != cmd_it)
        return std::ref(cmd_it->second);

      return std::unexpected(convertion_state::not_found);
    }

    std::expected<std::reference_wrapper<option>, convertion_state> get_option(const std::string& name)
    {
      const auto opt_it = m_options.find(name);
      if(m_options.end() != opt_it)
        return std::ref(opt_it->second);

      return std::unexpected(convertion_state::not_found);
    }

    std::expected<std::reference_wrapper<flag>, convertion_state> get_flag(const std::string& name)
    {
      const auto flag_it = m_flags.find(name);
      if(m_flags.end() != flag_it)
        return std::ref(flag_it->second);

      return std::unexpected(convertion_state::not_found);
    }

    const std::unordered_map<std::string, command>& get_subcommands() const
    {
      return m_commands;
    }

    const std::unordered_map<std::string, option>& get_options() const
    {
      return m_options;
    }

    const std::unordered_map<std::string, flag>& get_flags() const
    {
      return m_flags;
    }
  private:
    value_storage m_value;
    std::unordered_map<std::string, command> m_commands;
    std::unordered_map<std::string, option> m_options;
    std::unordered_map<std::string, flag> m_flags;
  private:
    friend class parser;
    friend class parse_resault;
  };

  class option_builder
  {
  public:
    using default_value_type = std::string;
    enum class alias_options
    {
      off,
      single_delimiter,
      double_delimiter, // TODO: (or maybe not) as_name
      _default = single_delimiter
    };

  public:
    option_builder() = default;

    option_builder& requires_values(int min, int max = -1, const std::vector<std::string>& defaults = {})
    {
      m_arguments.clear();
      auto default_count = defaults.size();
      auto spec_count = std::max<size_t>(min, max == -1 ? default_count : max);
      for(auto i = 0; i < spec_count; ++i)
      {
        argument_spec spec;
        spec.required = i < min;
        if(i < default_count)
          spec.default_value = defaults[i];
        m_arguments.push_back(spec);
      }
      m_min_values_limit = min;
      m_max_values_limit = max;
      return *this;
    }
    
    const std::vector<argument_spec>& get_arguments() const 
    {
      return m_arguments;
    }

    int get_min_limit() const 
    { 
      return m_min_values_limit; 
    }

    int get_max_limit() const 
    { 
      return m_max_values_limit; 
    }

    option_builder& set_alias(const std::string& name);

    option_builder& set_alias_options(alias_options opt)
    {
      m_alias_opts = opt;
      return *this;
    }
  private:
    option_builder(command_builder* parent, const std::string& name)
      : m_parent(parent), m_name(name)
    {
    }
  private:
    std::vector<argument_spec> m_arguments; 
    alias_options m_alias_opts{alias_options::_default};
    command_builder* m_parent{nullptr};
    std::string m_name;
    int m_min_values_limit{0};
    int m_max_values_limit{0};
  
    friend class parser;
    friend class command_builder;
  };

  class command_builder
  {
  public:
    using default_value_type = std::string;

  public:
    // new one for the added command
    command_builder& add_subcommand(const std::string& name) // e.g. git submodule
    {
      auto& sub = m_subcommands[name];
      sub.m_debug_name = name; // debug only
      return sub;
    }

    option_builder& add_option(const std::string& name) // e.g. git --help
    {
      auto& opt = m_options[name];
      opt = option_builder{this, name};
      return opt;
    }

    command_builder& add_flag(const std::string& name) // must be one utf-8 char
                                                       // e.g git -v
    {
      m_flags.emplace(name);
      return *this;
    }

    command_builder& requires_values(int min, int max = -1, const std::vector<std::string>& defaults = {})
    {
      m_arguments.clear();
      auto default_count = defaults.size();
      auto spec_count = std::max<size_t>(min, max == -1 ? default_count : max);
      for(auto i = 0; i < spec_count; ++i)
      {
        argument_spec spec;
        spec.required = i < min;
        if(i < default_count)
          spec.default_value = defaults[i];
        m_arguments.push_back(spec);
      }
      m_min_values_limit = min;
      m_max_values_limit = max;
      return *this;
    }
    
    const std::vector<argument_spec>& get_arguments() const 
    {
      return m_arguments;
    }

    int get_min_limit() const 
    { 
      return m_min_values_limit; 
    }

    int get_max_limit() const 
    { 
      return m_max_values_limit; 
    }

    command_builder& set_option_alias(const std::string& alias_name,
                                      const std::string& option_name) // TODO: clash resulotion config
    {
      m_options_aliases[alias_name] = option_name;
      return *this;
    }
  public: // private: wtf is this?
    command_builder() = default;
  private:
    std::vector<argument_spec> m_arguments;
    std::string m_debug_name;
    std::unordered_map<std::string, command_builder> m_subcommands;
    std::unordered_map<std::string, option_builder> m_options;
    std::unordered_set<std::string> m_flags;
    std::unordered_map<std::string, std::string> m_options_aliases; // { alias : option }
    char m_min_values_limit{ 0 };
    char m_max_values_limit{ 0 };

  private:
    friend class parser;
  };

  inline option_builder& option_builder::set_alias(const std::string& name)
  {
    m_parent->set_option_alias(name, m_name);
    return *this;
  }

  enum class error_code
  {
    none,

    // parse errors

    unexpected_token,    // found a token that doesn’t fit the expected grammar
    missing_value,       // an option or command requires a value, but none was provided
    invalid_flag,        // a flag doesn’t exist
    invalid_option,      // an option doesn’t exist
    invalid_subcommand,  // a subcommand doesn’t exist
    invalid_alias,       // an alias doesn’t resolve correctly
    malformed_multiflag, // a multi-flag (e.g., -abc) contains invalid flags
    utf8_error,          // invalid utf-8 sequence encountered
    missing_identifier,  // expected an identifier after a delimiter but didn’t find one
    invalid_single_delimiter,
    invalid_double_delimiter,
    invalid_identifier,
    empty_literal,
    unexpected_eof // reached end of file unexpectedly
  };

  enum class error_type
  {
    none,
    parse_error,
    evaluation_error
  };

  struct error // is this the best error handling you can do?
  {
    error_type type{ error_type::none };
    error_code code{ error_code::none };
    std::string message;
  };

  class parse_resault
  {
  public:
    using error_vector = std::vector<error>;
  public:
    parse_resault() = default;

    bool has_error() const 
    { 
      return !m_errors.empty(); 
    }

    const error_vector& get_errors() const 
    { 
      return m_errors; 
    }
  public:
    command root;
  private:
    std::vector<error> m_errors;
  private:
    friend class parser;
  };

  enum class multiflag_policy
  {
    strict, // stop at first invalid flag, reject the whole token
    lax, // process valid flags, skip invalid ones, no error unless all arr invalid
    validate_all // check all flags first, only apply if all are valid, error otherwise
  };

  class parser
  {
  public:
    parser() = default;

    // avoid stupid misstakes like shallow copying the pointers and spare 3 hours debugging!!
    parser(const parser&) = delete;
    parser(parser&&) = delete;

    // build stage(pre parse)

    inline command_builder& add_subcommand(const std::string& name)
    {
      return m_root.add_subcommand(name);
    }

    inline option_builder& add_option(const std::string& name)
    {
      return m_root.add_option(name);
    }

    inline void add_flag(const std::string& name) { m_root.add_flag(name); }

    // TODO: windowsss(not utf-8)
    parse_resault parse(int argc, char** argv)
    {
      m_current_command = &m_root;
      m_current_resault_command = &m_resault.root;
      auto args_str = clara::detail::join_args(argc, argv);
      m_lx = detail::lexer{args_str};

      return parse();
    }

    parse_resault parse(const std::vector<std::string>& args)
    {
      std::stringstream ss;
      // TODO: move this to an overload of join_args fun
      std::for_each(args.begin() + 1, args.end(),
                    [&ss](const auto& elem) { ss << elem << " "; });

      auto str = ss.str();
      m_lx = detail::lexer{str};

      return parse();
    }

  private:
    enum class single_delimiter_ : uint8_t
    {
      invalid,
      alias,
      flag,
      mutliflag
    };

    enum class double_delimiter_ : uint8_t
    {
      invalid,
      alias,
      option
    };

    enum class identifier : bool
    {
      invalid,
      subcommand
    };

    enum class alias_policy
    {
      override, // alias replaces existing option
      error,    // error on clash
      ignore    // ignore alias if it clashes
    };
    
    struct parsed_values
    {
      std::vector<std::string> values;
      bool valid = true;
    };
  private:
    parse_resault parse()
    {
      bool have_error{ false };
      m_current_token = m_lx.advance();
      m_peek_token = m_lx.advance();

      while(m_current_token.type != detail::token_type::eof && !have_error)
      {
        error err;
        m_current_error = &err;
        if(!parse_one())
        {
          advance();
          if(!m_continue_on_error)
            have_error = true;
        }

        for(auto f : m_to_parse)
        {
          if(!f())
          {
            advance();
            if(!m_continue_on_error)
              have_error = true;
          }
        }
        m_to_parse.clear();
        if(err.type != error_type::none)
          m_resault.m_errors.push_back(err);
      }
      return m_resault;
    }

    void advance()
    {
      m_current_token = m_peek_token;
      m_peek_token = m_lx.advance();
    }

    // returns true if it could parse this current token and changes the state
    // to the next tokens and returns false if it couldn't parse the current
    // token and it requires the state to be advanced
    bool parse_one()
    {
      using detail::token_type;
      switch(m_current_token.type)
      {
        case token_type::delimiter: {
          D_PRINT("parse_delimiter start" << m_current_token.literal << " ");
          auto d = parse_delimiter();
          D_PRINT("parse_delimiter end");
          return d;
        }
        case token_type::double_delimiter: {
          D_PRINT("parse_double_delimiter start" << m_current_token.literal << " ");
          auto d = parse_double_delimiter();
          D_PRINT("parse_double_delimiter end");
          return d;
        }
        case token_type::identifire: {
          D_PRINT("parse_identifire start" << m_current_token.literal << " ");
          auto d = parse_identifire();
          D_PRINT("parse_identifire end");
          return d;
        }
        case token_type::space: {
          D_PRINT("space: " << m_current_token.literal << " ");
          return false;
        }
        case token_type::assign: {
          D_PRINT("assign escaped parsing: " << m_current_token.literal);
          return false;
        }
        default: {
          D_PRINT("unknown token cant parse");
          D_PRINT("token: type: " << uint32_t(m_current_token.type)
                                  << " literal: '" << m_current_token.literal
                                  << "'");
          return false;
        }
      }
    }

    // returns true if it can parse the delimiter and the next tokne and it's
    // requirement(s) returns false otherwise and it requires the token to be
    // advanced externally
    bool parse_delimiter()
    {
      // we are now at the delimiter token

      if(!expect_next(detail::token_type::identifire))
      {
        set_missing_identifier("", m_peek_token.literal);
        return false;
      }

      // we are now at the identifire after the delimiter

      auto tp = get_single_delimiter_type(m_current_token);
      switch(tp)
      {
        case single_delimiter_::flag: {
          save_flag(m_current_token.literal, {});
          advance();
          return true;
        }
        case single_delimiter_::alias: {
          const auto& alias_it =
            m_current_command->m_options_aliases.find(m_current_token.literal);
          return resolve_alias(true, alias_it);
        }
        case single_delimiter_::mutliflag: {
          return resolve_multiflag();
        }
        case single_delimiter_::invalid:
        default: {
          set_invalid_single_delimiter(m_current_token.literal);
          return false;
        }
      }
    }

    // returns true if it can parse the double_delimiter and the next tokne and
    // it's requirement(s) returns false otherwise and it requires the token to
    // be advanced externally

    bool parse_double_delimiter()
    {
      // we are now at the delimiter token

      if(!expect_next(detail::token_type::identifire))
      {
        set_missing_identifier("", m_peek_token.literal);
        // return at the double_delimiter token require advancment
        return false;
      }

      // we are now at the identifire after the delimiter

      auto tp = get_double_dlimiter_type(m_current_token);
      switch(tp)
      {
        case double_delimiter_::alias: 
        {
          const auto& alias_it = m_current_command->m_options_aliases.find(m_current_token.literal);
          return resolve_alias(false, alias_it);
        }
        case double_delimiter_::option: 
        {
          const auto& opt_it = m_current_command->m_options.find(m_current_token.literal);
          return resolve_option(opt_it->first, opt_it->second);
        }
        case double_delimiter_::invalid:
        default: 
        {
          set_invalid_double_delimiter(m_current_token.literal);
          return false;
        }
      }
    }

    // returns true if it can parse the identifier and it's requirement(s)
    // returns false otherwise and it requires the token to be advanced
    // externally
    bool parse_identifire()
    {
      // TODO: it's not (must be an identifire) it can be a value for a
      // subcommand called earlier and then used some options from it and then
      // gave it a value like subcmd --opt opts_val cmds_val but if opt takes
      // multiple values there is no way to know which is which either the
      // commands_value should be given before opt_value or the option should
      // have limit (TODO) stand-alone identifire must be a subcommand -> wrong

      // subcommand check
      auto sub_it = m_current_command->m_subcommands.find(m_current_token.literal);
      if(m_current_command->m_subcommands.end() != sub_it)
      {
        // it's a subcommand resolve it and forward the resolving state
        return resolve_command(sub_it->first, sub_it->second);
      }

      set_invalid_subcommand(m_current_token.literal);
      // invalid subcommand require advancment
      return false;
    }

    // this function is bad , update it got better
    bool resolve_alias(bool is_single_delm_call, const auto& alias_it)
    {
      // if is aliase route to option path
      if(m_current_command->m_options_aliases.end() != alias_it)
      {
        // TODO: missmatch check
        switch(m_current_command->m_options[alias_it->second].m_alias_opts)
        {
          case option_builder::alias_options::off: 
          {
            advance();
            return true;
          }
          case option_builder::alias_options::double_delimiter: 
          {
            if(!is_single_delm_call)
            {
              return resolve_option(alias_it->second,
                                    m_current_command->m_options[alias_it->second]);
            }
          }
          case option_builder::alias_options::single_delimiter:
          {
            if(is_single_delm_call)
            {
              return resolve_option(alias_it->second,
                                    m_current_command->m_options[alias_it->second]);
            }
          }
        }
      }
      set_invalid_alias(alias_it->first);
      return false;
    }

    // returns true if it can parse the option and it's requirement(s)
    // returns false otherwise and it requires the token to be advanced externally
    bool resolve_option(const std::string& opt_name, const option_builder& opt_build)
    {
      // we are now at the options identifire
      option opt;
      opt.m_value = value_storage{ opt_build.get_arguments() };
      if(opt_build.m_min_values_limit > 0) // HERE
      {
        if(expect_next(detail::token_type::eof)) // allow any token to be argument only
                                                 // error on eof (expected next token)
        {
          set_unexpected_eof();
          return false;
        }
        advance();
        // we are now at the token after the identifire (might be assign or
        // space) or both
        auto parsed = parse_values(opt_build.m_min_values_limit, opt_build.m_max_values_limit);
        if(!parsed.valid)
          return false; //TODO: check here if it is in valid pos or not for advance

        opt.m_value.set_values(parsed.values);
      }
      else 
      {
        advance();
      }

      if(!opt.m_value.is_valid())
      {
        set_missing_value("required argument missing/invalid");
        return false; //TODO: and also here for advance
      }

      save_option(opt_name, opt);
      return true;
    }

    // deprecated
    bool resolve_multiflag_v0()
    {
      // returns true if it can parse the multi-flags and resolve each one of
      // them returns false otherwise and it requires the token to be advanced
      // externally
      // TODO: maybe allow to enable resolving the rest of the flags
      // if one of the flags are not correct,
      // or add some more configurations to this function
      // because the behavior now is if a flag isn't correct
      // we return immedatly but there might be another flags in front of it
      // that won't be resolved or even checked.

      // we are now at the identifier next to the delimiter
      // that we suspect is multiflag

      if(m_current_token.literal.empty())
      {
        // add a check for empty token.
        return false;
      }
      // TODO: add an option to validate all flags then run them because in some
      // tools running a command with some flags and not the others can be wrong
      // so better to error than to do something you are not supposed to
      // rm -rf
      size_t pos = 0, end_pos = 0;
      bool all_flags_valid = true; // Use a flag to track overall validity

      while(end_pos < m_current_token.literal.size())
      {
        bool advance_success =
          utf8::advance_one_char(m_current_token.literal, end_pos);
        if(!advance_success)
        {
          // Handle UTF-8 error (e.g., invalid character)
          // TODO: Add error handling for invalid UTF-8
          set_utf8_error(m_current_token.literal);
          return false;
        }

        const auto& str = m_current_token.literal.substr(pos, end_pos);

        const auto& flag_it = m_current_command->m_flags.find(str);
        pos = end_pos;
        // check if the current flag is not valid
        if(m_current_command->m_flags.end() == flag_it)
        {
          if(!m_continue_on_error)
          {
            // Stop processing on the first error
            set_malformed_multiflag(m_current_token.literal, str);
            return false;
          }
        }
        else
        {
          save_flag(str, {}); // Add the valid flag
        }
      }

      // we are done with this identifier advance it
      advance();
      return true; // Return true even if not all flags are valid 'cause we
                   // already advanced idk if we should only advance if all are
                   // valid?
    }

    // deprecated
    bool resolve_multiflag_v1()
    {
      if(m_current_token.literal.empty())
      {
        set_empty_literal(0);
        return false;
      }

      size_t pos = 0;
      std::vector<std::string> valid_flags;
      std::string invalid_flag;

      // Step 1: Parse all characters
      while(pos < m_current_token.literal.size())
      {
        size_t end_pos = pos;
        if(!utf8::advance_one_char(m_current_token.literal, end_pos))
        {
          set_utf8_error(m_current_token.literal);
          return false;
        }
        std::string flag_str =
          m_current_token.literal.substr(pos, end_pos - pos);
        if(m_current_command->m_flags.find(flag_str) !=
           m_current_command->m_flags.end())
        {
          valid_flags.push_back(flag_str);
        }
        else
        {
          invalid_flag = flag_str;
          if(m_multiflag_policy == multiflag_policy::strict)
          {
            set_malformed_multiflag(m_current_token.literal, invalid_flag);
            return false; // Stop immediately
          }
        }
        pos = end_pos;
      }

      // Step 2: Handle policy
      switch(m_multiflag_policy)
      {
        case multiflag_policy::strict: {
          // Already handled above, all flags are valid if we reach here
          for(const auto& flag : valid_flags)
            save_flag(flag, {});

          advance();
          return true;
        }
        case multiflag_policy::lax: {
          for(const auto& flag : valid_flags)
            save_flag(flag, {});

          if(valid_flags.empty())
          {
            set_malformed_multiflag(m_current_token.literal, invalid_flag);
            advance();
            return false; // No valid flags
          }
          advance();
          return true;
        }
        case multiflag_policy::validate_all: {
          if(!invalid_flag.empty())
          {
            set_malformed_multiflag(m_current_token.literal, invalid_flag);
            return false; // All must be valid
          }
          for(const auto& flag : valid_flags)
            save_flag(flag, {});

          advance();
          return true;
        }
      }
      return false; // Unreachable, but for safety
    }

    bool resolve_multiflag()
    {
      if(m_current_token.literal.empty())
      {
        set_empty_literal(m_current_token.start_pos);
        return false;
      }

      std::vector<std::string> valid_flags;
      std::string invalid_flag;
      size_t pos = 0;

      // parse utf-8 characters as potential flags
      while(pos < m_current_token.literal.size())
      {
        size_t end_pos = pos;
        if(!utf8::advance_one_char(m_current_token.literal, end_pos))
        {
          set_utf8_error(m_current_token.literal);
          return false;
        }

        std::string flag_str = m_current_token.literal.substr(pos, end_pos - pos);
        const auto& flag_it = m_current_command->m_flags.find(flag_str);
        if(flag_it != m_current_command->m_flags.end())
        {
          valid_flags.push_back(flag_str); // valid flag found
        }
        else
        {
          invalid_flag = flag_str;
          if(m_multiflag_policy == multiflag_policy::strict)
          {
            set_malformed_multiflag(m_current_token.literal, invalid_flag);
            return false; // strict mode: fail on first invalid flag
          }
        }
        pos = end_pos;
      }

      // handle policy
      switch(m_multiflag_policy)
      {
        case multiflag_policy::strict:
        {
          // all flags are valid if we reach here (early return on invalid)
          for(const auto& flag : valid_flags)
          {
            save_flag(flag, {});
          }
          advance();
          return true;
        }
        case multiflag_policy::lax:
        {
          for(const auto& flag : valid_flags)
          {
            save_flag(flag, {});
          }
          if(valid_flags.empty())
          {
            set_malformed_multiflag(m_current_token.literal, invalid_flag);
            advance();
            return false; // no valid flags
          }
          advance();
          return true;
        }
        case multiflag_policy::validate_all:
        {
          if(!invalid_flag.empty())
          {
            set_malformed_multiflag(m_current_token.literal, invalid_flag);
            return false; // all must be valid
          }
          for(const auto& flag : valid_flags)
          {
            save_flag(flag, {});
          }
          advance();
          return true;
        }
        default:
          return false; // unknown policy, fail safely
      }
    }

    // returns true if it can parse the command and it's requirement(s)
    // returns false otherwise and it requires the token to be advanced
    // externally
    bool resolve_command(const std::string& name, command_builder& cmd_build)
    {
      // we are now at the command identifier

      D_PRINT("resolve_command: name: " << name << " curr tok: "
                                        << m_current_token.literal);
      command cmd;
      cmd.m_value = value_storage{ cmd_build.get_arguments() };
      if(cmd_build.get_min_limit() > 0)
      {
        if(expect_next(detail::token_type::eof)) // allow any token to be argument only
                                                 // error on eof (expected next token)
        {
          set_unexpected_eof();
          return false;
        }
        advance();

        // we are now at the next token after the option
        auto parsed = parse_values(cmd_build.m_min_values_limit, cmd_build.m_max_values_limit);
        if(parsed.valid)
          return false;

        cmd.m_value.set_values(parsed.values);
      }
      else
      {
        advance();
      }

      if(!cmd.m_value.is_valid())
      {
        set_missing_value("required argument missing/invalid");
        return false; //TODO: check for advance
      }

      // TODO: refactor this so you add it first and work on the reference
      // instead of copying it again when you add it
      auto& added_cmd = save_command(name, cmd);

      // change the current command to this (resolved) one
      change_command(added_cmd, cmd_build);
      // advance(); // idon't know in this case if we should advance
      //  because of the parse_args (here and in resolve_option)
      //  TODO: check
      //  update: it seems like we dont need to advance
      return true;
    }

    // TODO: policy to override this behavior -> the function parses anything
    // till it reaches the max limit and then returns
    // what if the user wanted some default values (in the evaluator)
    // then this function shouldn't consume anything i mean
    // if it encountered something known it should stop there and raise
    // a limit error and the evaluator should catch it (if there is default
    // value(s))

    // for now its nither making this nor that so yeah!...

    // deprecated
    // TODO: error if no argument there is no values
    // update: WTF did i just say?
    std::string parse_list_v0(int min, int max)
    {
      // we are now at the first item of the list
      // it shouldn't be eof (must be checkd externally)

      std::stringstream ss;
      int value_ctr = 0;
      while(m_current_token.type != detail::token_type::eof && value_ctr <= max)
      {
        if(parse_known())
        {
          return ss.str();
        }
        else
        {
          if(m_current_token.type != detail::token_type::eof)
          {
            ss << m_current_token.literal;
            advance();
            value_ctr++;
          }
        };
      }
      return ss.str();
    }

    enum class list_policy
    {
      strict,    // enforce min/max exactly
      lookahead, // stop at known tokens, validate min/max
      greedy,    // consume until known token or eof
      lax        // allow any number up to max
    };

    // deprecated
    std::string parse_list_v1(int min, int max,
                              list_policy policy = list_policy::lookahead)
    {
      std::stringstream ss;
      int value_ctr = 0;

      while(m_current_token.type != detail::token_type::eof && value_ctr < max)
      {
        if(policy == list_policy::lookahead && parse_known())
        {
          break; // Stop at known token
        }
        if(policy == list_policy::strict && value_ctr >= max)
        {
          set_unexpected_token(m_current_token.literal,
                               m_current_token.start_pos);
          break;
        }
        ss << m_current_token.literal << " ";
        advance();
        value_ctr++;
      }

      std::string result = ss.str();
      if(policy != list_policy::lax && value_ctr < min)
      {
        set_missing_value(m_current_token.literal);
      }
      return result.empty()
               ? ""
               : result.substr(0, result.size() - 1); // Trim trailing space
    }


    std::vector<std::string> parse_list(int min, int max,
                                        list_policy policy = list_policy::lookahead)
    {
      D_PRINT("parse list: first tk: " << m_current_token.literal);
      std::vector<std::string> values;
      if(max == -1) max = INT_MAX;
      int value_ctr = 0;

      while(m_current_token.type != detail::token_type::eof && value_ctr < max)
      {
        if(m_current_token.type == detail::token_type::space)
        {
          advance();
          continue;
        }
        if(policy == list_policy::lookahead && parse_known())
        {
          break; // stop at known token
        }
        if(policy == list_policy::strict && value_ctr >= max)
        {
          set_unexpected_token(m_current_token.literal,
                               m_current_token.start_pos);
          break;
        }
        values.push_back(m_current_token.literal);
        advance();
        value_ctr++;
      }

      if(policy != list_policy::lax && value_ctr < min)
      {
        set_missing_value(m_current_token.literal);
      }
      return values;
    }

    bool parse_known()
    {
      // we are now at the token that might represnt something known
      // if so we schedule it
      // else we return false (we don't know it)

      D_PRINT("parse known called: with token: "
              << detail::token_type_to_string(m_current_token.type)
              << " literal: " << m_current_token.literal);
     
      using detail::token_type;

      switch(m_current_token.type)
      {
        case token_type::delimiter: 
        {
          D_PRINT("i know that one trying to parse...");
          auto sd = get_single_delimiter_type(m_peek_token);
          if(sd != single_delimiter_::invalid)
          {
            schedule_parse([this]() { return parse_delimiter(); });
            return true;
          }
          D_PRINT("its invalid!");
          return false;
        }
        case token_type::double_delimiter: 
        {
          D_PRINT("i know that one trying to parse...");
          auto dd = get_double_dlimiter_type(m_peek_token);
          if(dd != double_delimiter_::invalid)
          {
            schedule_parse([this]() { return parse_double_delimiter(); });
            return true;
          }
          return false;
        }
        case token_type::identifire: 
        {
          // identifires work with current token instead of peek
          auto sub_it =
            m_current_command->m_subcommands.find(m_current_token.literal);
          if(m_current_command->m_subcommands.end() != sub_it)
          {
            schedule_parse([this]() { return parse_identifire(); });
            return true;
          }
        }
        case token_type::space:
        default: 
        {
          D_PRINT("nah idon't parse this one!");
          return false;
        }
      }
    }

    single_delimiter_ get_single_delimiter_type(const detail::token& tok) const
    {
      D_PRINT("single_delimiter: Checking if it's an identifier");
      if(tok.type != detail::token_type::identifire)
      {
        return single_delimiter_::invalid;
      }
      D_PRINT("single_delimiter: Done checking identifier");

      // Check if the token is a single character
      D_PRINT("single_delimiter: Checking if it's a single char");
      bool is_single = utf8::is_single_char(tok.literal);
      D_PRINT("single_delimiter: Done checking single char");

      // Alias check (done once for both single and multi-char cases)
      const auto& alias_it =
        m_current_command->m_options_aliases.find(tok.literal);
      if(alias_it != m_current_command->m_options_aliases.end())
      {
        D_PRINT("single_delimiter: It's an alias");
        return single_delimiter_::alias;
      }

      if(is_single)
      {
        // Single-char case: check if it's a flag
        const auto& flag_it = m_current_command->m_flags.find(tok.literal);
        if(flag_it != m_current_command->m_flags.end())
        {
          D_PRINT("single_delimiter: It's a flag");
          return single_delimiter_::flag;
        }
        D_PRINT("single_delimiter: It's invalid (single char)");
        return single_delimiter_::invalid;
      }
      else
      {
        // Multi-char case: check if it's a multi-flag
        if(is_multiflag(tok))
        {
          D_PRINT("single_delimiter: It's a multi-flag");
          return single_delimiter_::mutliflag;
        }
        D_PRINT("single_delimiter: It's invalid (multi-char)");
        return single_delimiter_::invalid;
      }
    }

    double_delimiter_ get_double_dlimiter_type_v0(const detail::token& tok) const
    {
      // we check if tok is a valid dd identifier

      // we are at the identifier [[old]]

      if(tok.type != detail::token_type::identifire)
      // if(m_current_token.type != detail::token_type::identifire)
      {
        // return at the double_delimiter token require advancment
        return double_delimiter_::invalid;
      }

      // alias check
      const auto& alias_it =
        m_current_command->m_options_aliases.find(tok.literal);
      if(m_current_command->m_options_aliases.end() != alias_it)
      {
        // if it's an alias resolve it and forward the resolving state
        return double_delimiter_::alias;
      }

      // it's not an alias so check if it's a real option
      const auto& opt_it = m_current_command->m_options.find(tok.literal);
      if(m_current_command->m_options.end() != opt_it)
      {
        // if it's an option resolve it and forward the resolving state
        return double_delimiter_::option;
      }

      return double_delimiter_::invalid;
    }

    double_delimiter_ get_double_dlimiter_type(const detail::token& tok) const
    {
      if(tok.type != detail::token_type::identifire)
      {
        return double_delimiter_::invalid;
      }

      // Alias check
      const auto& alias_it =
        m_current_command->m_options_aliases.find(tok.literal);
      if(alias_it != m_current_command->m_options_aliases.end())
      {
        return double_delimiter_::alias;
      }

      // Option check
      const auto& opt_it = m_current_command->m_options.find(tok.literal);
      if(opt_it != m_current_command->m_options.end())
      {
        return double_delimiter_::option;
      }

      return double_delimiter_::invalid;
    }

    // deprecated
    bool is_multiflag_v0(const detail::token& tok,
                         bool continue_on_error = false) const
    {
      if(tok.literal.empty())
      {
        return false; // Empty token is not a multi-flag
      }

      size_t pos = 0;
      while(pos < tok.literal.size())
      {
        size_t end_pos = pos;
        if(!utf8::advance_one_char(tok.literal, end_pos))
        {
          return false; // Invalid UTF-8 sequence
        }

        // Extract one character as a potential flag
        auto flag_str = tok.literal.substr(pos, end_pos - pos);
        const auto& flag_it = m_current_command->m_flags.find(flag_str);
        if(flag_it == m_current_command->m_flags.end())
        {
          return false; // One invalid flag means it's not a multi-flag
        }

        pos = end_pos;
      }

      return true; // All characters are valid flags
    }

    bool is_multiflag(const detail::token& tok) const
    {
      if(tok.literal.empty())
      {
        return false; // empty token is not a multi-flag
      }

      std::vector<std::string> valid_flags;
      std::string invalid_flag;

      // parse all characters as potential flags
      size_t pos = 0;
      while(pos < tok.literal.size())
      {
        // treat each character as a single flag (no utf-8 handling)
        std::string flag_str = tok.literal.substr(pos, 1);
        const auto& flag_it = m_current_command->m_flags.find(flag_str);

        if(flag_it != m_current_command->m_flags.end())
        {
          valid_flags.push_back(flag_str); // valid flag found
        }
        else
        {
          invalid_flag = flag_str; // track invalid flag
          if(m_multiflag_policy == multiflag_policy::strict)
          {
            return false; // strict mode: fail on first invalid flag
          }
        }
        pos++;
      }

      // handle policy
      switch(m_multiflag_policy)
      {
        case multiflag_policy::strict:
          // if we reach here, all flags are valid (due to early return above)
          return !valid_flags.empty();
        case multiflag_policy::lax:
          // accept if at least one valid flag exists
          return !valid_flags.empty();
        case multiflag_policy::validate_all:
          // all characters must be valid flags (no invalid flags found)
          return invalid_flag.empty() && !valid_flags.empty();
        default:
          return false; // unknown policy, fail safely
      }
    }
    /*

    bool parse_value_s_v0(std::string& val, bool allow_multiple, bool& r)
    {
      // we are now at the token after the identifire (might be assign or space)
      // or both

      // if its a space or assign skip it
      if(m_current_token.type == detail::token_type::space)
        advance(); // skip space TODO: or = (chnages the parser)
      if(m_current_token.type == detail::token_type::assign) // require a value
      {
        r = true;
        advance();
      }

      // we are now at the value(s) token

      // idk if this should be only identifire?
      if(m_current_token.type == detail::token_type::eof)
      {
        set_unexpected_eof();
        return false;
      }

      if(allow_multiple)
      {
        const auto& args = parse_list();
        if(!args.empty())
          val = args;
        else
        {
          set_missing_value(m_current_token.literal);
          // require advancment
          return false;
        }
      }
      else
      {
        val = m_current_token.literal;
        advance();
      }
      return true;
    }

    bool parse_value_s_v1(std::string& val, int min, int max)
    {
      if(m_current_token.type == detail::token_type::space)
        advance();

      if(m_current_token.type == detail::token_type::assign)
        advance();

      if(m_current_token.type == detail::token_type::eof)
      {
        set_missing_value(m_current_token.literal); // or whatever context
        return false;
      }

      if(min > 1)
      {
        val = parse_list(min, max);
        if(val.empty())
        {
          set_missing_value(m_current_token.literal);
          return false;
        }
      }
      else if(min == 1)
      {
        val = m_current_token.literal;
        advance();
      }
      else
      {
        return false; // doesn't require any value
      }
      return true;
    }

    bool parse_value_s_v1_1(value_storage& val, int min, int max)
    {
      if(m_current_token.type == detail::token_type::space)
        advance();
      if(m_current_token.type == detail::token_type::assign)
        advance();
      if(m_current_token.type == detail::token_type::eof)
      {
        set_missing_value(m_current_token.literal);
        return false;
      }

      std::vector<std::string> values;
      if(min > 0)
      {
        std::string list = parse_list(min, max);
        if(list.empty())
        {
          set_missing_value(m_current_token.literal);
          return false;
        }
        // Split list by spaces (or custom separator later)
        size_t pos = 0;
        while(pos < list.size())
        {
          size_t end = list.find(' ', pos);
          if(end == std::string::npos)
            end = list.size();
          values.push_back(list.substr(pos, end - pos));
          pos = end + 1;
        }
      }
      val = value_storage(std::move(values));
      return true;
    }
*/
    bool parse_value_s_(value_storage& val, int min, int max)
    {
      if(m_current_token.type == detail::token_type::space) 
        advance();
      if(m_current_token.type == detail::token_type::assign) 
        advance();
      
      if(m_current_token.type == detail::token_type::eof) 
      {
        set_unexpected_eof();
        return false;
      }

      if(min > 0) 
      {
        std::vector<std::string> values = parse_list(min, max);
        D_PRINT("parse list returned: ");
        for (const auto& val : values)
          D_PRINT("val: " << val);
        if(values.empty()) 
        {
          set_missing_value(m_current_token.literal);
          return false;
        }
        val.set_values(values);
      }
      return true;
    }

    bool parse_value_s__(value_storage& val, int min, int max) 
    {
      if(m_current_token.type == detail::token_type::space) 
        advance();
      if(m_current_token.type == detail::token_type::assign) 
        advance();
      if(m_current_token.type == detail::token_type::eof) 
      {
        set_missing_value(m_current_token.literal);
        return false;
      }

      auto values = parse_list(min, max);
      val.set_values(std::move(values));
      if(!val.is_valid()) 
      {
        set_missing_value("required argument missing");
        return false;
      }
      return true;
    }
 
    parsed_values parse_values(int min, int max)
    {
      parsed_values res;
      if(max == -1)
        max = INT_MAX;
      int value_count = 0;

      if(m_current_token.type == detail::token_type::space) 
        advance();
      if(m_current_token.type == detail::token_type::assign) 
        advance();
      if(m_current_token.type == detail::token_type::space) 
        advance();

      while(m_current_token.type != detail::token_type::eof && value_count < max)
      {
        if(m_current_token.type == detail::token_type::space)
        {
          advance();
          continue;
        }
        if(parse_known())
          break;
        if(m_current_token.type != detail::token_type::identifire && 
            m_current_token.type != detail::token_type::string)
        {
          set_unexpected_token(m_current_token.literal, m_current_token.start_pos);
          res.valid = false;
          return res;
        }
        res.values.push_back(m_current_token.literal);
        advance();
        value_count++;
      }
      return res;
    }

    // checks if peek_token is equal to tt (token wise) if so it advances.
    // returns true if so,
    // false otherwise
    bool expect_next(detail::token_type tt)
    {
      if(m_peek_token.type == tt)
      {
        advance();
        return true;
      }
      return false;
    }

    bool peek_next(detail::token_type tt)
    {
      if(m_peek_token.type == tt)
        return true;
      return false;
    }

    // checks if peek_token is equal to cp (unicode wise) if so it advances
    // returns true if so,
    // false otherwise
    bool expect_next(utf8::code_point cp)
    {
      // TODO: check me
      size_t pos = 0;
      if(utf8::decode(m_peek_token.literal, pos) == cp &&
         pos >= m_peek_token.literal.size() - 1)
      {
        advance();
        return true;
      }
      return false;
    }

    template <typename FUN> 
    void schedule_parse(FUN&& fun)
    {
      m_to_parse.emplace_back(fun);
    }

    // changes the m_current_resault_command->and the m_current_command to
    // curr_cmd_res and curr_cmd_build
    void change_command(command& curr_cmd_res, command_builder& curr_cmd_build)
    {
      m_current_resault_command = &curr_cmd_res;
      m_current_command = &curr_cmd_build;
    }

    // adds opt with the name 'name' to
    // m_current_resault_command->m_options[name]
    option& save_option(const std::string& name, const option& opt)
    {
      m_current_resault_command->m_options[name] = opt;
      return m_current_resault_command->m_options[name];
    }

    // adds cmd with the name 'name' to
    // m_current_resault_command->m_commands[name]
    command& save_command(const std::string& name, const command& cmd)
    {
      m_current_resault_command->m_commands[name] = cmd;
      return m_current_resault_command->m_commands[name];
    }

    // adds f with the name 'name' to m_current_resault_command->m_flags[name]
    flag& save_flag(const std::string& name, const flag& f)
    {
      m_current_resault_command->m_flags[name] = f;
      return m_current_resault_command->m_flags[name];
    }

    // errors

    void set_unexpected_token(const std::string& literal, int pos)
    {
      m_current_error->type = error_type::parse_error;
      m_current_error->code = error_code::unexpected_token;
      m_current_error->message =
        std::format("Unexpected token '{}' at position {}", literal, pos);
    }

    void set_missing_value(const std::string& name)
    {
      m_current_error->type = error_type::parse_error;
      m_current_error->code = error_code::missing_value;
      m_current_error->message =
        std::format("Option '{}' requires a value, but none was provided", name);
    }

    void set_invalid_flag(const std::string& name)
    {
      m_current_error->type = error_type::parse_error;
      m_current_error->code = error_code::invalid_flag;
      m_current_error->message = std::format("Unknown flag '{}'", name);
    }

    void set_invalid_option(const std::string& name)
    {
      m_current_error->type = error_type::parse_error;
      m_current_error->code = error_code::invalid_option;
      m_current_error->message = std::format("Unknown option '{}'", name);
    }

    void set_invalid_subcommand(const std::string& name)
    {
      m_current_error->type = error_type::parse_error;
      m_current_error->code = error_code::invalid_subcommand;
      m_current_error->message = std::format("Unknown subcommand '{}'", name);
    }

    void set_invalid_alias(const std::string& name)
    {
      m_current_error->type = error_type::parse_error;
      m_current_error->code = error_code::invalid_alias;
      m_current_error->message =
        std::format("Alias '{}' is not recognized or misused", name);
    }

    void set_malformed_multiflag(const std::string& literal,
                                 const std::string& invalid_char)
    {
      m_current_error->type = error_type::parse_error;
      m_current_error->code = error_code::malformed_multiflag;
      m_current_error->message = std::format(
        "Multi-flag '{}' contains invalid flag '{}'", literal, invalid_char);
    }

    void set_utf8_error(const std::string& literal)
    {
      m_current_error->type = error_type::parse_error;
      m_current_error->code = error_code::utf8_error;
      m_current_error->message =
        std::format("Invalid UTF-8 sequence in '{}'", literal);
    }

    void set_missing_identifier(const std::string& delimiter,
                                const std::string& found)
    {
      m_current_error->type = error_type::parse_error;
      m_current_error->code = error_code::missing_identifier;
      m_current_error->message = std::format(
        "Expected identifier after '{}' but found '{}'", delimiter, found);
    }

    void set_unexpected_eof()
    {
      m_current_error->type = error_type::parse_error;
      m_current_error->code = error_code::unexpected_eof;
      m_current_error->message =
        std::format("Unexpected end of input while parsing");
    }

    void set_invalid_single_delimiter(const std::string& literal)
    {
      m_current_error->type = error_type::parse_error;
      m_current_error->code = error_code::invalid_single_delimiter;
      m_current_error->message =
        std::format("Invalid single delimiter '{}'", literal);
    }

    void set_invalid_double_delimiter(const std::string& literal)
    {
      m_current_error->type = error_type::parse_error;
      m_current_error->code = error_code::invalid_double_delimiter;
      m_current_error->message =
        std::format("Invalid double delimiter '{}'", literal);
    }

    void set_invalid_identifier(const std::string& literal)
    {
      m_current_error->type = error_type::parse_error;
      m_current_error->code = error_code::invalid_identifier;
      m_current_error->message =
        std::format("Invalid identifier '{}'", literal);
    }

    void set_empty_literal(uint32_t pos)
    {
      m_current_error->type = error_type::parse_error;
      m_current_error->code = error_code::empty_literal;
      m_current_error->message =
        std::format("Empty literal in unexpected position {}", pos);
    }

  private:
    detail::lexer m_lx;
    command_builder
      m_root; // root command (doesn't allow any value) update: why not?!
              // this would be the interface that builds the whole application
    detail::token m_current_token, m_peek_token;
    command_builder* m_current_command{&m_root};

    parse_resault m_resault;
    command* m_current_resault_command{&m_resault.root};
    std::vector<std::function<bool(void)>> m_to_parse;
    bool m_continue_on_error{false};
    error* m_current_error;
    multiflag_policy m_multiflag_policy{multiflag_policy::validate_all};
  };
} // namespace clara::inline v_0_0_0

#endif // CLARA_HPP
