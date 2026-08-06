#include "tokenizer.h"

#include <cctype>

namespace common {

std::vector<std::string> Tokenize(const std::string& text) {
  std::vector<std::string> tokens;
  std::string current;

  for (unsigned char c : text) {
    if (std::isalnum(c)) {
      current += static_cast<char>(std::tolower(c));
    } else if (!current.empty()) {
      tokens.push_back(current);
      current.clear();
    }
  }
  if (!current.empty()) {
    tokens.push_back(current);
  }

  return tokens;
}

}  // namespace common
