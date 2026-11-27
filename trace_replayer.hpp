#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

class Trace {
public:
  int timestamp, input_length, output_length;
  std::vector<int> hash_ids;
  Trace(int ts, int inLen, int outLen, nlohmann::basic_json<> Hids)
      : timestamp(ts), input_length(inLen), output_length(outLen) {
    for (auto id : Hids)
      hash_ids.push_back(id);
  }

  std::string to_string() {
    std::stringstream stream;
    stream << "timestamp: " << timestamp << " input_length: " << input_length
           << " output_length: " << output_length << " hash_ids: [ ";
    for (auto id : hash_ids)
      stream << id << " ";
    stream << "]";
    return stream.str();
  }
};

class TracePlayer {

  std::vector<Trace> JsonV;
  int i = -1;

public:
  TracePlayer(std::string path) {
    std::ifstream jsonl_file(path);

    if (!jsonl_file)
      throw new std::runtime_error("File Not Found");

    for (std::string line; std::getline(jsonl_file, line);) {
      auto parsed_line = nlohmann::json::parse(line);
      JsonV.push_back(
          Trace(parsed_line.at("timestamp"), parsed_line.at("input_length"),
                parsed_line.at("output_length"), parsed_line.at("hash_ids")));
    }
  }

  void reInit() { i = -1; }
  Trace *step() {
    if (i + 1 < int(JsonV.size())) {
      i++;
      return &JsonV[i];
    }
    return nullptr;
  }
};