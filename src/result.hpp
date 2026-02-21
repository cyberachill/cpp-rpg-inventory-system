#pragma once

#include <optional>
#include <string>
#include <utility>
#include <stdexcept>
#include <type_traits>

/*======================================================================
 *  0) Simple Result type (value / error handling)
 *====================================================================*/
template <typename T>
class Result {
    std::optional<T> value_;
    std::string      error_;

public:
    // success constructors
    Result(const T& v) : value_(v) {}
    Result(T&& v)      : value_(std::move(v)) {}

    // error constructor
    Result(const std::string& err) : value_(std::nullopt), error_(err) {}

    // factory helpers
    template <typename U = T>
    static Result ok(U&& v) { return Result(std::forward<U>(v)); }

    static Result err(const std::string& e) { return Result(e); }

    // conversion to bool – true = ok, false = error
    explicit operator bool() const noexcept { return value_.has_value(); }
    bool ok()   const noexcept { return static_cast<bool>(*this); }
    bool error()const noexcept { return !value_; }

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

    static Result ok()   { return Result(true, ""); }
    static Result err(const std::string& e) { return Result(false, e); }

    explicit operator bool() const noexcept { return ok_; }
    bool ok()   const noexcept { return ok_; }
    bool error()const noexcept { return !ok_; }

    const std::string& error() const noexcept { return error_; }
};
