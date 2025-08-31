// File:   ParseRinex.cpp
// Author: Rick.Bennett
// Date:   August 31, 2025
// Description:
// Read GNSS observables from a RINEX file and store obs in CSV format
//
 
#include <iostream>
#include <fstream>

#include "../include/ParseRinex.hpp"

int parse_obs_type_count(const std::string& line) {
    size_t i = 0;
    // Skip leading non-digit characters (e.g., constellation, spaces)
    while (i < line.size() && !isdigit(line[i])) ++i;
    size_t start = i;
    // Read consecutive digits
    while (i < line.size() && isdigit(line[i])) ++i;
    if (start < i) {
        try {
            return std::stoi(line.substr(start, i - start));
        } catch (...) {
            return -1; // Invalid integer
        }
    }
    return -1; // No integer found
}

bool parse_rinex_obs(const std::string &path, RinexObs &out) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "Cannot open " << path << "\n";
        return false;
    }
    std::string line;
    bool header_done=false;
    int obs_type_count;
    bool obs_type_not_found = true; 
    bool obs_type_count_mismatch = true;
    bool v3=false; 
    std::vector<std::string> obs_types;
    int n_obs_types=0;

    // try to determine which RINEX version
    while (getline(f, line)) {
        if (line.find("RINEX VERSION / TYPE") != std::string::npos) {
            v3 = is_rinex_v3(line);
        }

        // parse RINEX3 observation types from header
        if (line.find("SYS / # / OBS TYPES") != std::string::npos) {
            obs_type_not_found = false;
            v3 = true;
            char sys = line[0];
            if (sys != 'G') continue; // Only GPS for now
            int n = 0;
            try {
                n = stoi(line.substr(3,3));
            } catch(...) {
                n = 0;
            }
            obs_type_count = parse_obs_type_count(line);
            if (obs_type_count <= 0) {
              std::cerr << "Error: Invalid observation type count (" 
                        << obs_type_count << ") in header.\n";
              return false;
            }
            
            std::vector<std::string> fld = extract_obs_types_from_line(
              line, 7, 3, 4
            );
            for (const std::string& t_raw : fld) {
                std::string t = trim(t_raw);
                if (!t.empty()) obs_types.push_back(t);
                if ((int)obs_types.size() == n) break;
            }
            while ((int)obs_types.size() < n) {
                std::string l2;
                if (!getline(f, l2)) break;
                if (l2.find("SYS / # / OBS TYPES") == std::string::npos) break;
                auto fld2 = extract_obs_types_from_line(
                    l2, 0, 3, 4);
                for (const std::string& t_raw : fld2) {
                    std::string t = trim(t_raw);
                    if (!t.empty()) obs_types.push_back(t);
                    if ((int)obs_types.size() == n) break;
                }
            }
        }

        // parse RINEX2 observation types from header
        if (line.find("# / TYPES OF OBSERV") != std::string::npos) {
            obs_type_not_found = false;
            v3 = false;
            int n = 0;
            try {
                n = stoi(line.substr(0,6));
            } catch(...) {
                n = 0;
            }
            
            obs_type_count = parse_obs_type_count(line);
            if (obs_type_count <= 0) {
              std::cerr << "Error: Invalid observation type count (" 
                        << obs_type_count << ") in header.\n";
              return false;
            }

            std::vector<std::string> fld = extract_obs_types_from_line(line, 6, 2, 3);
            for (const std::string& t_raw : fld) {
                std::string t = trim(t_raw);
                if (!t.empty()) obs_types.push_back(t);
                if ((int)obs_types.size() == n) break;
            }
            while ((int)obs_types.size() < n) {
                std::string l2;
                if (!getline(f, l2)) break;
                std::vector<std::string> fld2 = extract_obs_types_from_line(l2, 0, 2, 3);
                for (const std::string& t_raw : fld2) {
                    std::string t = trim(t_raw);
                    if (!t.empty()) obs_types.push_back(t);
                    if ((int)obs_types.size() == n) break;
                }
            }
            n_obs_types = (int)obs_types.size();
        }
        if (line.find("END OF HEADER") != std::string::npos) {
            header_done = true;
            break;
        }
        // ...other header parsing...
    }
    out.is_v3 = v3;
    out.obs_types = obs_types;
    
    // parse the data 
    if (obs_type_count == obs_types.size()) {
      obs_type_count_mismatch = false; 
    }
    
    if (obs_type_not_found || obs_type_count_mismatch) {
      return false;
    } else {
      return true;
    } 
}