// dbe.h
#pragma once

#include "Table.h"
#include <imgui.h>
#include <libpq-fe.h>
#include <memory>
#include <string>
#include <vector>

class DBE
{
  public:
    DBE() = default;
    ~DBE();

    // Main interface
    void render();   // Just renders the DBE content
    void shutdown(); // Just database cleanup

  private:
    // Database connection state
    struct DatabaseState
    {
        char connStr[1024] = "";
        bool showPassword = false;
        PGconn *conn = nullptr;
        std::vector<std::string> tables;
        std::string selectedTable;
        std::unique_ptr<Table> tableView;
        std::string connectedHost;
        std::string connectedUser;
        std::string connectedPort;
        double connectionTimeMs = 0.0;

        bool isConnected() const { return conn != nullptr; }
    } dbState;

    // Database operations
    std::vector<std::string> fetchTables();
    void connect();
    void disconnect();

    // Rendering helpers
    void renderConnectionBar();
    void renderConnectionInfo();
    void renderContent();
    void renderLeftPanel();
    void renderMainPanel();
};