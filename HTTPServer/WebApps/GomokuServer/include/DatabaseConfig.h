#pragma once

#include <string>

struct DatabaseConfig
{
    std::string host;
    std::string user;
    std::string password;
    std::string database;
};

DatabaseConfig databaseConfigFromEnvironment();
