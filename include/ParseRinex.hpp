// ParseRinex.hpp
#pragma once 
#include <unordered_map>
#include <string>
#include <utility>
#include <vector>

namespace rinex {

// Represents a single observation epoch, storing L1/L2 measurements for each satellite.
// The map key is the normalized satellite ID (e.g., "G01"), and the value is a pair
// of doubles: (L1 measurement, L2 measurement).
struct ObsEpoch {
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  double second = 0.0;
  int event_flag = 0;
  int num_sv = 0;
  std::unordered_map<std::string, std::pair<double, double>> sat_L1L2;
};

// organizes the RINEX observations, including RINEX version, the observations types,
// and the collection of ObsEpochs.  
struct RinexObs{
    bool is_v3=false;
    std::vector<std::string> obs_types; // as in header, e.g., L1C, L1P, L2W, etc.
    std::vector<ObsEpoch> epochs;
};

// Enum representing possible error codes returned by the RINEX parser.
// Allows callers to distinguish between different failure scenarios
// such as missing header fields, incompatible observation types, or file errors.
enum class ParseRinexError {
    Success,
    FileNotFound,
    MissingHeader,
    InvalidObsTypeCount,
    IncompatibleObsTypes,
    NoEpochs
};

ParseRinexError parse_rinex_obs(const std::string& path, rinex::RinexObs& out);

// The code currently parses only GPS for now
bool is_gps_sat(const std::string &sv);

// remove leading and trailing whitespace, tabs, and newlines from a string 
std::string trim(const std::string &s); 

// extract obs types from the header using space-based split, filtering for valid types.
// This function works for both RINEX versions 2 and 3. 
std::vector<std::string> extract_obs_types_from_line(
    const std::string& line,
    size_t skip_chars,
    int min_len,
    int max_len,
    const std::string& valid_start = "CLDSPT"
  );

// returns true if the string represents a valid floating point number
bool is_number(const std::string &s);

// edit RINEX2 satellite IDs to conform to RINEX3 standard
std::string normalize_sat_id(const std::string &sv);

// True if the RINEX file is version 3
bool is_rinex_v3(const std::string& line);

// The observation type line should list the number of expected observation types 
int parse_obs_type_count(const std::string& line);

} // end namespace rinex 