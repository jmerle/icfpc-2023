#pragma once

#include <cstdlib>
#include <string>

std::string getEnv(const std::string &name, const std::string &defaultValue) {
    const char *value = std::getenv(name.c_str());
    return value != nullptr ? value : defaultValue;
}
