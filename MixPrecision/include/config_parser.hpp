#ifndef CONFIGPARSER_GUARD
#define CONFIGPARSER_GUARD 1

#include "Change.hpp"
#include <map>
#include <memory>
#include <string>

std::map<std::string, std::unique_ptr<StrChange>> parse_config(const std::string &filename);

#endif
