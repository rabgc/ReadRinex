// ParseRinex.hpp
#pragma once 
#include <unordered_map>
#include <string>
#include <utility>
#include <vector>

// Represents a single observation epoch, storing L1/L2 measurements for each satellite.
// The map key is the normalized satellite ID (e.g., "G01"), and the value is a pair
// of doubles: (L1 measurement, L2 measurement).
struct ObsEpoch {
  std::unordered_map<std::string, std::pair<double, double>> sat_L1L2;
};

// Organizes the RINEX observations, including RINEX version, the observations types,
// and the collection of ObsEpochs.  
struct RinexObs{
    bool is_v3=false;
    std::vector<std::string> obs_types; // as in header, e.g., L1C, L1P, L2W, etc.
    std::vector<ObsEpoch> epochs;
};

// The code currently parses only GPS.  
// Accept RINEX 2: 'Gxx' often absent (just PRN), but we enforce 'G'
// If numeric like "01", treat as GPS for v2 minimal parsing
inline bool is_gps_sat(const std::string &sv){
    if(sv.empty()) return false;
    if(sv[0]=='G') return true;
    if(isdigit(sv[0])) return true;
    return false;
}

// remove leading and trailing whitespace, tabs, and newlines from a string. 
inline std::string trim(const std::string &s){
    size_t b = s.find_first_not_of(" \t\r\n");
    if(b==std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b,e-b+1);
}


// extract obs types from the header using space-based split, filtering for valid types.
// This function works for both RINEX versions 2 and 3. 
inline std::vector<std::string> extract_obs_types_from_line(
    const std::string& line,
    size_t skip_chars,
    int min_len,
    int max_len,
    const std::string& valid_start = "CLDSPT")
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

// returns true if the string represents a valid floating point number
inline bool is_number(const std::string &s){
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

// Edit RINEX2 SV IDs to conform to RINEX3 standard
inline std::string normalize_sat_id(const std::string &sv){
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

// True if the RINEX file is version 3
inline bool is_rinex_v3(const std::string& line) {
    if(line.size() >= 20 && line.find("RINEX VERSION / TYPE") != std::string::npos) {
        std::string v = trim(line.substr(0, 20));
        if(!v.empty() && (v[0] == '3' || v[0] == '4')) return true;
    }
    return false;
}

// Parse the RINEX observation file and return true if successful 
bool parse_rinex_obs(const std::string &path, RinexObs &out);

// The observation type line should list the number of expected observation types 
int parse_obs_type_count(const std::string& line);

// TODO 
// test and build the following 
//

void extract_obs_types_multiline(std::ifstream& f, int expected_count, std::vector<std::string>& obs_types, bool is_rinex3);

bool validate_obs_type_count(int obs_type_count, int actual_count);

void report_error(const std::string& message);

