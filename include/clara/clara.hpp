#ifndef CLARA_HPP
#define CLARA_HPP

#pragma once

#include <cstring>
#include <memory>
#include <cstdint>
#include <expected>
#include <functional>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include <sstream>
#include <string>
#include <string_view>
#include <span>

#define D_PRINT(x) std::cout << x << "\n";

#if defined(__linux__)
  #if defined(__ANDROID__) 
    #define CLARA_ANDROID
  #else 
    #define CLARA_LINUX
  #endif //defined(__ANDROID__)
#elif defined(_WIN32)
  #define CLARA_WINDOWS
  #if defined(_WIN64) 
    #define CLARA_WINDOWS_64
  #endif //defined(_WIN64)
#elif defined(__APPLE__) || defined(__MACH__)
	#include <TargetConditionals.h>
  #define CLARA_APPLE
  #if TARGET_IPHONE_SIMULATOR == 1 
    #define CLARA_IPHONE_SIMULATOR
    #pragma warning untested platform "iphone simulator"
  #elif TARGET_OS_IPHONE == 1 
    #define CLARA_IPHONE 
    #pragma warning untested platform "iphone"
  #elif TARGET_OS_MAC == 1
    #define CLARA_MACOS
    #pragma warning untested platform "macos"
  #else 
    #pragma error unknown apple platform
  #endif //TARGET_IPHONE_SIMULATOR == 1
#else 
  #define CLARA_PLATFORM_UNKNOWN
  #pragma error unknown platform utf-8 will be assumed
#endif //defined(__linux__)

#if not defined(CLARA_WINDOWS)

  #define CLARA_UTF8

  #define CLARA_DELIMITER "-"
  #define CLARA_DOUBLE_DELIMITER "--" 


#else // => windows


  #define CLARA_DELIMITER U'/'
  #define CLARA_DOUBLE_DELIMITER "//" 

#endif

namespace clara::inline v_0_0_0
{
  namespace detail
  {
    inline std::string join_args(int argc, char** argv)
    {
      //seems rather unnatural but it's for the best!
      std::stringstream ss;
      for(auto i = 1; i < argc; i++) //no need for launch command name
      {
        ss << argv[i] << " ";
      }
      return ss.str();
    }
  } //namespace detail
    

namespace utf8 {
    using code_point = uint32_t;

    // Constants for UTF-8 bit patterns
    namespace detail {
        constexpr uint8_t ASCII_MASK = 0x80;
        constexpr uint8_t TWO_BYTE_MASK = 0xE0;
        constexpr uint8_t TWO_BYTE_SIG = 0xC0;
        constexpr uint8_t THREE_BYTE_MASK = 0xF0;
        constexpr uint8_t THREE_BYTE_SIG = 0xE0;
        constexpr uint8_t FOUR_BYTE_MASK = 0xF8;
        constexpr uint8_t FOUR_BYTE_SIG = 0xF0;
        constexpr uint8_t CONTINUATION_MASK = 0xC0;
        constexpr uint8_t CONTINUATION_SIG = 0x80;

        // Get the expected byte count for a UTF-8 sequence based on the first byte
        inline size_t get_sequence_length(uint8_t first_byte) {
            if ((first_byte & ASCII_MASK) == 0) return 1;
            if ((first_byte & TWO_BYTE_MASK) == TWO_BYTE_SIG) return 2;
            if ((first_byte & THREE_BYTE_MASK) == THREE_BYTE_SIG) return 3;
            if ((first_byte & FOUR_BYTE_MASK) == FOUR_BYTE_SIG) return 4;
            return 0; // Invalid
        }

        // Validate continuation bytes in a UTF-8 sequence
        inline bool validate_continuation(std::string_view input, size_t pos, size_t count) {
            for (size_t i = 1; i < count; ++i) {
                if (pos + i >= input.size() || 
                    (static_cast<uint8_t>(input[pos + i]) & CONTINUATION_MASK) != CONTINUATION_SIG) {
                    return false;
                }
            }
            return true;
        }
    }

    // Check if the entire string is valid UTF-8
    inline bool is_valid_utf8(std::string_view input) {
        size_t pos = 0;
        while (pos < input.size()) {
            uint8_t first_byte = static_cast<uint8_t>(input[pos]);
            size_t bytes = detail::get_sequence_length(first_byte);
            if (bytes == 0 || pos + bytes > input.size() || 
                !detail::validate_continuation(input, pos, bytes)) {
                return false;
            }
            pos += bytes;
        }
        return true;
    }

    // Advance position by one UTF-8 character
    inline bool advance_one_char(std::string_view input, size_t& pos) {
        if (pos >= input.size()) return false;
        uint8_t first_byte = static_cast<uint8_t>(input[pos]);
        size_t bytes = detail::get_sequence_length(first_byte);
        if (bytes == 0 || pos + bytes > input.size() || 
            !detail::validate_continuation(input, pos, bytes)) {
            return false;
        }
        pos += bytes;
        return true;
    }


    inline code_point decode(std::string_view input, size_t& pos) 
    {
      if(pos >= input.size()) return 0;

      uint8_t byte = static_cast<uint8_t>(input[pos]);
      pos++;

      if(byte <= 0x7f) return byte; // ascii

      code_point cp = 0;
      int extra_bytes = 0;

      if((byte & 0xe0) == 0xc0) // 2 byte sequence 
      {
        cp = byte & 0x1f;
        extra_bytes = 1;
      } 
      else if((byte & 0xf0) == 0xe0)  // 3 byte sequence
      {
        cp = byte & 0x0f;
        extra_bytes = 2;
      } 
      else if((byte & 0xf8) == 0xf0)  // 4 byte sequence
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
        if(pos >= input.size()) return U'?'; // incomplete sequence
        byte = static_cast<uint8_t>(input[pos++]);
        if((byte & 0xc0) != 0x80) return U'?'; // invalid continuation byte
        cp = (cp << 6) | (byte & 0x3f);
      }
      return cp;
    }

