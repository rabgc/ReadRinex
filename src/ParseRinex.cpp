// File:   ParseRinex.cpp
// Author: Rick.Bennett
// Date:   August 31, 2025
// Description:
// Read GNSS observables from a RINEX file and store obs in CSV format
//
 
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "../include/ParseRinex.hpp"

inline int parse_obs_type_count(const std::string& line) {
  std::istringstream iss(line);
  std::string token1, token2;
  iss >> token1 >> token2;
  // RINEX3: first token is a single uppercase letter (constellation)
  if (token1.size() == 1 && isupper(token1[0])) {
    if (is_number(token2)) return std::stoi(token2);
    else return -1;
  }
  // RINEX2: first token should be the count
  if (is_number(token1)) return std::stoi(token1);
  return -1;
}

bool parse_rinex_obs(const std::string &path, RinexObs &out) {

  std::ifstream f(path);
  if (!f) {
    std::cerr << "Cannot open " << path << "\n";
    return false;
  }
  std::string line;

  // Header parsing state
  bool version_found = false;
  bool obs_type_line_found = false;
  bool is_v3 = false;
  std::string version_line;
  std::string obs_type_line;
  std::vector<std::string> obs_types;
  int obs_type_count = 0;

  // Header parsing loop
  while (getline(f, line)) {
    if (line.find("RINEX VERSION / TYPE") != std::string::npos) {
      version_found = true;
      version_line = line;
      is_v3 = is_rinex_v3(line);
    }
    if (line.find("SYS / # / OBS TYPES") != std::string::npos) {
      obs_type_line_found = true;
      obs_type_line = line;
      char sys = line[0];
      if (sys != 'G') continue; // Only GPS for now
      obs_type_count = parse_obs_type_count(line);
      if (obs_type_count <= 0) {
        std::cerr << "Error: Invalid observation type count (" << obs_type_count << ") in header.\n";
        return false;
      }
      std::vector<std::string> fld = extract_obs_types_from_line(line, 7, 3, 4);
      for (const std::string& t_raw : fld) {
        std::string t = trim(t_raw);
        if (!t.empty()) obs_types.push_back(t);
        if ((int)obs_types.size() == obs_type_count) break;
      }
      while ((int)obs_types.size() < obs_type_count) {
        std::string l2;
        if (!getline(f, l2)) break;
        if (l2.find("SYS / # / OBS TYPES") == std::string::npos) break;
        auto fld2 = extract_obs_types_from_line(l2, 0, 3, 4);
        for (const std::string& t_raw : fld2) {
          std::string t = trim(t_raw);
          if (!t.empty()) obs_types.push_back(t);
          if ((int)obs_types.size() == obs_type_count) break;
        }
      }
    }
    if (line.find("# / TYPES OF OBSERV") != std::string::npos) {
      obs_type_line_found = true;
      obs_type_line = line;
      obs_type_count = parse_obs_type_count(line);
      if (obs_type_count <= 0) {
        std::cerr << "Error: Invalid observation type count (" << obs_type_count << ") in header.\n";
        return false;
      }
      std::vector<std::string> fld = extract_obs_types_from_line(line, 6, 2, 3);
      for (const std::string& t_raw : fld) {
        std::string t = trim(t_raw);
        if (!t.empty()) obs_types.push_back(t);
        if ((int)obs_types.size() == obs_type_count) break;
      }
      while ((int)obs_types.size() < obs_type_count) {
        std::string l2;
        if (!getline(f, l2)) break;
        std::vector<std::string> fld2 = extract_obs_types_from_line(l2, 0, 2, 3);
        for (const std::string& t_raw : fld2) {
          std::string t = trim(t_raw);
          if (!t.empty()) obs_types.push_back(t);
          if ((int)obs_types.size() == obs_type_count) break;
        }
      }
    }
    if (line.find("END OF HEADER") != std::string::npos) {
      break;
    }
    // ...other header parsing...
  }

  // Validation
  if ((is_v3 && obs_type_line.find("SYS / # / OBS TYPES") == std::string::npos) ||
      (!is_v3 && obs_type_line.find("# / TYPES OF OBSERV") == std::string::npos)) {
    std::cerr << "Error: Version and obs type line format disagree.\n";
    return false;
  }
  if (!version_found || !obs_type_line_found) {
    std::cerr << "Error: Missing version or obs type line in header.\n";
    return false;
  }
  // Stricter validation for obs type count and parsed types
  if (obs_type_count <= 0) {
    std::cerr << "Error: Obs type count is zero or negative.\n";
    return false;
  }
  if ((int)obs_types.size() == 0) {
    std::cerr << "Error: No obs types parsed from header.\n";
    return false;
  }
  if (obs_type_count != (int)obs_types.size()) {
    std::cerr << "Error: Obs type count mismatch.\n";
    return false;
  }

  // RINEX2 header with RINEX3-style obs types
  if (!is_v3 && obs_type_line.find("# / TYPES OF OBSERV") != std::string::npos) {
    for (const auto& t : obs_types) {
      if (t.size() >= 3 && (t.back() == 'C' || t.back() == 'W' || t.back() == 'P' || t.back() == 'S' || t.back() == 'X')) {
        std::cerr << "Error: RINEX2 header contains RINEX3-style obs type '" << t << "'.\n";
        return false;
      }
    }
  }
  // RINEX3 header with RINEX2-style obs types
  if (is_v3 && obs_type_line.find("SYS / # / OBS TYPES") != std::string::npos) {
    for (const auto& t : obs_types) {
      if (t == "C1" || t == "L1" || t == "S1" || t == "C2" || t == "L2" || t == "S2") {
        std::cerr << "Error: RINEX3 header contains RINEX2-style obs type '" << t << "'.\n";
        return false;
      }
    }
  }

  out.is_v3 = is_v3;
  out.obs_types = obs_types;

  // ...parse data section...

  return true;
}