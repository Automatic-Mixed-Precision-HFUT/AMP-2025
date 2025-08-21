#include <functional>
#include <nlohmann/json.hpp>
#include <fstream>
#include <map>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include "../include/Change.hpp"
#include "llvm/Support/raw_ostream.h"
using json = nlohmann::json;
using namespace std;


using json = nlohmann::json;

std::map<std::string, std::unique_ptr<StrChange>> parse_config(const std::string &filename) {
    std::map<std::string, std::unique_ptr<StrChange>> changes;

    std::ifstream file(filename);
    if (!file.is_open()) {
        errs() << "[ParseConfig] Failed to open file: " << filename << "\n";
        return changes;
    }

    json root;
    try {
        file >> root;
    } catch (const std::exception &e) {
        errs() << "[ParseConfig] JSON parse error: " << e.what() << "\n";
        return changes;
    }
    
    // errs()<<root.dump();
    // errs()<<"\n\n\n\n";

    function<std::string(const nlohmann::json&)> parse_array_type = [&](const json &typeJson) -> std::string {
        if (typeJson.is_array()) {
            std::string result;
            for (const auto &t : typeJson) {
                if (!result.empty()) result += ", ";
                if (t.is_array()) {
                    result += parse_array_type(t);  // recursive flatten
                } else if (t.is_string()) {
                    result += t.get<std::string>();
                }
            }
            return result;
        } else if (typeJson.is_string()) {
            return typeJson.get<std::string>();
        }
        return "";
    };

    auto handle_section = [&](const std::string &section, const std::string &classification, bool isCall = false) {
        if (!root.contains(section) || !root[section].is_array()) return;
        for (const auto &entry : root[section]) {
            std::string id;
            int field = -1;
            std::string typeStr = parse_array_type(entry.value("type", ""));
            std::string swit = entry.value("switch", "");

            if (classification == "localVar") {
                std::string name = entry.value("name", "");
                std::string function = entry.value("function", "");
                id = name + "@" + function;
                field = entry.value("field", -1);
            } else {
                id = entry.value("id", entry.value("name", ""));
                field = entry.value("field", -1);  // some may have field (structs)
            }

            if (id.empty() || typeStr.empty()) continue;

            if (isCall) {
                changes[id] = std::make_unique<FuncStrChange>(classification, typeStr, field, swit);
            } else {
                changes[id] = std::make_unique<StrChange>(classification, typeStr, field);
            }
        }
    };

    handle_section("globalVar", "globalVar");
    handle_section("localVar", "localVar");
    handle_section("op",       "op");
    handle_section("call",     "call", true);

    return changes;
}
