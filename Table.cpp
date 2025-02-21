#include "Table.h"
#include <iostream>

Table::Table(PGconn *conn) : conn(conn) {}

Table::~Table() {}

void Table::render()
{
    if (columns.empty())
    {
        return;
    }

    ImGuiTableFlags flags;
    setupTableFlags(flags);
    float tableHeight = ImGui::GetContentRegionAvail().y - 40;

    if (ImGui::BeginTable("##TableData", columns.size(), flags, ImVec2(0, tableHeight)))
    {
        renderTableHeaders();
        renderTableRows();
        ImGui::EndTable();
    }

    renderPagination();
}

void Table::loadTableData(const std::string &tableName, int offset)
{
    if (!conn)
        return;

    initializeTable(tableName, offset);

    std::string dataQuery = columns.empty() ? buildInitialQuery(offset) : buildFilteredQuery();

    PGresult *dataRes = nullptr;
    executeQuery(dataQuery, dataRes);
    if (!dataRes)
        return;

    if (columns.empty())
    {
        loadColumns(dataRes);
    }

    loadRows(dataRes);
    PQclear(dataRes);
    checkForMoreRows();
}

void Table::initializeTable(const std::string &tableName, int offset)
{
    if (currentTable != tableName)
    {
        currentTable = tableName;
        columns.clear();
    }
    currentOffset = offset;
    rows.clear();
}

void Table::executeQuery(const std::string &query, PGresult *&result)
{
    std::cout << "Executing query: " << query << std::endl;
    result = PQexec(conn, query.c_str());

    if (PQresultStatus(result) != PGRES_TUPLES_OK)
    {
        std::cerr << "Data query failed: " << PQerrorMessage(conn) << std::endl;
        PQclear(result);
        result = nullptr;
    }
}

void Table::loadColumns(PGresult *result)
{
    int numDataCols = PQnfields(result);
    columns.reserve(numDataCols);

    for (int i = 0; i < numDataCols; i++)
    {
        columns.push_back(PQfname(result, i));
        std::cout << "Column " << i << ": " << columns.back() << std::endl;
    }

    // Initialize filters after loading columns
    columnFilters.clear();
    columnFilters.resize(columns.size(), "");
}

void Table::loadRows(PGresult *result)
{
    int numRows = PQntuples(result);
    std::cout << "Found " << numRows << " rows" << std::endl;

    rows.reserve(numRows);
    for (int i = 0; i < numRows; i++)
    {
        std::vector<std::string> row;
        row.reserve(columns.size());

        for (int j = 0; j < columns.size(); j++)
        {
            if (PQgetisnull(result, i, j))
            {
                row.push_back("NULL");
            }
            else
            {
                row.push_back(PQgetvalue(result, i, j));
            }
        }
        rows.push_back(std::move(row));
    }
}

std::string Table::buildInitialQuery(int offset) const { return "SELECT * FROM \"" + currentTable + "\" ORDER BY 1 LIMIT " + std::to_string(rowsPerPage) + " OFFSET " + std::to_string(offset); }

void Table::renderTableRows()
{
    for (int row = 0; row < rows.size(); row++)
    {
        if (!shouldShowRow(rows[row]))
        {
            continue;
        }

        ImGui::TableNextRow();
        for (int col = 0; col < columns.size(); col++)
        {
            ImGui::TableSetColumnIndex(col);
            if (isEditing && row == editRow && col == editCol)
            {
                renderTableCellEdit(row, col);
            }
            else
            {
                renderTableCell(row, col);
            }
        }
    }
}

void Table::renderTableCell(int row, int col)
{
    std::string cellId = "##" + currentTable + "_Cell_" + std::to_string(row) + "_" + std::to_string(col) + "_";

    // Extract first line for display
    std::string value = rows[row][col];
    std::string firstLine = value;
    size_t newlinePos = value.find('\n');
    if (newlinePos != std::string::npos)
    {
        firstLine = value.substr(0, newlinePos);
    }

    ImVec2 pos = ImGui::GetCursorPos();

    // Render selectable
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
    if (ImGui::Selectable(cellId.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight())))
    {
        if (ImGui::IsMouseDoubleClicked(0))
        {
            handleCellClick(row, col);
        }
    }
    ImGui::PopStyleVar();

    // Render text content
    ImGui::SetCursorPos(pos);
    ImGui::TextUnformatted(firstLine.c_str());
}

void Table::renderTableCellEdit(int row, int col)
{
    std::string editId = "##" + currentTable + "_Edit_" + std::to_string(row) + "_" + std::to_string(col);

    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

    if (!ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0))
    {
        ImGui::SetKeyboardFocusHere();
    }

    bool valueChanged = ImGui::InputTextMultiline(editId.c_str(), editBuffer, sizeof(editBuffer), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 3), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine);

    ImGui::PopStyleVar();

    if (ImGui::IsItemDeactivated())
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            cancelEdit();
        }
        else
        {
            saveEdit();
        }
    }
}

bool Table::isFilterActive() const
{
    return std::any_of(columnFilters.begin(), columnFilters.end(), [](const std::string &filter) { return !filter.empty(); });
}

