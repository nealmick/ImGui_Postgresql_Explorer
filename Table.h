#pragma once
#include <algorithm> // for std::transform
#include <imgui.h>
#include <libpq-fe.h>
#include <map>
#include <string>
#include <vector>

class Table
{
  public:
    Table(PGconn *conn);
    ~Table();

    void loadTableData(const std::string &tableName, int offset = 0);
    void render();

  private:
    PGconn *conn;
    std::string currentTable;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
    int currentOffset = 0;
    int rowsPerPage = 100;
    bool hasMoreRows = false;

    // For editing
    bool isEditing = false;
    int editRow = -1;
    int editCol = -1;
    char editBuffer[256];

    // For sorting
    ImGuiTableSortSpecs *currentSort = nullptr;

    // For filtering
    std::vector<std::string> columnFilters; // Changed from char array
    char filterBuffer[256];                 // Buffer for ImGui input

    void handleCellClick(int row, int col);
    void saveEdit();
    void cancelEdit();
    std::string generateUpdateQuery(int row, int col, const std::string &newValue);
    void checkForMoreRows();
    void handleSorting();
    bool compareRows(const std::vector<std::string> &a, const std::vector<std::string> &b) const;
    bool shouldShowRow(const std::vector<std::string> &row) const;
    void reloadWithFilters();
    std::string buildFilteredQuery() const;
    int activeFilterColumn = -1;
    int lastActiveColumn = -1;
	
	int sortColumn = 0;       // default to first column
	bool sortAscending = true; // default order



};