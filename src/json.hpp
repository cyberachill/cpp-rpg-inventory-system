#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include <stdexcept>
#include <unordered_map>
#include <iterator>

/*======================================================================
 *  0) Minimal JSON implementation (header‑only, self‑contained)
 *====================================================================*/

class json;                                     // forward declaration

using json_array  = std::vector<json>;
using json_object = std::unordered_map<std::string, json>;

class json {
public:
    /* ----- variant that holds every JSON value type ----- */
    using value_type = std::variant<
        std::nullptr_t,
        bool,
        int64_t,
        double,
        std::string,
        json_array,
        json_object>;

    json() : v_(nullptr) {}
    json(std::nullptr_t) : v_(nullptr) {}
    json(bool b) : v_(b) {}
    json(int i) : v_(static_cast<int64_t>(i)) {}
    json(int64_t i) : v_(i) {}
    json(double d) : v_(d) {}
    json(const char* s) : v_(std::string(s)) {}
    json(const std::string& s) : v_(s) {}
    json(std::string&& s) : v_(std::move(s)) {}
    json(const json_array& a) : v_(a) {}
    json(json_array&& a) : v_(std::move(a)) {}
    json(const json_object& o) : v_(o) {}
    json(json_object&& o) : v_(std::move(o)) {}

    /* ----- object construction via {"key",value} list ----- */
    struct kv_pair {
        std::string key;
        json        value;
        template <typename V>
        kv_pair(const char* k, V&& v) : key(k), value(std::forward<V>(v)) {}
    };
    json(std::initializer_list<kv_pair> init) : v_(json_object{}) {
        json_object& obj = std::get<json_object>(v_);
        for (auto&& kv : init) obj.emplace(std::move(kv.key), std::move(kv.value));
    }

    /* ----- type queries --------------------------------------------------- */
    bool is_null()               const { return std::holds_alternative<std::nullptr_t>(v_); }
    bool is_bool()               const { return std::holds_alternative<bool>(v_); }
    bool is_number_integer()     const { return std::holds_alternative<int64_t>(v_); }
    bool is_number_float()       const { return std::holds_alternative<double>(v_); }
    bool is_number()             const { return is_number_integer() || is_number_float(); }
    bool is_string()             const { return std::holds_alternative<std::string>(v_); }
    bool is_array()              const { return std::holds_alternative<json_array>(v_); }
    bool is_object()             const { return std::holds_alternative<json_object>(v_); }

    size_t size() const {
        if (is_array())  return std::get<json_array>(v_).size();
        if (is_object()) return std::get<json_object>(v_).size();
        return 0;
    }

    bool contains(const std::string& key) const {
        if (!is_object()) return false;
        const json_object& obj = std::get<json_object>(v_);
        return obj.find(key) != obj.end();
    }

    /* ----- element access ------------------------------------------------- */
    json& operator[](const std::string& key) {
        if (!is_object()) v_ = json_object{};
        json_object& obj = std::get<json_object>(v_);
        return obj[key];                 // creates null entry if missing
    }
    json& operator[](const char* key) { return (*this)[std::string(key)]; }

    const json& operator[](const std::string& key) const {
        if (!is_object()) throw std::out_of_range("json is not an object");
        const json_object& obj = std::get<json_object>(v_);
        auto it = obj.find(key);
        if (it == obj.end()) throw std::out_of_range("key not found: " + key);
        return it->second;
    }

    const json& at(const std::string& key) const { return (*this)[key]; }
    json&       at(const std::string& key)       { return (*this)[key]; }

    /* ----- value(key,default) -------------------------------------------- */
    template <typename T>
    T value(const std::string& key, const T& def) const {
        if (is_object()) {
            const json_object& obj = std::get<json_object>(v_);
            auto it = obj.find(key);
            if (it != obj.end()) return it->second.get<T>();
        }
        return def;
    }