    /*
    // Decode a UTF-8 sequence into a code point
    inline code_point decode(std::string_view input, size_t& pos) {
        if (pos >= input.size()) return U'?';
        uint8_t first_byte = static_cast<uint8_t>(input[pos]);
        size_t bytes = detail::get_sequence_length(first_byte);
        if (bytes == 0 || pos + bytes > input.size() || 
            !detail::validate_continuation(input, pos, bytes)) {
            pos++; // Skip invalid byte
            return U'?';
        }

        code_point cp = 0;
        switch (bytes) {
            case 1: return first_byte;
            case 2: cp = first_byte & 0x1F; break;
            case 3: cp = first_byte & 0x0F; break;
            case 4: cp = first_byte & 0x07; break;
        }

        for (size_t i = 1; i < bytes; ++i) {
            cp = (cp << 6) | (static_cast<uint8_t>(input[pos + i]) & 0x3F);
        }
        pos += bytes;
        return cp;
    }

    */
    // Encode a code point into UTF-8
    inline std::string encode(code_point cp) {
        if (cp <= 0x7F) return {static_cast<char>(cp)};
        if (cp <= 0x7FF) return {
            static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)),
            static_cast<char>(0x80 | (cp & 0x3F))
        };
        if (cp <= 0xFFFF) return {
            static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)),
            static_cast<char>(0x80 | ((cp >> 6) & 0x3F)),
            static_cast<char>(0x80 | (cp & 0x3F))
        };
        if (cp <= 0x10FFFF) return {
            static_cast<char>(0xF0 | ((cp >> 18) & 0x07)),
            static_cast<char>(0x80 | ((cp >> 12) & 0x3F)),
            static_cast<char>(0x80 | ((cp >> 6) & 0x3F)),
            static_cast<char>(0x80 | (cp & 0x3F))
        };
        return "?"; // Invalid code point
    }

    // Check if a code point is a letter (simplified)
    inline bool is_letter(code_point cp) {
        // ASCII letters and underscore
        if ((cp >= U'a' && cp <= U'z') || (cp >= U'A' && cp <= U'Z') || cp == U'_') return true;
        // Common Unicode ranges for letters
        if ((cp >= 0x00C0 && cp <= 0x00FF) || // Latin-1 Supplement
            (cp >= 0x0100 && cp <= 0x017F) || // Latin Extended-A
            (cp >= 0x0180 && cp <= 0x024F) || // Latin Extended-B
            (cp >= 0x0370 && cp <= 0x03FF) || // Greek and Coptic
            (cp >= 0x0400 && cp <= 0x04FF) || // Cyrillic
            (cp >= 0x0600 && cp <= 0x06FF) || // Arabic
            (cp >= 0x0750 && cp <= 0x077F) || // Arabic Supplement
            (cp >= 0x08A0 && cp <= 0x08FF) || // Arabic Extended-A
            (cp >= 0xFB50 && cp <= 0xFDFF) || // Arabic Presentation Forms-A
            (cp >= 0xFE70 && cp <= 0xFEFF))   // Arabic Presentation Forms-B
            return true;
        return false; // TODO: Full Unicode categories (Lu, Ll, Lt, Lm, Lo)
    }

    inline bool is_digit(code_point cp) {
        return cp >= U'0' && cp <= U'9';
    }

    // Check if string is a valid number (integer or float)
    inline bool is_number(std::string_view str, bool allow_float = true) {
        if (str.empty()) return false;
        size_t pos = 0;

        // Optional sign
        if (pos < str.size() && (str[pos] == '+' || str[pos] == '-')) pos++;
        if (pos >= str.size()) return false;

        bool has_digits = false, has_decimal = false, has_exponent = false;

        // Special case: starts with decimal point (e.g., ".3")
        if (allow_float && pos < str.size() && str[pos] == '.') {
            has_decimal = true;
            pos++;
        }

        // Parse digits
        while (pos < str.size() && is_digit(decode(str, pos))) has_digits = true;

        // Decimal point
        if (allow_float && !has_decimal && pos < str.size() && str[pos] == '.') {
            has_decimal = true;
            pos++;
            while (pos < str.size() && is_digit(decode(str, pos))) has_digits = true;
        }

        // Exponent
        if (allow_float && pos < str.size() && (str[pos] == 'e' || str[pos] == 'E')) {
            has_exponent = true;
            pos++;
            if (pos < str.size() && (str[pos] == '+' || str[pos] == '-')) pos++;
            bool exp_digits = false;
            while (pos < str.size() && is_digit(decode(str, pos))) exp_digits = true;
            if (!exp_digits) return false;
        }

        return has_digits && pos == str.size();
    }

    inline bool is_float(std::string_view str) {
        return is_number(str, true);
    }

    inline bool is_integer(std::string_view str) {
        return is_number(str, false);
    }

    // Check if a code point is an emoji
    inline bool is_emoji(code_point cp) {
        return (cp >= 0x1F300 && cp <= 0x1F5FF) || // Miscellaneous Symbols and Pictographs
               (cp >= 0x1F600 && cp <= 0x1F64F) || // Emoticons
               (cp >= 0x1F680 && cp <= 0x1F6FF) || // Transport and Map Symbols
               (cp >= 0x1F900 && cp <= 0x1F9FF) || // Supplemental Symbols
               (cp >= 0x1FA70 && cp <= 0x1FAFF) || // Extended Pictographic
               (cp >= 0x1F3FB && cp <= 0x1F3FF) || // Skin Tone Modifiers
               (cp >= 0x1F1E6 && cp <= 0x1F1FF);   // Regional Indicators (Flags)
    }

    // Check if string is a valid identifier
    inline bool is_identifier(std::string_view str) {
        if (str.empty()) return false;
        size_t pos = 0;
        if (!is_letter(decode(str, pos))) return false;
        return true; // TODO: Optionally check remaining chars
    }

    // Check if string is a single UTF-8 character
    inline bool is_single_char(std::string_view str) {
        if (str.empty()) return false;
        uint8_t first_byte = static_cast<uint8_t>(str[0]);
        size_t bytes = detail::get_sequence_length(first_byte);
        return bytes != 0 && str.length() == bytes && 
               detail::validate_continuation(str, 0, bytes);
    }
  } // namespace utf8
 
  namespace detail
  {
    using code_point = uint32_t;

    enum class token_type : uint8_t
    {
      /*unknown, illegal,*/ eof, identifire, 
      //number,
      assign,
      string, space,
      delimiter, double_delimiter
    };

    struct token 
    {
      token() = default;
      token(token_type t, const char ch) //for ascii and tokens with single ascii char
        : type(t), literal(std::string(1, ch))
      {
      }

      token(token_type t, const std::string_view str) //utf-8 and tokens with muliple chars
        : type(t), literal(str)
      {
      }

      token_type type;
      std::string literal;
    };

    constexpr std::string_view token_type_to_string(const token_type tt)
    {
      using namespace std::string_view_literals;
      switch(tt)
      {
        //case token_type::illegal:          return "illegal"sv;
        case token_type::eof:              return "eof"sv;
        case token_type::identifire:       return "identifire"sv;
        //case token_type::number:           return "number"sv;
        case token_type::string:           return "string"sv;
        case token_type::space:            return "space"sv;
        case token_type::assign:           return "assign"sv;
        case token_type::delimiter:        return "delimiter"sv;
        case token_type::double_delimiter: return "double_delimiter"sv;
        //default:                           return "unknown"sv;
      }
    }

    class lexer
    {
    public:
      using input_type = std::string_view;
      using delimiter_set = std::unordered_set<std::string_view>;
    public:
      lexer() = default;
      lexer(const input_type input, const delimiter_set& ds = make_default_delimiters_set())
        : m_input(input), m_delimiters(ds)
      {
        m_current = read_char();
      }

      token advance()
      {
        using namespace std::string_view_literals;
        
        consume_whitespace();
        token tok;

        switch(m_current)
        {
          case U'=':
          {
            //skip it
            //read_char();
            //return advance();
            tok = { token_type::assign, "="sv };
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
          case U' ':
          case U'\t':
          case U'\n':
          {
            tok.type = token_type::space;
            tok.literal = " ";
            break;
          }
          case 0:
          {  
            tok = { token_type::eof, ""sv };
            break;
          }
          default:
          {
            //always return from here on, we don't need to read_char at the end
            if(is_delimiter(m_current))
            {
              auto pc = peek_char();
              if(is_delimiter(pc))
              {
                tok.type = token_type::double_delimiter;
                tok.literal = utf8::encode(m_current) + utf8::encode(pc);
                read_char(); //skip the next delimiter (it's part of double_delimiter)
              }
              else 
              {
                tok.type = token_type::delimiter;
                tok.literal = utf8::encode(m_current);
              }
              m_current = read_char();
              return tok;
            }
            else if(utf8::is_valid_utf8(utf8::encode(m_current)))//(utf8::is_identifier(utf8::encode(m_current)))
            {
              tok.literal = read_identifire();
              tok.type = token_type::identifire;
              return tok;
            }
            //if(utf8::is_number(utf8::encode(m_current)))
            //{
            //  //TODO: floats/ints/uints //or just number as double idk
            //  tok.literal = read_number();
            //  tok.type = token_type::number;
            //  return tok;
            //}
            //{
            //  //convert to utf-8 for the literal 
            //  std::string literal;
            //  if(m_current <= 0x7f)
            //    literal = std::string(1, static_cast<char>(m_current));
            //  else //TODO: FIXME: proper utf-8 encoding
            //    literal = utf8::encode(m_current);
            //  
            //  tok = { token_type::illegal, literal };
            //}
            break;
          }
        }
        m_current = read_char();
        return tok;
      }
    private:
      code_point read_char()
      {
        if(m_read_position >= m_input.size())
          return 0; //eof

        auto pos = m_read_position;
        code_point ch = utf8::decode(m_input, pos);
        m_position = m_read_position;
        m_read_position = pos;
        return ch;
      }

      code_point peek_char() const
      {
        if(m_read_position >= m_input.size())
          return 0;
        auto pos = m_read_position;
        return utf8::decode(m_input, pos);
      }
     
      //all read_xxx functions return string_view because it's just reading from m_input and there is no need for another copy (other than in token)
      std::string_view read_identifire()
      {
        bool f{false};
        size_t pos = m_position;
        
        //allow identifires to have number in them but not the first char like: foo_1 but not like: 1_foo (tricky to lex)
        while(!std::isspace(m_current & 0xff) && utf8::is_valid_utf8(utf8::encode(m_current)))//utf8::is_letter(m_current) || utf8::is_digit(m_current) || utf8::is_emoji(m_current))
        //while(utf8::is_valid_utf8(utf8::encode(m_current)))
        {
          if(m_current == U'=')
          {
            f = true;
            break; //TODO: validate this pls
          }
          m_current = read_char();
        } 
        const auto& sub = m_input.substr(pos, m_position - pos);
        return sub;
      }

      std::string_view read_number()
      {
        size_t pos = m_position;
        //TODO: add support for floats and sceintific notation and hex, oct, etc..
        while(utf8::is_digit(m_current))
        {
          m_current = read_char();
        }
        return m_input.substr(pos, m_position - pos);
      }

      std::string_view read_string(code_point open)
      {
        m_current = read_char(); //skip open
        size_t pos = m_position; //after open 
        
        while(true)
        {
          if(m_current == open)
            break;
          else if (m_current == U'\0') //same as 0 but to be explicit
            break; //TODO: lex error unterminated string 
                   //TODO: don't allow stupidly long strings 
          m_current = read_char();
        }
        return m_input.substr(pos, m_position - pos);
      }

      void consume_whitespace()
      {
        //only redundent whitespace
        while(std::isspace(m_current & 0xff) && peek_char() != 0 && std::isspace(peek_char() & 0xff)) 
        {
          m_current = read_char();
        }
        //while(std::isspace(m_current & 0xff) || m_current == U'\n' || m_current == U'\t')
        //{
        //  m_current = read_char();
        //}
      }

      bool is_delimiter(code_point cp)
      {
        auto r = m_delimiters.find(utf8::encode(cp)) != m_delimiters.end();
        return r;
      }

      static delimiter_set make_default_delimiters_set()
      {
        return delimiter_set{ 
          CLARA_DELIMITER
        };
      }
    private:
      input_type m_input;
      size_t m_position{ 0 };
      size_t m_read_position{ 0 };
      code_point m_current{ 0 };
      delimiter_set m_delimiters;
    };

  } //namespace detail

  namespace parse 
  {
    class option_builder;
    
    template <typename T> 
    std::pair<T, bool> get(const std::string& value)
    {
      static_assert(false, "no template specialization found");
    }

    template <>
    inline std::pair<std::string, bool> get(const std::string& value)
    {
      return { value, true };
    }

    template <>
    inline std::pair<int64_t, bool> get(const std::string& value)
    {
      int64_t i64 = 0;
      try {
        i64 = std::stol(value);
      } catch(...) {
        return { 0, false };
      }
      return { i64, true };
    }

    template <>
    inline std::pair<uint64_t, bool> get(const std::string& value)
    {
      uint64_t u64 = 0;
      try {
        u64 = std::stoul(value);
      } catch(...) {
        return { 0, false };
      }
      return { u64, false };
    }

    template <>
    inline std::pair<double, bool> get(const std::string& name)
    {
      double d = 0.0f;
      try {
        d = std::stod(name);
      } catch(...) {
        return { 0.0f, false };
      }
      return { d, true };
    }

    class command_builder;

    class option_builder
    {
    public:
      using default_value_type = std::string;
      enum class alias_options
      {
        off, single_delimiter, double_delimiter, //TODO: (or maybe not) as_name
        _default = single_delimiter
      };
    public:
      option_builder& requires_value() // e.g. g++ -o a.out
      {
        m_requires_value = true;
        return *this;
      }

      option_builder& allow_multiple()
      {
        m_allows_multiple = true;
        return *this;
      }

      option_builder& set_alias(const std::string& name); //via the parent

      option_builder& set_alias_options(alias_options opts)
      {
        m_alias_opts = opts;
        return *this;
      }
      option_builder() = default;
    private:
      option_builder(command_builder* parent, const std::string& name)
        : m_parent(parent), m_name(name)
      {
      }
    private:
      bool m_requires_value{ false };
      bool m_allows_multiple{ false };
      alias_options m_alias_opts{ alias_options::_default };
      command_builder* m_parent{ nullptr };
      std::string m_name;
    private:
      friend class parser;
      friend class command_builder;
    };


    class command_builder 
    {
    public:
      using default_value_type = std::string;
    public:
      
      //new one for the added command
      command_builder& add_subcommand(const std::string& name) // e.g. git submodule
      {
        auto& sub = m_subcommands[name];
        sub.m_debug_name = name; // debug only
        return sub;
      }

      option_builder& add_option(const std::string& name) // e.g. git --version
      {
        auto& opt = m_options[name];
        opt = option_builder{ this, name };
        return opt;
      }

      command_builder& add_flag(const std::string& name) // must be one utf-8 char 
                                                         // e.g git -v
      {
        m_flags.emplace(name);
        return *this;
      }

      command_builder& requires_value() // e.g. git submodule add <url> <path> 
                                        // this could require a value of type string array
      {
        m_requires_value = true;
        return *this;
      }

      command_builder& allows_multiple()
      {
        m_allows_multiple = true;
        return *this;
      }

      command_builder& set_option_alias(const std::string& alias_name, const std::string& option_name) //TODO: clash resulotion config
      {
        m_options_aliases[alias_name] = option_name;
        return *this;
      }
    public://private:
      command_builder() = default;
    private:
      std::string m_debug_name;
      std::unordered_map<std::string, command_builder> m_subcommands;
      std::unordered_map<std::string, option_builder> m_options;
      std::unordered_set<std::string> m_flags;
      std::unordered_map<std::string, std::string> m_options_aliases;// { alias : option } 
      bool m_requires_value{ false };
      bool m_allows_multiple{ false };
    private:
      friend class parser;
    };
 
    inline option_builder& option_builder::set_alias(const std::string& name)
    {
      m_parent->set_option_alias(name, m_name);
      return *this;
    }


    /*
    class hashable 
    {
    public:
      hashable() = default;
      hashable(std::string_view name)
        : m_name(name)
      {
      }

      const std::string& get_name() const
      {
        return m_name;
      }

      bool operator == (const hashable& h)
      {
        return m_name == h.m_name;
      }
    protected:
      std::string m_name;
    };

    struct hashable_hash
    {
      size_t operator()(const hashable& hb)
      {
        return std::hash<std::string>{}(hb.get_name());
      }
    };
    */


    // for now options that allows multiple values are final by that i mean you can't use any other option after an option that requires a value.
    // i don't know if this is what it supposed to be or there is another way around but i didn't see any program that do otherwise
    // the other way around is forcing syntax like: 
    // multiple(arrays): --opt = [val1, val2, val3] (or) --opt = [val1 val2 val3] with or without an equals sign
    // maps: --opt = { key1 : value1, key2 : value2 } (or) --opt = { ke1 : value1 key2: value2 } with or without an equal sign
    // this forces keys on maps to don't have space in them or you can use a string literal to do so or we can remove the none comma version
    // maybe allow both and if usage is without brackets assume final otherwise don't (best of both worlds) i guess
    class option //: public hashable
    {
    public:
      enum class state 
      {
        not_convertable, no_value
      };
    public:
      bool has_value() const
      {
        return !m_value.empty();
      }

      const std::string& get_raw() const
      {
        return m_value;
      }

      template <typename T>
      std::expected<T, state> get() const
      {
        if(!m_value.empty())
        {
          auto r = get<T>(m_value);
          if(r.second)
            return r.first;
          
          return std::unexpected(state::not_convertable);
        }
        return std::unexpected(state::no_value);
      }
      
      //allow both '--opt [opt1, opt2, opt3]' and '--opt opt1 opt2 opt3'
      template <typename T>
      std::expected<std::vector<T>, state> get_vector() const
      {
        if(m_value.empty())
          return std::unexpected(state::no_value);
        
        std::vector<T> vec;
        size_t last_d = 0;
        for(auto i = 0; i < m_value.size(); i++)
        {
          if(m_value[i] == ' ')
          {
            auto r = get<T>(m_value.substr(last_d, i));
            if(r.second)
            {
              vec.emplace_back(r.first);
              last_d = i;
            }
            else 
            {
              return std::unexpected(state::no_value);
            }
          }
        }
        
        if(last_d < m_value.size()) 
        {
          auto r = get<T>(m_value.substr(last_d));
          if (r.second) vec.emplace_back(r.first);
        }

        return vec;
      }

      //TODO: maybe requires map, and usage would be: --opt { key1:value1 key1:value1 }
      //bool operator == (const option& opt) const
      //{
      //  return m_name == opt.m_name;
      //}
    //private:
      option() = default;

      //option(std::string_view name, std::string_view val = "")
      //  : m_name(name), m_value(val)
      //{
      //}
    private:
      //std::string m_name;
      std::string m_value;
    private:
      friend class parser;
    };

    
    // idon't know if flags should allow values or not like: 'g++ -std=c++23'
    // this don't make sense in our implementation because:
    // -1: -std would translate to -s -t -d 
    // -2: flags don't have values either called or not
    // unless aliases would override the default parse behavior
    // only then '-std=c++23' would translate to something like: ' --standard_version=c++23 ' 
    // this is what i'll implement but i have no idea how to do so efficiently
    class flag //: public hashable
    {
    public:
      //bool operator == (const flag& f) const
      //{
      //  return m_name == f.m_name;
      //}
    private:
      //std::string m_name;
    public://private:
      flag() = default;
      //explicit flag(std::string_view name, bool val = false)
      //  : m_name(name), m_value(val)
      //{
      //}
    private:
      friend class parser;
    };

    class command //: public hashable
    {
    public:
      enum class state 
      {
        not_found, not_convertable, no_value
      };
    public: 
      bool has_value() const 
      {
        return !m_value.empty();
      }

      const std::string& get_raw() const
      {
        return m_value;
      }

      template <typename T>
      std::expected<T, state> get() const
      {
        if(!m_value.empty())
        {
          auto r = get<T>(m_value);
          if(r.second)
            return r.first;

          return std::unexpected(state::not_convertable);
        } 
        return std::unexpected(state::no_value);
      }
      
      // allow both '--opt [opt1, opt2, opt3]' and '--opt opt1 opt2 opt3'
      template <typename T>
      std::expected<T, void> get_vector() const
      {
        //FIXME: D.R.Y
        if(m_value.empty())
          return std::unexpected(state::no_value);
      
        std::vector<T> vec;
        size_t last_d = 0;
        for(auto i = 0; i < m_value.size(); i++)
        {
          if(m_value[i] == ' ')
          {
            auto r = get<T>(m_value.substr(last_d, i));
            if(r.second)
            {
              vec.emplace_back(r.first);
              last_d = i;
            }
            else 
            {
              return std::unexpected(state::no_value);
            }
          }
        }
        
        if(last_d < m_value.size()) 
        {
          auto r = get<T>(m_value.substr(last_d));
          if (r.second) vec.emplace_back(r.first);
        }

        return vec;
      }

      std::expected<std::reference_wrapper<command>, state> get_command(const std::string& name)
      {
        const auto cmd_it = m_commands.find(name);
        if(m_commands.end() != cmd_it)
          return std::ref(cmd_it->second);

        return std::unexpected(state::not_found);
      }

      std::expected<std::reference_wrapper<option>, state> get_option(const std::string& name)
      {
        const auto opt_it = m_options.find(name);
        if(m_options.end() != opt_it)
          return std::ref(opt_it->second);

        return std::unexpected(state::not_found);
      }

      std::expected<std::reference_wrapper<flag>, state> get_flag(const std::string& name)
      {
        const auto flag_it = m_flags.find(name);
        if(m_flags.end() != flag_it)
          return std::ref(flag_it->second);

        return std::unexpected(state::not_found);
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

      //bool operator == (const command& cmd) const
      //{
      //  return m_name == cmd.m_name;
      //}
    //private:
      command() = default;
      //explicit command(std::string_view name, std::string_view val = "")
      //  : m_name(name), m_value(val)
      //{ 
      //}
    private:
      //std::string m_name;
      std::string m_value;
      std::unordered_map<std::string, command> m_commands;
      std::unordered_map<std::string, option> m_options;
      std::unordered_map<std::string, flag> m_flags;
    private:
      friend class parser;
      friend class parse_resault;
    };

    class parse_resault 
    {
    public:
      parse_resault() = default;

      bool has_error() const;
    public:
      command root;
    };

    class parser 
    {
    public: 
      parser() = default;
      
      // build stage(pre parse)
      
      inline command_builder& add_subcommand(const std::string& name)
      {
        return m_root.add_subcommand(name);
      }
      
      inline option_builder& add_option(const std::string& name)
      {
        return m_root.add_option(name);
      }

      inline void add_flag(const std::string& name)
      {
        m_root.add_flag(name);
      }

      // parse function(only one function)
      // TODO: windowsss(not utf-8)
      parse_resault parse(int argc, char** argv)
      {
        auto args_str = clara::detail::join_args(argc, argv);
        m_lx = detail::lexer{ args_str };

        
        while(m_current_token.type != detail::token_type::eof)
        {
          if(!parse_one())
            advance();
          for(auto f : m_to_parse)
            if(f())
              advance();
          m_to_parse.clear();
        }
        return m_resault; 
      }
    private:
      enum class single_delimiter_ : uint8_t
      {
        invalid, alias, flag, mutliflag
      };

      enum class double_delimiter_ : uint8_t
      {
        invalid, alias, option
      };

      enum class identifier : bool 
      {
        invalid, subcommand
      };
    private:
      void advance()
      {
        m_current_token = m_peek_token;
        m_peek_token = m_lx.advance();
      }

      // returns true if it could parse this current token and changes the state to the next tokens 
      // and returns false if it couldn't parse the current token and it requires the state to be advanced
      bool parse_one()
      {
        using detail::token_type;
        switch(m_current_token.type)
        {
          case token_type::delimiter:
          {
            D_PRINT("parse_delimiter start" << m_current_token.literal << " ");
            auto d = parse_delimiter();
            D_PRINT("parse_delimiter end");
            return d;
          }
          case token_type::double_delimiter:
          {
            D_PRINT("parse_double_delimiter start" << m_current_token.literal << " ");
            auto d = parse_double_delimiter();
            D_PRINT("parse_double_delimiter end");
            return d;
          }
          case token_type::identifire:
          {  
            D_PRINT("parse_identifire start" << m_current_token.literal << " ");
            auto d = parse_identifire();
            D_PRINT("parse_identifire end");
            return d;
          }
          case token_type::space:
          {
            D_PRINT("space: " << m_current_token.literal << " ");
            return false;
          }
          case token_type::assign:
          {
            D_PRINT("assign escaped parsing: " << m_current_token.literal);
            return false;
          }
          default:
          {
            D_PRINT("unknown token cant parse");
            D_PRINT("token: type: " << uint32_t(m_current_token.type) << " literal: '" << m_current_token.literal << "'");
            return false;
          }
        }
      }

      // returns true if it can parse the delimiter and the next tokne and it's requirement(s)
      // returns false otherwise and it requires the token to be advanced externally
      bool parse_delimiter_v0()
      {
        if(!expect_next(detail::token_type::identifire))
        {
          // return at the delimiter token require advancment
          return false; //TODO: error
        }

        // now we are at identifier token we will try to parse it and if we fail 
        // we will return at it and we will require advancment

        //FIXME: there is a repeated alias check D.R.Y

        if(utf8::is_single_char(m_current_token.literal)) //single char => it's either an alias or a real flag
        {
          // check if it's an alias
          const auto& alias_it = m_current_command->m_options_aliases.find(m_current_token.literal);
          if(m_current_command->m_options_aliases.end() != alias_it)
          {
            // if it's an alias resolve it and forward resolving state
            return resolve_alias(true, alias_it);
          }
          
          // it's not an alias so it might be a flag
          
          // flag check
          const auto& flag_it = m_current_command->m_flags.find(m_current_token.literal);
          if(m_current_command->m_flags.end() != flag_it)
          {
            // so it's a flag save it and advance then return true
            //flag f{};
            //m_current_resault_command->m_flags[m_current_token.literal] = f;
            save_flag(m_current_token.literal, {});
            advance();
            return true;
          }
          // not a flag require advancment
          return false;
        }
        else // multi char it's either an alias or multiple flags
        {
          // alias check
          const auto& alias_it = m_current_command->m_options_aliases.find(m_current_token.literal);
          if(m_current_command->m_options_aliases.end() != alias_it)
          {
            // if it's an alias resolve it and forward resolving state
            return resolve_alias(true, alias_it);
          }
          
          // so it must be multi-flag or (error)
          return resolve_multiflag();
        }
      }

      bool parse_delimiter()
      {
        auto tp = get_single_delimiter_type();
        
        if(!expect_next(detail::token_type::identifire))
        {
          // return at the delimiter token require advancment
          return false; //TODO: error
        }

        switch(tp)
        {
          case single_delimiter_::flag:
          {
            save_flag(m_current_token.literal, {});
            advance();
            return true;
          }
          case single_delimiter_::alias:
          {
            const auto& alias_it = m_current_command->m_options_aliases.find(m_current_token.literal);
            return resolve_alias(true, alias_it);
          }
          case single_delimiter_::mutliflag:
          {
            return resolve_multiflag();
          }
          case single_delimiter_::invalid:
          default:
          {
            return false;
          }
        }
      }

      // returns true if it can parse the double_delimiter and the next tokne and it's requirement(s)
      // returns false otherwise and it requires the token to be advanced externally
      bool parse_double_delimiter_v0()
      {
        if(!expect_next(detail::token_type::identifire))
        {
          // return at the double_delimiter token require advancment
          return false;
        }

        // alias check
        const auto& alias_it = m_current_command->m_options_aliases.find(m_current_token.literal);
        if(m_current_command->m_options_aliases.end() != alias_it)
        {
          // if it's an alias resolve it and forward the resolving state
          return resolve_alias(false, alias_it);
        }
        
        // it's not an alias so check if it's a real option
        const auto& opt_it = m_current_command->m_options.find(m_current_token.literal);
        if(m_current_command->m_options.end() != opt_it)
        {
          // if it's an option resolve it and forward the resolving state
          return resolve_option(opt_it->first, opt_it->second);
        }
        else 
        {
          // invalid option require advancment 
          return false;
        }
      }


      bool parse_double_delimiter()
      {
        // we are now at the delimiter token 
        
        if(!expect_next(detail::token_type::identifire))
        {
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
            return false;
          }
        }
      }


      // returns true if it can parse the identifier and it's requirement(s)
      // returns false otherwise and it requires the token to be advanced externally
      bool parse_identifire()
      {
        // stand-alone identifire must be a subcommand 
        
        // subcommand check
        auto sub_it = m_current_command->m_subcommands.find(m_current_token.literal);
        if(m_current_command->m_subcommands.end() != sub_it)
        {
          // it's a subcommand resolve it and forward the resolving state
          return resolve_command(sub_it->first, sub_it->second);
        }
        
        // invalid subcommand require advancment
        return false;
      }

      //this function is bad
      bool resolve_alias(bool is_single_delm_call, const auto& alias_it)
      {
        // if is aliase route to option path
        if(m_current_command->m_options_aliases.end() != alias_it)
        {
          //TODO: missmatch check
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
                return resolve_option(alias_it->second, m_current_command->m_options[alias_it->second]);
            }
            case option_builder::alias_options::single_delimiter:
            {
              if(is_single_delm_call)
                return resolve_option(alias_it->second, m_current_command->m_options[alias_it->second]);
            }
          }
        }
        return false;
      }

      // returns true if it can parse the option and it's requirement(s)
      // returns false otherwise and it requires the token to be advanced externally
      bool resolve_option_v0(const std::string& opt_name, const option_builder& opt_build)
      {       
        bool rv = false;
        option opt;
        D_PRINT("option: current token: " << m_current_token.literal);
        D_PRINT("opttion: " << opt_name << " builder: " << opt_build.m_name << " req val: " << opt_build.m_requires_value);
        if(opt_build.m_requires_value)
        {
          if(expect_next(detail::token_type::eof)) // allow any token to be argument only error on eof (expected next token)
            return false;
          
          advance(); 
          // now at the token after the option identifier

          // if its a space or = skip it
          if(m_current_token.type == detail::token_type::space)
            advance(); //skip space TODO: or = (chnages the parser)
          else if(m_current_token.type == detail::token_type::assign) // require a value 
          {
            advance();
            rv = true;
          }

          // we are now at the option's argument(s)

          if(opt_build.m_allows_multiple)
          {
            const auto& args =  parse_args();
            if(!args.empty())
              opt.m_value = args;
            else 
            {
              // require advancment
              return false;
            }
          }
          else 
          {
            opt.m_value = m_current_token.literal;
            advance();
          }
        }

        save_option(opt_name, opt);
        return true;
      }


      bool resolve_option(const std::string& opt_name, const option_builder& opt_build)
      {
        // we are now at the options identifire  
        option opt;
        if(opt_build.m_requires_value)
        {
          if(expect_next(detail::token_type::eof)) // allow any token to be argument only error on eof (expected next token)
            return false;
          
          advance();

          // we are now at the token after the identifire (might be assign or space) or both

          bool r = false;
          if(!parse_value_s(opt.m_value, opt_build.m_allows_multiple, r))
            return false;
        }
        
        save_option(opt_name, opt);
        return true;
      }


      // returns true if it can parse the multi-flags and resolve each one of them
      // returns false otherwise and it requires the token to be advanced externally
      // TODO: maybe allow to enable resolving the rest of the flags 
      // if one of the flags are not correct,
      // or add some more configurations to this function
      // because the behavior now is if a flag isn't correct 
      // we return immedatly but there might be another flags in front of it 
      // that won't be resolved or even checked.
      bool resolve_multiflag_v0()
      {
        // we are now at the identifier next to the delimiter
        // that we suspect is multiflag

        size_t pos = 0, end_pos = 0;
        bool state = true;
        while(end_pos < m_current_token.literal.size() && state)
        {
          state = utf8::advance_one_char(m_current_token.literal, end_pos);
          const auto& str = m_current_token.literal.substr(pos, end_pos);
          const auto& flag_it = m_current_command->m_flags.find(str);
          pos = end_pos;

          // check if the current flag is valid
          if(m_current_command->m_flags.end() == flag_it)
          {
            // invlaid flag require advancment (skip the whole flags identifier even if some are correct)
            return false;
          }

          add_flag(str);
        }

        // we are done with this identifier advance it
        advance();
        return true;
      }

      bool resolve_multiflag(bool continue_on_error = false, std::vector<std::string>* error_flags = nullptr) {
    // returns true if it can parse the multi-flags and resolve each one of them
    // returns false otherwise and it requires the token to be advanced externally
    // TODO: maybe allow to enable resolving the rest of the flags
    // if one of the flags are not correct,
    // or add some more configurations to this function
    // because the behavior now is if a flag isn't correct
    // we return immedatly but there might be another flags in front of it
    // that won't be resolved or even checked.

    // we are now at the identifier next to the delimiter
    // that we suspect is multiflag

    if (m_current_token.literal.empty()) { //add a check for empty token.
        return true; //Or return false, depending on the desired behavior.
    }
    // TODO: add an option to validate all flags then run them because in some tools
    // running a command with some flags and not the others can be wrong
    // so better to error than to do something you are not supposed to 
    // rm -rf
    size_t pos = 0, end_pos = 0;
    bool all_flags_valid = true; // Use a flag to track overall validity

    while (end_pos < m_current_token.literal.size()) {
        bool advance_success = utf8::advance_one_char(m_current_token.literal, end_pos);
        if (!advance_success) {
            // Handle UTF-8 error (e.g., invalid character)
            // TODO: Add error handling for invalid UTF-8
            if(error_flags){
                error_flags->push_back("Invalid UTF-8 character");
            }
            return false;
        }

        const auto& str = m_current_token.literal.substr(pos, end_pos);
        
        const auto& flag_it = m_current_command->m_flags.find(str);
        pos = end_pos;
        // check if the current flag is not valid
        if (m_current_command->m_flags.end() == flag_it) {
            // invalid flag
            all_flags_valid = false;
            if (error_flags) {
                error_flags->push_back(str); // Report the invalid flag
            }

            if (!continue_on_error) {
                // Stop processing on the first error
                return false;
            }
        } else {
            save_flag(str, {}); // Add the valid flag
        }
    }

    // we are done with this identifier advance it
    advance();
    return true; // Return true even if not all flags are valid 'cause we already advanced 
                 // idk if we should only advance if all are valid? 
}

      // returns true if it can parse the command and it's requirement(s)
      // returns false otherwise and it requires the token to be advanced externally
      bool resolve_command_v0(const std::string& name, command_builder& cmd_build)
      {
        // we are now at the command identifier
        
        command cmd;
        if(cmd_build.m_requires_value)
        {
          if(!expect_next(detail::token_type::eof)) // allow any token to be argument only error on eof (expected next token)
            return false; 

          // we are now at the next token after the option 

          if(cmd_build.m_allows_multiple)
          {
            const auto& args = parse_args();
            if(!args.empty())
              cmd.m_value = args;
            else 
            {
              // require advancement //RIGHT
              return false;
            }
          }
          else
          {
            cmd.m_value = m_current_token.literal;
          }
        }
        else
        {
          advance();
        }
       
        // TODO: refactor this so you add it first and work on the reference instead of
        // copying it again when you add it 
        auto& added_cmd = save_command(name, cmd);

        // change the current command to this (resolved) one 
        change_command(added_cmd, cmd_build);
        //advance(); // idon't know in this case if we should advance 
                   // because of the parse_args (here and in resolve_option)
                   // TODO: check
                   // update: it seems like we dont need to advance 
        return true;
      }

      bool resolve_command(const std::string& name, command_builder& cmd_build)
      {
        // we are now at the command identifier
        
        command cmd;
        if(cmd_build.m_requires_value)
        {
          if(!expect_next(detail::token_type::eof)) // allow any token to be argument only error on eof (expected next token)
            return false; 

          // we are now at the next token after the option 
          bool r = false;
          if(parse_value_s(cmd.m_value, cmd_build.m_allows_multiple, r))
            return false;
        }
        else
        {
          advance();
        }
       
        // TODO: refactor this so you add it first and work on the reference instead of
        // copying it again when you add it 
        auto& added_cmd = save_command(name, cmd);

        // change the current command to this (resolved) one 
        change_command(added_cmd, cmd_build);
        //advance(); // idon't know in this case if we should advance 
                   // because of the parse_args (here and in resolve_option)
                   // TODO: check
                   // update: it seems like we dont need to advance 
        return true;
      }


      // returns argument_list as a space separated list if it can parse the arguments 
      // returns "" otherwise and requires the token to be advanced externally
      std::string parse_args()
      {
        // we are now at the first argument or the '[' if it's using the argument list syntax

        // if the current token is single Unicode char and it's equal to '[' then it's a list of arguments 
        
        //deprecated
        /*
        if(utf8::is_single_char(m_current_token.literal) && m_current_token.literal[0] == U'[')
        {
          // skip the opening '[' 
          advance();

          return parse_arguments_list(U']', U',');
        }
        */
        
        // assume everything else is args (it doesn't now)
        return parse_list();
      }

      // returns argument_list as a space separated list if it can parse argument list
      // it assume everything as argument ( so useres can input arguments with the sam name as a subcommand )
      // if this behavior is not desired pleas use the argument_list syntax e.g. [ arg arg2 arg3 ]
      // im not planning to support backtracking the but if i found it to be useful 
      // i might add it as feature (turned off by default)
      std::string parse_list_v0()
      {
        std::stringstream ss;
        while(m_current_token.type != detail::token_type::eof)
        {
          ss << m_current_token.literal;
          advance();
        }
        return ss.str();
      }

      std::string parse_list()
      {
        // we are now at the first item of the list 

        std::stringstream ss;
        while(m_current_token.type != detail::token_type::eof)
        {
          if(parse_known())
          {
            return ss.str();
          }
          else 
          { 
            ss << m_current_token.literal;
            advance();
          };
        }
        return ss.str();
      }

      std::vector<std::function<bool(void)>> m_to_parse;
      template <typename FUN>
      void schedual_parse(FUN&& fun)
      {
        m_to_parse.emplace_back(fun);
      }

      bool parse_known()
      {
        // we are now at the token that might represnt something known 
        // if so we parse it 
        // else we return false (we don't know it)
        

        D_PRINT("parse known called: with token: " << detail::token_type_to_string(m_current_token.type) << " literal: " << m_current_token.literal);
        using detail::token_type;

        switch(m_current_token.type)
        {
          case token_type::delimiter:
          {
            D_PRINT("i know that one trying to parse...");
            auto sd = get_single_delimiter_type();
            if(sd != single_delimiter_::invalid)
            {
              schedual_parse([this] (){return parse_delimiter();});
              return true;
              //auto d = parse_delimiter();
              //D_PRINT("parse resault: " << (d ? "true" : " false"));
              //return d;
            }
            return false;
          }
          case token_type::double_delimiter:
          {  
            D_PRINT("i know that one trying to parse...");
            auto dd = get_double_dlimiter_type(m_peek_token);
            if(dd != double_delimiter_::invalid)
            {
              schedual_parse([this] (){return parse_double_delimiter();});
              return true;
              //auto d = parse_double_delimiter();
              //D_PRINT("parse resault: " << (d ? "true" : " false"));
              //return d;
            }
            return false;
          }
          case token_type::identifire:
          { 
            auto sub_it = m_current_command->m_subcommands.find(m_current_token.literal);
            if(m_current_command->m_subcommands.end() != sub_it)
            {
              schedual_parse([this] (){return parse_identifire();});
              return true;
            }
            //D_PRINT("i know that one trying to parse...");
            //auto d = parse_identifire();
            //D_PRINT("parse resault: " << (d ? "true" : " false"));
            //return d;
          }
          case token_type::space:
          default:
          {
            D_PRINT("nah idon't parse this one!");
            return false;
          }
        }
      }

     single_delimiter_ get_single_delimiter_type()
     {
       // now we are at identifier token we will try to parse it and if we fail 
        // we will return at it and we will require advancment

       single_delimiter_ type;
       if(m_current_token.type != detail::token_type::identifire)
        {
          // return at the delimiter token require advancment
          return single_delimiter_::invalid; //TODO: error
        }

       
        //FIXME: there is a repeated alias check D.R.Y

        if(utf8::is_single_char(m_current_token.literal)) //single char => it's either an alias or a real flag
        {
          // check if it's an alias
          const auto& alias_it = m_current_command->m_options_aliases.find(m_current_token.literal);
          if(m_current_command->m_options_aliases.end() != alias_it)
          {
            // if it's an alias resolve it and forward resolving state
            return single_delimiter_::alias;
          }
          
          // it's not an alias so it might be a flag
          
          // flag check
          const auto& flag_it = m_current_command->m_flags.find(m_current_token.literal);
          if(m_current_command->m_flags.end() != flag_it)
          {
            // so it's a flag save it and advance then return true
            //flag f{};
            //m_current_resault_command->m_flags[m_current_token.literal] = f;
            return single_delimiter_::flag;
          }
          // not a flag require advancment
          return single_delimiter_::invalid;
        }
        else // multi char it's either an alias or multiple flags
        {
          // alias check
          const auto& alias_it = m_current_command->m_options_aliases.find(m_current_token.literal);
          if(m_current_command->m_options_aliases.end() != alias_it)
          {
            // if it's an alias resolve it and forward resolving state
            return single_delimiter_::alias;
          }
          
          // so it must be multi-flag or (error)
          return is_multiflag() ? single_delimiter_::mutliflag : parser::single_delimiter_::invalid;
        }
     }

     double_delimiter_ get_double_dlimiter_type(const detail::token& tok)
     {
        // we check if tok is a valid dd identifier

       // we are at the identifier [[old]]
       
       
       if(tok.type != detail::token_type::identifire)
       //if(m_current_token.type != detail::token_type::identifire)
       {
          // return at the double_delimiter token require advancment
          return double_delimiter_::invalid;
       }

        // alias check
        const auto& alias_it = m_current_command->m_options_aliases.find(tok.literal);
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


    bool is_multiflag(bool continue_on_error = false) 
    {
    // returns true if it can parse the multi-flags and resolve each one of them
    // returns false otherwise and it requires the token to be advanced externally
    // TODO: maybe allow to enable resolving the rest of the flags
    // if one of the flags are not correct,
    // or add some more configurations to this function
    // because the behavior now is if a flag isn't correct
    // we return immedatly but there might be another flags in front of it
    // that won't be resolved or even checked.

    // we are now at the identifier next to the delimiter
    // that we suspect is multiflag

    bool ret = false;
    std::string copy = m_current_token.literal;
    if (copy.empty()) { //add a check for empty token.
        return false; //Or return false, depending on the desired behavior.
    }
    // TODO: add an option to validate all flags then run them because in some tools
    // running a command with some flags and not the others can be wrong
    // so better to error than to do something you are not supposed to 
    // rm -rf
    size_t pos = 0, end_pos = 0;
    bool all_flags_valid = true; // Use a flag to track overall validity

    while (end_pos < copy.size()) {
        bool advance_success = utf8::advance_one_char(m_current_token.literal, end_pos);
        if (!advance_success) {
            // Handle UTF-8 error (e.g., invalid character)
            // TODO: Add error handling for invalid UTF-8
            
          //if(error_flags){
          //      error_flags->push_back("Invalid UTF-8 character");
          //  }
            return false;
        }

        const auto& str = copy.substr(pos, end_pos);
        
        const auto& flag_it = m_current_command->m_flags.find(str);
        pos = end_pos;
        // check if the current flag is not valid
        if (m_current_command->m_flags.end() == flag_it) {
            // invalid flag
            all_flags_valid = false;
            //if (error_flags) {
            //    error_flags->push_back(str); // Report the invalid flag
            //}

            if (!continue_on_error) {
                // Stop processing on the first error
                return false;
            }
        } else {
            ret = true; // Add the valid flag
        }
    }

    return ret; // Return true even if not all flags are valid 'cause we already advanced 
                 // idk if we should only advance if all are valid? 
}


      // returns argument_list as a space separated list if it can parse argument list 'till the end(with the closing)
      // returns "" otherwise and it requires the token to be advanced externally
      std::string parse_arguments_list(utf8::code_point expect_end, utf8::code_point sep)
      {
        // TODO: skip last whitespace(if any) and last comma (if any) 
        // makes the user life way more easy if they want to auto generate the args list 
        // like c++ it's valid to do so:
        // auto x = { 1, 2, 3 , };
        // currently supporting only comma separated lists e.g. [ arg1, arg2, arg3 ]
        //TODO: add support for non comma separated lists e.g.  [ arg1 arg2 arg3 ] 
        //TODO: check me
        // we are now at the first argument 
        std::stringstream ss;

        // if the peek token is single Unicode char and it's equal to sep then it's valid 
        while(utf8::is_single_char(m_peek_token.literal) && m_peek_token.literal[0] == sep)
        {
          ss << m_current_token.literal;
          advance();
          // we are now at the sep
          advance();
          // we are now at the next argument
        }
        
        if(!expect_next(expect_end)) // the state now is before the expected end i.e. at the last argument
          return {};

        // the state now is at the expect_end token
        
        // skip it 
        advance();
        return ss.str();
      }

      bool parse_value_s(std::string& val, bool allow_multiple, bool& r) 
      {
        // we are now at the token after the identifire (might be assign or space) or both

        // if its a space or assign skip it
        if(m_current_token.type == detail::token_type::space)
          advance(); //skip space TODO: or = (chnages the parser)
        if(m_current_token.type == detail::token_type::assign) // require a value 
        {
          r = true; // syntax error
          advance();
        }

        // we are now at the value(s) token

        if(allow_multiple)
        {
          const auto& args =  parse_args();
          if(!args.empty())
            val = args;
          else 
          {
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
        //TODO: check me
        size_t pos = 0;
        if(utf8::decode(m_peek_token.literal, pos) == cp && pos >= m_peek_token.literal.size() - 1)
        {
          advance();
          return true;
        }
        return false;
      }

      // changes the m_current_resault_command->and the m_current_command to curr_cmd_res and curr_cmd_build
      void change_command(command& curr_cmd_res, command_builder& curr_cmd_build)
      {
        m_current_resault_command = &curr_cmd_res;
        m_current_command = &curr_cmd_build;
      }

      // adds opt with the name 'name' to m_current_resault_command->m_options[name]
      option& save_option(const std::string& name, const option& opt)
      {
        m_current_resault_command->m_options[name] = opt;
        return m_current_resault_command->m_options[name];
      }

      // adds cmd with the name 'name' to m_current_resault_command->m_commands[name]
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

      // deprecated
      //parse_resault build_base_resault()
      //{
        // just copy the skeleton of the builder(s)
      //  parse_resault pr;
        //pr.root = add_command(m_root);
      //  return pr;
      //}

      //command add_command(const command_builder& cmd_build)
      //{
      //  command cmd;

      //  for(const auto& subcmd_build : cmd_build.m_subcommands)
      //  {
      //    auto c = add_command(subcmd_build.second);
      //    cmd.m_commands[subcmd_build.first] = {};
      //  }

      //  for(const auto& opt_build : cmd_build.m_options)
      //  {
      //    cmd.m_options[opt_build.first] = option{};
      //  }

      //  for(const auto& flag_name : cmd_build.m_flags)
      //  {
      //    cmd.m_flags[flag_name] = flag{};
      //  }
      //  return cmd;
      //}
    private:
      detail::lexer m_lx;
      command_builder m_root; // root command (doesn't allow any value) update: why not?!
                              // this would be the interface that builds the whole application
      detail::token m_current_token,
                    m_peek_token;
      command_builder* m_current_command{ &m_root };

      parse_resault m_resault;
      command* m_current_resault_command{ &m_resault.root };
    };
  }
} //namespace clara::inline v_0_0_0

#endif //CLARA_HPP
