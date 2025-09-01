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
  
  // open the RINEX file for reading; return false if file canot be opened
  std::ifstream f(path);
  if (!f) return false;
  

  // initialize state
  bool version_found = false, obs_type_line_found = false, eoh_found = false, is_v3 = false;
  
  std::string line;
  std::string version_line, obs_type_line;
  std::vector<std::string> obs_types;
  int obs_type_count = 0;

  // loop over the file 
  while (std::getline(f, line)) {
    line = rinex::trim(line);
    
    if (line.find("RINEX VERSION / TYPE") != std::string::npos) {
      version_found = true;
      version_line = line;
      is_v3 = rinex::is_rinex_v3(line);
    }

    // rinex v3
    if (line.find("SYS / # / OBS TYPES") != std::string::npos) {
      obs_type_line_found = true;
      obs_type_line = line;

      char sys = line[0];
      if (sys != 'G') continue; // only GPS for now

      obs_type_count = rinex::parse_obs_type_count(line);
      if (obs_type_count <= 0) return false;

      // stor obsercations types available in fld (field) vector
      std::vector<std::string> fld = rinex::extract_obs_types_from_line(line, 7, 3, 4);
      for (const std::string& t_raw : fld) {
        std::string t = rinex::trim(t_raw); // obs type 
        if (!t.empty()) obs_types.push_back(t);
        if ((int)obs_types.size() == obs_type_count) break; // exit for loop if match
      }
      // if the number of listed types is less than the number of types reported in the file
      //  try the next line
      while ((int)obs_types.size() < obs_type_count) {
        std::string l2; // the next line
        if (!std::getline(f, l2)) break;
        if (l2.find("SYS / # / OBS TYPES") == std::string::npos) break;
        auto fld2 = rinex::extract_obs_types_from_line(l2, 0, 3, 4);
        for (const std::string& t_raw : fld2) {
          std::string t = rinex::trim(t_raw);
          if (!t.empty()) obs_types.push_back(t);
          if ((int)obs_types.size() == obs_type_count) break;
        }
      }
    }

    // rinex v2
    if (line.find("# / TYPES OF OBSERV") != std::string::npos) {
      obs_type_line_found = true;
      obs_type_line = line;

      obs_type_count = rinex::parse_obs_type_count(line);
      if (obs_type_count <= 0) return false;

      std::vector<std::string> fld = rinex::extract_obs_types_from_line(line, 6, 2, 3);
      for (const std::string& t_raw : fld) {
        std::string t = rinex::trim(t_raw);
        if (!t.empty()) obs_types.push_back(t);
        if ((int)obs_types.size() == obs_type_count) break;
      }
      // same as above. Check next line for more observations.
      while ((int)obs_types.size() < obs_type_count) {
        std::string l2; // next line 
        if (!std::getline(f, l2)) break;
        std::vector<std::string> fld2 = rinex::extract_obs_types_from_line(l2, 0, 2, 3);
        for (const std::string& t_raw : fld2) {
          std::string t = rinex::trim(t_raw);
          if (!t.empty()) obs_types.push_back(t);
          if ((int)obs_types.size() == obs_type_count) break;
        }
      }
    }

    // exit loop over header 
    if (line.find("END OF HEADER") != std::string::npos) {
      eoh_found = true;
      break;
    }
  }

  // if there were any problems parsing the header return false 
  if (!eoh_found || 
      !version_found || 
      !obs_type_line_found || 
       obs_type_count <= 0 || 
       obs_types.size() != (size_t)obs_type_count
      ) return false;
  out.is_v3 = is_v3;
  out.obs_types = obs_types;

  // now parse epochs and observations
  ObsEpoch current_epoch;
  std::vector<std::string> sv_ids;
  
  // initialize the state 
  int svs_remaining = 0, obs_lines_remaining = 0;
  bool in_epoch = false;

  // loop over the remaning lines in the file
  while (std::getline(f, line)) {
    line = rinex::trim(line);
    if (line.empty()) continue;

    // rinex v3
    if (is_v3) {

      // if current line is an epoch header line 
      if (line[0] == '>') { 
        std::istringstream iss(line.substr(1));
        int year, month, day, hour, minute, event_flag, num_sv;
        double second;
        iss >> year >> month >> day >> hour >> minute >> second >> event_flag >> num_sv;
        if (iss.fail()) continue;

        // these current epoch data are only set if the epoch header was successfully parsed
        current_epoch = ObsEpoch{};
        current_epoch.year = year;
        current_epoch.month = month;
        current_epoch.day = day;
        current_epoch.hour = hour;
        current_epoch.minute = minute;
        current_epoch.second = second;
        current_epoch.event_flag = event_flag;
        current_epoch.num_sv = num_sv;
        svs_remaining = num_sv;
        in_epoch = true;
        continue;
      }
      if (in_epoch && svs_remaining > 0) { // if epoch header parsing fails svs_remaining=0
        std::istringstream sat_iss(line);

        std::string sv_id;
        sat_iss >> sv_id; // read the sv id, which is the first space delimited token
        sv_id = rinex::normalize_sat_id(sv_id); // impose rinex v3 naming convention 

        std::vector<double> obs_values;
        for (size_t j = 0; j < out.obs_types.size(); ++j) {
          double val = 0.0;
          sat_iss >> val;
          obs_values.push_back(val);
        }
        double l1 = obs_values.size() > 0 ? obs_values[0] : 0.0; // L1
        double l2 = obs_values.size() > 1 ? obs_values[1] : 0.0; // L2
        current_epoch.sat_L1L2[sv_id] = std::make_pair(l1, l2);

        svs_remaining--;
        if (svs_remaining == 0) {
          out.epochs.push_back(current_epoch);
          in_epoch = false;
        }
        continue;
      }
    } else {
      
      // rinex v2 
      int year, month, day, hour, minute, num_sv;
      double second;
      int event_flag = 0;

      std::istringstream iss(line);
      if (iss >> year >> month >> day >> hour >> minute >> second >> event_flag >> num_sv) {
        current_epoch = ObsEpoch{};
        current_epoch.year = year;
        current_epoch.month = month;
        current_epoch.day = day;
        current_epoch.hour = hour;
        current_epoch.minute = minute;
        current_epoch.second = second;
        current_epoch.num_sv = num_sv;
        current_epoch.event_flag = event_flag;
        sv_ids.clear();
        std::string token;
        while (iss >> token) sv_ids.push_back(token);
        while ((int)sv_ids.size() < num_sv) {
          if (!std::getline(f, line)) break;
          line = rinex::trim(line);
          std::istringstream sv_iss(line);
          while (sv_iss >> token) sv_ids.push_back(token);
        }
        obs_lines_remaining = num_sv;
        in_epoch = true;
        continue;
      }
      if (in_epoch && obs_lines_remaining > 0) { // if epoch header parsing fails svs_remaining=0
        std::istringstream sat_iss(line);
        std::vector<double> obs_values;
        for (size_t j = 0; j < out.obs_types.size(); ++j) {
          double val = 0.0;
          sat_iss >> val;
          obs_values.push_back(val);
        }
        double l1 = obs_values.size() > 0 ? obs_values[0] : 0.0; // L1
        double l2 = obs_values.size() > 1 ? obs_values[1] : 0.0; // L2

        std::string sv_id = rinex::normalize_sat_id(sv_ids[sv_ids.size() - obs_lines_remaining]);
        current_epoch.sat_L1L2[sv_id] = std::make_pair(l1, l2);
        
        obs_lines_remaining--;
        if (obs_lines_remaining == 0) {
          out.epochs.push_back(current_epoch);
          in_epoch = false;
        }
        continue;
      }
    }
  }
  if (out.epochs.empty()) return false;
  return true;
}
} // end namespace rinex