    /* ----- get<T>() – primitive & custom types --------------------------- */
    template <typename T>
    T get() const {
        if constexpr (std::is_same_v<T, int>) {
            if (std::holds_alternative<int64_t>(v_))
                return static_cast<int>(std::get<int64_t>(v_));
            if (std::holds_alternative<double>(v_))
                return static_cast<int>(std::get<double>(v_));
            throw std::runtime_error("type mismatch (int)");
        } else if constexpr (std::is_same_v<T, int64_t>) {
            if (std::holds_alternative<int64_t>(v_))
                return std::get<int64_t>(v_);
            if (std::holds_alternative<double>(v_))
                return static_cast<int64_t>(std::get<double>(v_));
            throw std::runtime_error("type mismatch (int64)");
        } else if constexpr (std::is_same_v<T, double>) {
            if (std::holds_alternative<double>(v_))
                return std::get<double>(v_);
            if (std::holds_alternative<int64_t>(v_))
                return static_cast<double>(std::get<int64_t>(v_));
            throw std::runtime_error("type mismatch (double)");
        } else if constexpr (std::is_same_v<T, bool>) {
            if (std::holds_alternative<bool>(v_))
                return std::get<bool>(v_);
            throw std::runtime_error("type mismatch (bool)");
        } else if constexpr (std::is_same_v<T, std::string>) {
            if (std::holds_alternative<std::string>(v_))
                return std::get<std::string>(v_);
            throw std::runtime_error("type mismatch (string)");
        } else {
            T result{};
            from_json(*this, result);        // user‑defined from_json
            return result;
        }
    }

    // convenience for the parser
    std::string as_string() const { return get<std::string>(); }

    /* ----- object iterator (key()/value()) ------------------------------- */
    class object_iterator {
        using internal = json_object::iterator;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = std::pair<const std::string, json>;
        using difference_type   = std::ptrdiff_t;
        using pointer           = value_type*;
        using reference         = value_type&;

        object_iterator() = default;
        explicit object_iterator(internal it) : it_(it) {}

        object_iterator& operator++() { ++it_; return *this; }
        object_iterator  operator++(int) { object_iterator tmp = *this; ++it_; return tmp; }

        bool operator==(const object_iterator& o) const { return it_ == o.it_; }
        bool operator!=(const object_iterator& o) const { return it_ != o.it_; }

        const std::string& key()   const { return it_->first; }
        const json&        value() const { return it_->second; }
        json&              value()       { return it_->second; }

    private:
        internal it_;
    };

    object_iterator object_begin()       { return object_iterator(std::get<json_object>(v_).begin()); }
    object_iterator object_end()         { return object_iterator(std::get<json_object>(v_).end());   }
    object_iterator object_begin() const { return object_iterator(const_cast<json_object&>(std::get<json_object>(v_)).begin()); }
    object_iterator object_end()   const { return object_iterator(const_cast<json_object&>(std::get<json_object>(v_)).end());   }

    /* ----- array iterator (range‑for) ------------------------------------ */
    using array_iterator       = json_array::iterator;
    using const_array_iterator = json_array::const_iterator;

    array_iterator begin() {
        if (!is_array()) throw std::runtime_error("json is not an array");
        return std::get<json_array>(v_).begin();
    }
    array_iterator end() {
        if (!is_array()) throw std::runtime_error("json is not an array");
        return std::get<json_array>(v_).end();
    }
    const_array_iterator begin() const {
        if (!is_array()) throw std::runtime_error("json is not an array");
        return std::get<json_array>(v_).cbegin();
    }
    const_array_iterator end() const {
        if (!is_array()) throw std::runtime_error("json is not an array");
        return std::get<json_array>(v_).cend();
    }

    /* ----- assignment from arbitrary types -------------------------------- */
    json& operator=(const json& other) = default;
    json& operator=(json&& other)      = default;

    // primitive / custom types (uses to_json overloads)
    template <typename T,
              typename = std::enable_if_t<
                  !std::is_same_v<std::decay_t<T>, json> &&
                  !std::is_same_v<std::decay_t<T>, json_array> &&
                  !std::is_same_v<std::decay_t<T>, json_object>>>
    json& operator=(T&& v) {
        json tmp;
        to_json(tmp, std::forward<T>(v));
        *this = std::move(tmp);
        return *this;
    }

