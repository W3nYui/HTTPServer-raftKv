#include "DatabaseConfig.h"

#include <cstdlib>
#include <stdexcept>

namespace
{
std::string requiredEnvironment(const char* name)
{
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0')
    {
        throw std::invalid_argument(std::string("Missing required environment variable: ") + name);
    }
    return value;
}
}

DatabaseConfig databaseConfigFromEnvironment()
{
    return {
        requiredEnvironment("GOMOKU_DB_HOST"),
        requiredEnvironment("GOMOKU_DB_USER"),
        requiredEnvironment("GOMOKU_DB_PASSWORD"),
        requiredEnvironment("GOMOKU_DB_NAME"),
    };
}
