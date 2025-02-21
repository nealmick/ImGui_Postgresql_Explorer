#define GL_SILENCE_DEPRECATION
#include "DBE.h"
#include <iostream>
#include <chrono>

DBE::~DBE() {
    shutdown();
}

void DBE::render() {
    renderConnectionBar();
    if (dbState.isConnected()) {
        renderConnectionInfo();
    }
    renderContent();
}

void DBE::renderConnectionBar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 8));
    
    float topPadding = 8.0f;
    float sidePadding = 10.0f;
    float windowWidth = ImGui::GetWindowWidth();
    float contentWidth = windowWidth - (2 * sidePadding);
    
    ImGui::SetCursorPos(ImVec2(5.0f, topPadding));
    ImGui::BeginChild("TopBar", ImVec2(contentWidth, 33), false);
    
    float totalWidth = ImGui::GetWindowWidth();
    float buttonWidth = 120;
    float inputWidth = totalWidth - (buttonWidth * 2 + 20);
    
    ImGuiInputTextFlags flags = dbState.showPassword ? 0 : ImGuiInputTextFlags_Password;
    
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::SetNextItemWidth(inputWidth);
    ImGui::InputTextWithHint("##ConnStr", "Type connection string here...", 
                            dbState.connStr, sizeof(dbState.connStr), flags);
    
    ImGui::SameLine();
    if (ImGui::Button(dbState.showPassword ? "Hide" : "Show", 
                     ImVec2(buttonWidth, ImGui::GetFrameHeight()))) {
        dbState.showPassword = !dbState.showPassword;
    }
    
    ImGui::SameLine();
    if (dbState.isConnected()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        if (ImGui::Button("Disconnect", ImVec2(buttonWidth, ImGui::GetFrameHeight()))) {
            disconnect();
        }
        ImGui::PopStyleColor();
    } else {
        if (ImGui::Button("Connect", ImVec2(buttonWidth, ImGui::GetFrameHeight()))) {
            connect();
        }
    }
    
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void DBE::renderConnectionInfo() {
    float sidePadding = 10.0f;
    float contentWidth = ImGui::GetWindowWidth() - (2 * sidePadding);
    
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7, 3));
    ImGui::BeginChild("ConnectionInfo", ImVec2(contentWidth, 25), false);
    
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    
    std::string connInfo = "User: " + dbState.connectedUser + 
                          "  |  Port: " + dbState.connectedPort + 
                          "  |  Conn Time: " + std::to_string(static_cast<int>(dbState.connectionTimeMs)) + "ms" + 
                          "  |  Host: " + dbState.connectedHost;
    
    ImGui::SetCursorPos(ImVec2(10, 4));
    ImGui::Text("%s", connInfo.c_str());
    
    ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void DBE::renderContent() {
    ImGui::BeginChild("Content", ImVec2(0, 0), false);
    
    if (dbState.isConnected()) {
        renderLeftPanel();
        ImGui::SameLine();
        renderMainPanel();
    }
    
    ImGui::EndChild();
}

void DBE::renderLeftPanel() {
    ImGui::BeginChild("LeftPanel", ImVec2(200, 0), true);
    ImGui::Text("Tables");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 2.0f));
    
    for (const auto& table : dbState.tables) {
        if (ImGui::Selectable(table.c_str(), dbState.selectedTable == table)) {
            dbState.selectedTable = table;
        }
    }
    
    ImGui::EndChild();
}

void DBE::renderMainPanel() {
    ImGui::BeginChild("MainPanel", ImVec2(0, 0), false);
    
    if (!dbState.selectedTable.empty() && dbState.tableView) {
        ImGui::Dummy(ImVec2(0, 4.0f));
        
        std::string title = dbState.selectedTable;
        if (!title.empty()) {
            title[0] = std::toupper(title[0]);
        }
        title += " Table";
        
        ImGui::Text("%s", title.c_str());
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 2.0f));
        
        static std::string lastTable;
        if (lastTable != dbState.selectedTable) {
            dbState.tableView->loadTableData(dbState.selectedTable, 0);
            lastTable = dbState.selectedTable;
        }
        
        dbState.tableView->render();
    }
    
    ImGui::EndChild();
}

std::vector<std::string> DBE::fetchTables() {
    std::vector<std::string> tables;
    if (!dbState.conn) return tables;
    
    const char* query = "SELECT table_name FROM information_schema.tables WHERE table_schema = 'public'";
    PGresult* res = PQexec(dbState.conn, query);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return tables;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        tables.push_back(PQgetvalue(res, i, 0));
    }
    
    PQclear(res);
    return tables;
}

void DBE::connect() {
    if (dbState.conn) return;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    dbState.conn = PQconnectdb(dbState.connStr);
    if (PQstatus(dbState.conn) == CONNECTION_OK) {
        auto endTime = std::chrono::high_resolution_clock::now();
        dbState.connectionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        
        dbState.connectedHost = PQhost(dbState.conn) ? PQhost(dbState.conn) : "localhost";
        dbState.connectedUser = PQuser(dbState.conn) ? PQuser(dbState.conn) : "unknown";
        dbState.connectedPort = PQport(dbState.conn) ? PQport(dbState.conn) : "5432";
        
        dbState.tables = fetchTables();
        dbState.tableView = std::make_unique<Table>(dbState.conn);
    } else {
        std::cerr << "Connection failed: " << PQerrorMessage(dbState.conn) << std::endl;
        PQfinish(dbState.conn);
        dbState.conn = nullptr;
    }
}

void DBE::disconnect() {
    if (!dbState.conn) return;
    
    dbState.tableView.reset();
    PQfinish(dbState.conn);
    dbState.conn = nullptr;
    dbState.tables.clear();
    dbState.selectedTable.clear();
    dbState.connectedHost.clear();
    dbState.connectedUser.clear();
    dbState.connectedPort.clear();
    dbState.connectionTimeMs = 0.0;
}

void DBE::shutdown() {
    disconnect();
}