    // iterable containers (e.g. std::vector<Item>) → JSON array
    template <typename C,
              typename = std::void_t<decltype(std::begin(std::declval<C>())),
                                      decltype(std::end(std::declval<C>()))>,
              typename = std::enable_if_t<!std::is_same_v<std::decay_t<C>, json>>>
    json& operator=(const C& container) {
        json_array arr;
        for (const auto& elem : container) {
            json jelem;
            to_json(jelem, elem);
            arr.emplace_back(std::move(jelem));
        }
        *this = json(std::move(arr));
        return *this;
    }

    /* ----- dump (pretty / compact) -------------------------------------- */
    std::string dump(int indent = -1) const {
        std::ostringstream ss;
        dump_impl(ss, 0, indent);
        return ss.str();
    }

    /* ----- static parse --------------------------------------------------- */
    static json parse(const std::string& s) {
        struct parser {
            const std::string& str;
            size_t pos = 0;

            parser(const std::string& s) : str(s) {}

            void skip_ws() {
                while (pos < str.size() && std::isspace(static_cast<unsigned char>(str[pos]))) ++pos;
            }
            char peek() const { return pos < str.size() ? str[pos] : '\0'; }
            char get()       { return pos < str.size() ? str[pos++] : '\0'; }
            bool eof() const { return pos >= str.size(); }

            json parse_value() {
                skip_ws();
                char c = peek();
                if (c == '{') return parse_object();
                if (c == '[') return parse_array();
                if (c == '"') return parse_string();
                if (c == 't' || c == 'f') return parse_bool();
                if (c == 'n') return parse_null();
                if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number();
                throw std::runtime_error(std::string("unexpected character '") + c + "'");
            }

            json parse_object() {
                expect('{');
                json_object obj;
                skip_ws();
                if (peek() == '}') { get(); return json(std::move(obj)); }
                while (true) {
                    skip_ws();
                    std::string key = parse_string().as_string();
                    skip_ws();
                    expect(':');
                    json val = parse_value();
                    obj.emplace(std::move(key), std::move(val));
                    skip_ws();
                    char c = get();
                    if (c == '}') break;
                    if (c != ',')
                        throw std::runtime_error("expected ',' or '}' in object");
                }
                return json(std::move(obj));
            }

            json parse_array() {
                expect('[');
                json_array arr;
                skip_ws();
                if (peek() == ']') { get(); return json(std::move(arr)); }
                while (true) {
                    json val = parse_value();
                    arr.emplace_back(std::move(val));
                    skip_ws();
                    char c = get();
                    if (c == ']') break;
                    if (c != ',')
                        throw std::runtime_error("expected ',' or ']' in array");
                }
                return json(std::move(arr));
            }

            json parse_string() {
                expect('"');
                std::string out;
                while (true) {
                    if (eof()) throw std::runtime_error("unterminated string");
                    char c = get();
                    if (c == '"') break;
                    if (c == '\\') {
                        if (eof()) throw std::runtime_error("unterminated escape");
                        char esc = get();
                        switch (esc) {
                            case '"': out.push_back('"');  break;
                            case '\\': out.push_back('\\');break;
                            case '/': out.push_back('/');  break;
                            case 'b': out.push_back('\b'); break;
                            case 'f': out.push_back('\f'); break;
                            case 'n': out.push_back('\n'); break;
                            case 'r': out.push_back('\r'); break;
                            case 't': out.push_back('\t'); break;
                            case 'u': { // simple placeholder for \uXXXX
                                for (int i = 0; i < 4; ++i) {
                                    if (eof() || !std::isxdigit(static_cast<unsigned char>(peek())))
                                        throw std::runtime_error("invalid \\u escape");
                                    get(); // ignore hex digit
                                }
                                out.push_back('?');
                                break;
                            }
                            default:
                                throw std::runtime_error(std::string("invalid escape \\") + esc);
                        }
                    } else {
                        out.push_back(c);
                    }
                }
                return json(std::move(out));
            }

            json parse_number() {
                size_t start = pos;
                if (peek() == '-') get();
                while (std::isdigit(static_cast<unsigned char>(peek()))) get();
                if (peek() == '.') {
                    get();
                    while (std::isdigit(static_cast<unsigned char>(peek()))) get();
                    double d = std::stod(str.substr(start, pos - start));
                    return json(d);
                } else {
                    long long i = std::stoll(str.substr(start, pos - start));
                    return json(static_cast<int64_t>(i));
                }
            }

            json parse_bool() {
                if (str.compare(pos, 4, "true") == 0) { pos += 4; return json(true); }
                if (str.compare(pos, 5, "false") == 0) { pos += 5; return json(false); }
                throw std::runtime_error("invalid boolean literal");
            }

            json parse_null() {
                if (str.compare(pos, 4, "null") == 0) { pos += 4; return json(nullptr); }
                throw std::runtime_error("invalid null literal");
            }

            void expect(char ch) {
                skip_ws();
                if (get() != ch) throw std::runtime_error(std::string("expected '") + ch + "'");
            }
        };

        parser p(s);
        json result = p.parse_value();
        p.skip_ws();
        if (!p.eof())
            throw std::runtime_error("extra characters after JSON document");
        return result;
    }

private:
    value_type v_;