void Table::renderPagination()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::BeginChild("##Pagination", ImVec2(0, 30), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (isFilterActive())
    {
        renderFilteringControls();
    }
    else
    {
        renderPaginationControls();
        renderSortingControls();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void Table::renderFilteringControls()
{
    currentOffset = 0;
    ImGui::Text("Found %d matching rows", static_cast<int>(rows.size()));
    ImGui::SameLine();
    if (ImGui::Button("Clear Search"))
    {
        for (auto &filter : columnFilters)
        {
            filter.clear();
        }
        reloadWithFilters();
    }
}

void Table::renderPaginationControls()
{
    if (currentOffset > 0)
    {
        if (ImGui::Button("Previous"))
        {
            currentOffset = std::max(0, currentOffset - rowsPerPage);
            loadTableData(currentTable, currentOffset);
        }
        ImGui::SameLine();
    }

    ImGui::Text("Page %d (rows %d-%d)", (currentOffset / rowsPerPage) + 1, static_cast<int>(currentOffset + 1), static_cast<int>(currentOffset + rows.size()));

    if (hasMoreRows)
    {
        ImGui::SameLine();
        if (ImGui::Button("Next"))
        {
            currentOffset += rowsPerPage;
            loadTableData(currentTable, currentOffset);
        }
    }
}

void Table::renderSortingControls()
{
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);

    std::vector<const char *> colNames;
    for (const auto &col : columns)
    {
        colNames.push_back(col.c_str());
    }

    if (ImGui::Combo("Sort", &sortColumn, colNames.data(), (int)colNames.size()))
    {
        loadTableData(currentTable, currentOffset);
    }

    ImGui::SameLine();
    if (ImGui::Checkbox("Reverse", &sortAscending))
    {
        loadTableData(currentTable, currentOffset);
    }
}

void Table::setupTableFlags(ImGuiTableFlags &flags) const { flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX; }

void Table::renderTableHeaders()
{
    ImGui::TableSetupScrollFreeze(0, 1);
    for (const auto &col : columns)
    {
        ImGui::TableSetupColumn(col.c_str(), ImGuiTableColumnFlags_None | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, ImGui::GetWindowWidth() * 0.1f);
    }

    // Custom header row that combines headers and filters
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    for (size_t i = 0; i < columns.size(); i++)
    {
        ImGui::TableSetColumnIndex(i);
        if (shouldShowFilter(i))
        {
            renderFilterInput(i);
        }
        else
        {
            renderHeaderCell(i);
        }
    }
}

bool Table::shouldShowFilter(size_t colIndex) const { return activeFilterColumn == colIndex; }

void Table::renderFilterInput(size_t colIndex)
{
    std::string filterId = "##Filter" + std::to_string(colIndex);

    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

    if (lastActiveColumn != activeFilterColumn)
    {
        ImGui::SetKeyboardFocusHere();
        lastActiveColumn = activeFilterColumn;
    }

    strncpy(filterBuffer, columnFilters[colIndex].c_str(), sizeof(filterBuffer) - 1);
    filterBuffer[sizeof(filterBuffer) - 1] = '\0';

    bool filterChanged = false;
    if (ImGui::InputText(filterId.c_str(), filterBuffer, sizeof(filterBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
    {
        filterChanged = true;
    }

    if (ImGui::IsItemDeactivated() && ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        filterBuffer[0] = '\0';
        filterChanged = true;
    }

    if (filterChanged)
    {
        columnFilters[colIndex] = filterBuffer;
        reloadWithFilters();
        activeFilterColumn = -1;
        lastActiveColumn = -1;
    }

    if (ImGui::IsItemActive() || ImGui::IsItemFocused())
    {
        ImGui::SetItemDefaultFocus();
    }
    else if (!ImGui::IsItemHovered())
    {
        activeFilterColumn = -1;
        lastActiveColumn = -1;
    }

    ImGui::PopStyleVar();
}

void Table::renderHeaderCell(size_t colIndex)
{
    std::string headerText = columns[colIndex];
    if (!columnFilters[colIndex].empty())
    {
        headerText += " (*)";
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
    if (ImGui::Selectable(headerText.c_str(), false, ImGuiSelectableFlags_None, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight())))
    {
        activeFilterColumn = colIndex;
        strncpy(filterBuffer, columnFilters[colIndex].c_str(), sizeof(filterBuffer) - 1);
    }
    ImGui::PopStyleVar();
}

void Table::handleCellClick(int row, int col)
{
    editRow = row;
    editCol = col;
    isEditing = true;
    strncpy(editBuffer, rows[row][col].c_str(), sizeof(editBuffer) - 1);
}

void Table::saveEdit()
{
    if (!isEditing || editRow < 0 || editCol < 0)
        return;

    std::string query = generateUpdateQuery(editRow, editCol, editBuffer);
    PGresult *res = PQexec(conn, query.c_str());

    if (PQresultStatus(res) == PGRES_COMMAND_OK)
    {
        // Update successful, update local data
        rows[editRow][editCol] = editBuffer;
    }
    else
    {
        std::cerr << "Update failed: " << PQerrorMessage(conn) << std::endl;
    }

    PQclear(res);
    isEditing = false;
    editRow = -1;
    editCol = -1;
}

void Table::cancelEdit()
{
    isEditing = false;
    editRow = -1;
    editCol = -1;
}

std::string Table::generateUpdateQuery(int row, int col, const std::string &newValue)
{
    // Use quoted identifiers for table and column names
    std::string query = "UPDATE \"" + currentTable + "\" SET \"" + columns[editCol] + "\" = '" + newValue + "' WHERE ";

    // Use all columns for WHERE clause to uniquely identify the row
    bool first = true;
    for (size_t i = 0; i < columns.size(); i++)
    {
        if (!first)
            query += " AND ";
        query += "\"" + columns[i] + "\" = '" + rows[row][i] + "'";
        first = false;
    }

    std::cout << "Executing update query: " << query << std::endl; // Debug output
    return query;
}

void Table::checkForMoreRows()
{
    if (!conn)
        return;

    std::string countQuery = "SELECT EXISTS(SELECT 1 FROM \"" + currentTable + "\" LIMIT 1 OFFSET " + std::to_string(currentOffset + rowsPerPage) + ")";

    PGresult *res = PQexec(conn, countQuery.c_str());
    if (PQresultStatus(res) == PGRES_TUPLES_OK)
    {
        hasMoreRows = std::string(PQgetvalue(res, 0, 0)) == "t";
    }
    PQclear(res);
}

void Table::handleSorting()
{
    ImGuiTableSortSpecs *sorts_specs = ImGui::TableGetSortSpecs();
    if (sorts_specs && sorts_specs->SpecsDirty)
    {
        currentSort = sorts_specs;

        if (currentSort->SpecsCount > 0)
        {
            std::sort(rows.begin(), rows.end(), [this](const std::vector<std::string> &a, const std::vector<std::string> &b) { return this->compareRows(a, b); });
        }

        sorts_specs->SpecsDirty = false;
    }
}

bool Table::compareRows(const std::vector<std::string> &a, const std::vector<std::string> &b) const
{
    if (!currentSort || currentSort->SpecsCount == 0)
        return false;

    for (int i = 0; i < currentSort->SpecsCount; i++)
    {
        const ImGuiTableColumnSortSpecs *sort_spec = &currentSort->Specs[i];
        int col = sort_spec->ColumnIndex;
        if (col >= a.size() || col >= b.size())
            continue;

        int cmp = a[col].compare(b[col]);
        if (cmp != 0)
            return sort_spec->SortDirection == ImGuiSortDirection_Ascending ? cmp < 0 : cmp > 0;
    }
    return false;
}

bool Table::shouldShowRow(const std::vector<std::string> &row) const
{
    if (columnFilters.size() != columns.size() || row.size() != columns.size())
    {
        return true; // Safety check: if sizes don't match, show the row
    }

    for (size_t i = 0; i < columns.size(); i++)
    {
        if (!columnFilters[i].empty())
        { // If filter is not empty
            std::string filter = columnFilters[i];
            std::string value = row[i];
            // Convert both to lowercase for case-insensitive search
            std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
            std::transform(value.begin(), value.end(), value.begin(), ::tolower);
            if (value.find(filter) == std::string::npos)
                return false;
        }
    }
    return true;
}

std::string Table::buildFilteredQuery() const
{
    std::string query = "SELECT * FROM \"" + currentTable + "\" WHERE 1=1";

    for (size_t i = 0; i < columns.size(); i++)
    {
        if (!columnFilters[i].empty())
        {
            query += " AND LOWER(\"" + columns[i] + "\"::text) LIKE LOWER('%" + columnFilters[i] + "%')";
        }
    }
    // Incorporate the selected sort column and order.
    query += " ORDER BY \"" + columns[sortColumn] + "\" " + (sortAscending ? "ASC" : "DESC");
    query += " LIMIT " + std::to_string(rowsPerPage) + " OFFSET " + std::to_string(currentOffset);
    return query;
}
void Table::clearAndExecuteQuery(const std::string &query, PGresult *&result)
{
    rows.clear();
    executeQuery(query, result); // Using our existing executeQuery function
}

void Table::loadRowsFromResult(PGresult *result)
{
    int numRows = PQntuples(result);
    int numCols = PQnfields(result);

    rows.reserve(numRows);
    for (int i = 0; i < numRows; i++)
    {
        std::vector<std::string> row;
        row.reserve(numCols);
        for (int j = 0; j < numCols; j++)
        {
            row.push_back(PQgetisnull(result, i, j) ? "NULL" : PQgetvalue(result, i, j));
        }
        rows.push_back(std::move(row));
    }
}

void Table::reloadWithFilters()
{
    if (!conn)
        return;

    std::string dataQuery = buildFilteredQuery();
    PGresult *dataRes = nullptr;

    clearAndExecuteQuery(dataQuery, dataRes);
    if (!dataRes)
        return;

    loadRowsFromResult(dataRes);
    PQclear(dataRes);
    checkForMoreRows();
}