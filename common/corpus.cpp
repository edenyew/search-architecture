#include "corpus.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace common {

std::vector<CorpusDoc> LoadCorpus(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("failed to open corpus file: " + path);
  }

  std::vector<CorpusDoc> docs;
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty()) continue;
    nlohmann::json j = nlohmann::json::parse(line);
    docs.push_back(CorpusDoc{
        .id = j.at("id").get<std::string>(),
        .text = j.at("text").get<std::string>(),
    });
  }
  return docs;
}

}  // namespace common
