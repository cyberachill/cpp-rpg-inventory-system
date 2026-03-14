#pragma once

#include <optional>
#include <string>
#include <utility>
#include <stdexcept>
#include <type_traits>

/*======================================================================
 *  0) Simple Result type (value / error handling)
 *====================================================================*/
template <typename T>
class Result {
    struct ErrTag {};
    std::optional<T> value_;
    std::string      error_;

    // private value constructors
    Result(const T& v, int)         : value_(v) {}
    Result(T&& v,      int)         : value_(std::move(v)) {}
    // private error constructor (tag prevents ambiguity for T=std::string)
    Result(ErrTag, const std::string& e) : value_(std::nullopt), error_(e) {}

public:
    // factory helpers
    template <typename U = T>
    static Result ok(U&& v) {
        return Result(std::forward<U>(v), 0);
    }
    static Result err(const std::string& e) { return Result(ErrTag{}, e); }

    // conversion to bool – true = ok, false = error
    explicit operator bool() const noexcept { return value_.has_value(); }

    // accessors
    const T& value() const {
        if (!value_) throw std::runtime_error("Result has no value: " + error_);
        return *value_;
    }
    T& value() {
        if (!value_) throw std::runtime_error("Result has no value: " + error_);
        return *value_;
    }
    const std::string& error() const noexcept { return error_; }
};

/* specialization for void */
template <>
class Result<void> {
    bool        ok_;
    std::string error_;

public:
    Result(bool ok = true, const std::string& err = "") : ok_(ok), error_(err) {}

    static Result ok()  { return Result(true,  ""); }
    static Result err(const std::string& e) { return Result(false, e); }

    explicit operator bool() const noexcept { return ok_; }
    const std::string& error() const noexcept { return error_; }
};
