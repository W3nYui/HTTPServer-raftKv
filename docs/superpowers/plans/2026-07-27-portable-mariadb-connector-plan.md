# Portable MariaDB and Connector/C++ Environment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `simple_server` build and run with a local MariaDB LTS service on CachyOS and Ubuntu WSL 22.04 while retaining the legacy Connector/C++ `cppconn` API.

**Architecture:** Build MySQL Connector/C++ 1.1 into a project-local, gitignored prefix. CMake resolves the legacy connector and C client library through actual discovery rather than fixed include paths. A runtime-only environment file holds the generated application credential and is loaded by the demo launcher.

**Tech Stack:** CMake, Bash, MariaDB LTS, MySQL Connector/C++ 1.1, C++17, CTest.

---

### Task 1: Make Connector/C++ dependency discovery portable

**Files:**
- Modify: `HTTPServer/CMakeLists.txt`

- [ ] Replace fixed `/usr/include/mysql-cppconn-8`, `/usr/include/mysql`, `mysqlcppconn`, and `mysqlclient` assumptions with `find_path`/`find_library` calls for `cppconn/connection.h`, `mysql_driver.h`, `mysqlcppconn`, and either `mariadb` or `mysqlclient`.
- [ ] Respect `MYSQL_CONNECTOR_CPP_ROOT` as a CMake cache path and search its `include` and `lib` directories first.
- [ ] Stop configuration with a message that names the missing connector headers, connector library, or C client library.
- [ ] Link `simple_server` with the discovered absolute library paths.
- [ ] Configure in an empty build directory before dependencies are installed and verify the expected Connector/C++ diagnostic is emitted.

### Task 2: Add local database bootstrap and launcher integration

**Files:**
- Create: `scripts/init-local-mariadb.sh`
- Modify: `scripts/start-demo.sh`
- Modify: `.gitignore`

- [ ] Create a Bash bootstrap script that requires a running local MariaDB service, writes `runtime/gomoku-db.env` with mode `600`, creates `Gomoku`, creates or updates `gomoku@localhost` and `gomoku@127.0.0.1`, applies `scripts/init-db.sql`, and grants only schema-level privileges.
- [ ] Generate the password with `openssl rand -base64 32` when no existing runtime environment file is present.
- [ ] Make `start-demo.sh` fail clearly when the runtime environment file is absent and export its values before launching `simple_server`.
- [ ] Keep `runtime/` ignored so the generated secret cannot be committed.

### Task 3: Document both supported host environments

**Files:**
- Modify: `README.md`

- [ ] Describe the legacy Connector/C++ 1.1 requirement separately from MariaDB server installation.
- [ ] Provide CachyOS package installation, local MariaDB initialization and service startup commands.
- [ ] Provide Ubuntu WSL 22.04 package installation and service startup commands, including the WSL systemd caveat.
- [ ] Document the project-local Connector/C++ install prefix, `MYSQL_CONNECTOR_CPP_ROOT`, database bootstrap, and build/start sequence.
- [ ] State that the generated `gomoku` account is local-only and that no `root/root` credential or remote 3306 exposure is configured.

### Task 4: Install and verify the CachyOS environment

**Files:**
- Verify: `HTTPServer/CMakeLists.txt`
- Verify: `scripts/init-local-mariadb.sh`

- [ ] Install `mariadb-lts` and build prerequisites with Pacman.
- [ ] Download the MySQL official Connector/C++ 1.1.13 archive, build it against the installed C client headers, and install it into `runtime/deps/mysql-connector-cpp`.
- [ ] Initialize and start MariaDB, then execute `scripts/init-local-mariadb.sh`.
- [ ] Configure `HTTPServer` with `-DMYSQL_CONNECTOR_CPP_ROOT=$PWD/runtime/deps/mysql-connector-cpp` and build target `simple_server`.
- [ ] Run `ctest --test-dir <build-dir> --output-on-failure` and report the actual result.
