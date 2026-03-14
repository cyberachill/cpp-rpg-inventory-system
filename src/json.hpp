#pragma once

/*======================================================================
 *  0) JSON — backed by nlohmann/json (header‑only, industry standard)
 *====================================================================*/

#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Parse JSON that may contain C‑style block comments (used in data files)
inline json json_parse(const std::string& s) {
    return json::parse(s, nullptr, /*allow_exceptions=*/true, /*ignore_comments=*/true);
}
