#pragma once

#include <string>
#include <vector>

namespace common {

// Splits text into lowercase, alphanumeric-only word tokens.
// e.g. "Cerebral White Matter." -> ["cerebral", "white", "matter"]
std::vector<std::string> Tokenize(const std::string& text);

}  // namespace common