    /* ----- helpers for dump ------------------------------------------------ */
    static std::string escape_str(const std::string& s) {
        std::string out;
        for (char c : s) {
            switch (c) {
                case '\"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b";  break;
                case '\f': out += "\\f";  break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[7];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                        out += buf;
                    } else out += c;
            }
        }
        return out;
    }

    void dump_impl(std::ostringstream& ss, int curIndent, int indent) const {
        if (is_null()) { ss << "null"; return; }
        if (is_bool()) { ss << (std::get<bool>(v_) ? "true" : "false"); return; }
        if (is_number_integer()) { ss << std::get<int64_t>(v_); return; }
        if (is_number_float())   { ss << std::get<double>(v_);   return; }
        if (is_string()) {
            ss << '"' << escape_str(std::get<std::string>(v_)) << '"';
            return;
        }
        if (is_array()) {
            const json_array& arr = std::get<json_array>(v_);
            ss << '[';
            if (!arr.empty()) {
                if (indent >= 0) ss << '\n';
                for (size_t i = 0; i < arr.size(); ++i) {
                    if (indent >= 0) ss << std::string(curIndent + indent, ' ');
                    arr[i].dump_impl(ss, curIndent + indent, indent);
                    if (i + 1 < arr.size()) ss << ',';
                    if (indent >= 0) ss << '\n';
                }
                if (indent >= 0) ss << std::string(curIndent, ' ');
            }
            ss << ']';
            return;
        }
        if (is_object()) {
            const json_object& obj = std::get<json_object>(v_);
            ss << '{';
            if (!obj.empty()) {
                if (indent >= 0) ss << '\n';
                size_t i = 0;
                for (const auto& kv : obj) {
                    if (indent >= 0) ss << std::string(curIndent + indent, ' ');
                    ss << '"' << escape_str(kv.first) << "\": ";
                    kv.second.dump_impl(ss, curIndent + indent, indent);
                    if (++i < obj.size()) ss << ',';
                    if (indent >= 0) ss << '\n';
                }
                if (indent >= 0) ss << std::string(curIndent, ' ');
            }
            ss << '}';
            return;
        }
    }
};

/* ----- primitive‑to‑json overloads (used by generic operator=) ----- */
inline void to_json(json& j, const int& v)    { j = json(v); }
inline void to_json(json& j, const int64_t& v){ j = json(v); }
inline void to_json(json& j, const double& v){ j = json(v); }
inline void to_json(json& j, const bool& v)  { j = json(v); }
inline void to_json(json& j, const std::string& v){ j = json(v); }
inline void to_json(json& j, const char* v) { j = json(v); }
