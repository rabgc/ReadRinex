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

namespace rinex {

bool is_number(const std::string &s){
    bool dot=false, sign=false, digit=false;
    for(char c: s){
        if(c==' '||c=='\t') continue;
        if(c=='+'||c=='-'){ if(sign) return false; sign=true; continue; }
        if(c=='.'){ if(dot) return false; dot=true; continue; }
        if(!isdigit(c) && c!='E' && c!='e') return false;
        digit=true;
    }
    return digit;
}

std::string trim(const std::string &s){
    size_t b = s.find_first_not_of(" \t\r\n");
    if(b==std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b,e-b+1);
}

bool is_rinex_v3(const std::string& line) {
    if(line.size() >= 20 && line.find("RINEX VERSION / TYPE") != std::string::npos) {
        std::string v = rinex::trim(line.substr(0, 20));
        if(!v.empty() && (v[0] == '3' || v[0] == '4')) return true;
    }
    return false;
}

std::vector<std::string> extract_obs_types_from_line(
    const std::string& line,
    size_t skip_chars,
    int min_len,
    int max_len, 
    const std::string& valid_start
  )
{
    std::string obs_str = line.substr(skip_chars);
    std::vector<std::string> words;
    size_t pos = 0;
    while (pos < obs_str.size()) {
        while (pos < obs_str.size() && isspace(obs_str[pos])) ++pos;
        size_t start = pos;
        while (pos < obs_str.size() && !isspace(obs_str[pos])) ++pos;
        if (start < pos) words.push_back(obs_str.substr(start, pos - start));
    }
    std::vector<std::string> fld;
    for (const auto& w : words) {
        if (w.size() >= min_len && w.size() <= max_len &&
            valid_start.find(w[0]) != std::string::npos) {
            fld.push_back(w);
        }
    }
    return fld;
}

bool is_gps_sat(const std::string &sv){
    if(sv.empty()) return false;
    if(sv[0]=='G') return true;
    if(isdigit(sv[0])) return true;
    return false;
}

std::string normalize_sat_id(const std::string &sv){
    std::string t=trim(sv);
    if(t.empty()) return t;
    if(t[0]=='G') return t; // already RINEX-3 style

    // RINEX-2 numeric PRN -> prefix G
    if(isdigit(t[0])){
      try {
        int prn = std::stoi(t);
        char buf[8]; snprintf(buf,sizeof(buf),"G%02d", prn);
        return std::string(buf);
      } catch (...) {
        // could not convert string to int
        return t; // fallback: return as-is
      }
    }
    return t;
}

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

bool parse_rinex_obs(const std::string &path, rinex::RinexObs &out) {

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
      is_v3 = rinex::is_rinex_v3(line);
    }
    if (line.find("SYS / # / OBS TYPES") != std::string::npos) {
      obs_type_line_found = true;
      obs_type_line = line;
      char sys = line[0];
      if (sys != 'G') continue; // Only GPS for now
      obs_type_count = rinex::parse_obs_type_count(line);
      if (obs_type_count <= 0) {
        std::cerr << "Error: Invalid observation type count (" << obs_type_count << ") in header.\n";
        return false;
      }
      std::vector<std::string> fld = rinex::extract_obs_types_from_line(line, 7, 3, 4);
      for (const std::string& t_raw : fld) {
        std::string t = rinex::trim(t_raw);
        if (!t.empty()) obs_types.push_back(t);
        if ((int)obs_types.size() == obs_type_count) break;
      }
      while ((int)obs_types.size() < obs_type_count) {
        std::string l2;
        if (!getline(f, l2)) break;
        if (l2.find("SYS / # / OBS TYPES") == std::string::npos) break;
        auto fld2 = rinex::extract_obs_types_from_line(l2, 0, 3, 4);
        for (const std::string& t_raw : fld2) {
          std::string t = rinex::trim(t_raw);
          if (!t.empty()) obs_types.push_back(t);
          if ((int)obs_types.size() == obs_type_count) break;
        }
      }
    }
    if (line.find("# / TYPES OF OBSERV") != std::string::npos) {
      obs_type_line_found = true;
      obs_type_line = line;
      obs_type_count = rinex::parse_obs_type_count(line);
      if (obs_type_count <= 0) {
        std::cerr << "Error: Invalid observation type count (" << obs_type_count << ") in header.\n";
        return false;
      }
      std::vector<std::string> fld = rinex::extract_obs_types_from_line(line, 6, 2, 3);
      for (const std::string& t_raw : fld) {
        std::string t = rinex::trim(t_raw);
        if (!t.empty()) obs_types.push_back(t);
        if ((int)obs_types.size() == obs_type_count) break;
      }
      while ((int)obs_types.size() < obs_type_count) {
        std::string l2;
        if (!getline(f, l2)) break;
        std::vector<std::string> fld2 = rinex::extract_obs_types_from_line(l2, 0, 2, 3);
        for (const std::string& t_raw : fld2) {
          std::string t = rinex::trim(t_raw);
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

  // validation
  if ((is_v3 && obs_type_line.find("SYS / # / OBS TYPES") == std::string::npos) ||
      (!is_v3 && obs_type_line.find("# / TYPES OF OBSERV") == std::string::npos)) {
    std::cerr << "Error: Version and obs type line format disagree.\n";
    return false;
  }
  if (!version_found || !obs_type_line_found) {
    std::cerr << "Error: Missing version or obs type line in header.\n";
    return false;
  }
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
  if (!is_v3 && obs_type_line.find("# / TYPES OF OBSERV") != std::string::npos) {
    for (const auto& t : obs_types) {
      if (t.size() >= 3 && (t.back() == 'C' || t.back() == 'W' || t.back() == 'P' || t.back() == 'S' || t.back() == 'X')) {
        std::cerr << "Error: RINEX2 header contains RINEX3-style obs type '" << t << "'.\n";
        return false;
      }
    }
  }
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

} // end namespace rinex 