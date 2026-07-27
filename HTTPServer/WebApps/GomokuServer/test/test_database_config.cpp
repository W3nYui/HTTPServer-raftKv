#include "DatabaseConfig.h"

#include <cassert>
#include <cstdlib>
#include <stdexcept>

int main()
{
    unsetenv("GOMOKU_DB_HOST");
    unsetenv("GOMOKU_DB_USER");
    unsetenv("GOMOKU_DB_PASSWORD");
    unsetenv("GOMOKU_DB_NAME");

    bool missingHostRejected = false;
    try
    {
        static_cast<void>(databaseConfigFromEnvironment());
    }
    catch (const std::invalid_argument&)
    {
        missingHostRejected = true;
    }
    assert(missingHostRejected);

    setenv("GOMOKU_DB_HOST", "", 1);
    setenv("GOMOKU_DB_USER", "gomoku", 1);
    setenv("GOMOKU_DB_PASSWORD", "test-password", 1);
    setenv("GOMOKU_DB_NAME", "Gomoku", 1);

    bool emptyHostRejected = false;
    try
    {
        static_cast<void>(databaseConfigFromEnvironment());
    }
    catch (const std::invalid_argument&)
    {
        emptyHostRejected = true;
    }
    assert(emptyHostRejected);

    setenv("GOMOKU_DB_HOST", "tcp://127.0.0.1:3306", 1);
    setenv("GOMOKU_DB_USER", "gomoku", 1);
    setenv("GOMOKU_DB_PASSWORD", "test-password", 1);
    setenv("GOMOKU_DB_NAME", "Gomoku", 1);

    const auto config = databaseConfigFromEnvironment();
    assert(config.host == "tcp://127.0.0.1:3306");
    assert(config.user == "gomoku");
    assert(config.password == "test-password");
    assert(config.database == "Gomoku");
}
