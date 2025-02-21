#pragma once

// Standard library includes
#include <algorithm>
#include <string>
#include <vector>
#include <map>

// External library includes
#include <imgui.h>
#include <libpq-fe.h>

class Table {
public:
    // Constructor/Destructor
    Table(PGconn* conn);
    ~Table();

    // Main public interface
    void loadTableData(const std::string& tableName, int offset = 0);
    void render();

private:
    // Database connection and state
    PGconn* conn;
    std::string currentTable;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
    int currentOffset = 0;
    int rowsPerPage = 100;
    bool hasMoreRows = false;

    // Table data management
    void initializeTable(const std::string& tableName, int offset);
    void executeQuery(const std::string& query, PGresult*& result);
    void clearAndExecuteQuery(const std::string& query, PGresult*& result);
    void loadColumns(PGresult* result);
    void loadRows(PGresult* result);
    void loadRowsFromResult(PGresult* result);
    std::string buildInitialQuery(int offset) const;
    std::string buildFilteredQuery() const;
    void checkForMoreRows();

    // Editing functionality
    bool isEditing = false;
    int editRow = -1;
    int editCol = -1;
    char editBuffer[256];
    void handleCellClick(int row, int col);
    void saveEdit();
    void cancelEdit();
    std::string generateUpdateQuery(int row, int col, const std::string& newValue);

    // Sorting functionality
    ImGuiTableSortSpecs* currentSort = nullptr;
    int sortColumn = 0;
    bool sortAscending = true;
    void handleSorting();
    bool compareRows(const std::vector<std::string>& a, 
                    const std::vector<std::string>& b) const;

    // Filtering functionality
    std::vector<std::string> columnFilters;
    char filterBuffer[256];
    int activeFilterColumn = -1;
    int lastActiveColumn = -1;
    bool isFilterActive() const;
    bool shouldShowFilter(size_t colIndex) const;
    bool shouldShowRow(const std::vector<std::string>& row) const;
    void reloadWithFilters();

    // UI Rendering - Table
    void setupTableFlags(ImGuiTableFlags& flags) const;
    void renderTableHeaders();
    void renderTableRows();
    void renderTableCell(int row, int col);
    void renderTableCellEdit(int row, int col);
    void renderTableCellContent(int row, int col);

    // UI Rendering - Headers
    void renderFilterInput(size_t colIndex);
    void renderHeaderCell(size_t colIndex);

    // UI Rendering - Pagination
    void renderPagination();
    void renderFilteringControls();
    void renderPaginationControls();
    void renderSortingControls();
};