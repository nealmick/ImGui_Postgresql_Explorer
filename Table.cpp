#include "Table.h"
#include <iostream>

Table::Table(PGconn *conn) : conn(conn) {}

Table::~Table() {}

void Table::loadTableData(const std::string &tableName, int offset)
{
    if (!conn)
        return;

    // If the table name is changing, clear columns; otherwise, keep them.
    if (currentTable != tableName)
    {
        currentTable = tableName;
        columns.clear();
    }
    currentOffset = offset;
    rows.clear();

    std::string dataQuery;
    // If columns are already loaded, we can use our filtered query (which includes sorting).
    if (!columns.empty())
    {
        dataQuery = buildFilteredQuery();
    }
    else
    {
        // First-time load: we need to fetch columns from the query result.
        dataQuery = "SELECT * FROM \"" + currentTable + "\" ORDER BY 1 LIMIT " + std::to_string(rowsPerPage) + " OFFSET " + std::to_string(offset);
    }

    std::cout << "Executing query: " << dataQuery << std::endl;
    PGresult *dataRes = PQexec(conn, dataQuery.c_str());

    if (PQresultStatus(dataRes) != PGRES_TUPLES_OK)
    {
        std::cerr << "Data query failed: " << PQerrorMessage(conn) << std::endl;
        PQclear(dataRes);
        return;
    }

    // If columns weren't loaded previously, fetch column names.
    if (columns.empty())
    {
        int numDataCols = PQnfields(dataRes);
        for (int i = 0; i < numDataCols; i++)
        {
            columns.push_back(PQfname(dataRes, i));
            std::cout << "Column " << i << ": " << columns.back() << std::endl;
        }
        // Initialize filters AFTER we have the columns.
        columnFilters.clear();
        columnFilters.resize(columns.size(), "");
    }

    // Get row data
    int numRows = PQntuples(dataRes);
    std::cout << "Found " << numRows << " rows" << std::endl;
    rows.reserve(numRows);
    for (int i = 0; i < numRows; i++)
    {
        std::vector<std::string> row;
        row.reserve(columns.size());
        for (int j = 0; j < columns.size(); j++)
        {
            if (PQgetisnull(dataRes, i, j))
                row.push_back("NULL");
            else
                row.push_back(PQgetvalue(dataRes, i, j));
        }
        rows.push_back(std::move(row));
    }

    PQclear(dataRes);
    checkForMoreRows();
}

void Table::render()
{
    if (columns.empty())
        return;

    // Add horizontal scrolling flag

    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX;

    float tableHeight = ImGui::GetContentRegionAvail().y - 40;

    if (ImGui::BeginTable("##TableData", columns.size(), flags, ImVec2(0, tableHeight)))
    {
        // Headers setup
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
            std::string filterId = "##Filter" + std::to_string(i);

            // Check if this column is being filtered/searched
            bool isFiltering = !columnFilters[i].empty();
            bool isActiveFilter = activeFilterColumn == i;

            if (isActiveFilter)
            {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

                // Set input as active when we first show it
                if (lastActiveColumn != activeFilterColumn)
                {
                    ImGui::SetKeyboardFocusHere();
                    lastActiveColumn = activeFilterColumn;
                }

                strncpy(filterBuffer, columnFilters[i].c_str(), sizeof(filterBuffer) - 1);
                filterBuffer[sizeof(filterBuffer) - 1] = '\0';

                bool filterChanged = false;
                // Use InputText with EnterReturnsTrue to commit changes normally
                if (ImGui::InputText(filterId.c_str(), filterBuffer, sizeof(filterBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
                {
                    filterChanged = true;
                }
                // Also, if the input loses focus and Escape was pressed, clear the filter
                if (ImGui::IsItemDeactivated() && ImGui::IsKeyPressed(ImGuiKey_Escape))
                {
                    filterBuffer[0] = '\0';
                    filterChanged = true;
                }
                if (filterChanged)
                {
                    columnFilters[i] = filterBuffer;
                    reloadWithFilters();
                    activeFilterColumn = -1;
                    lastActiveColumn = -1;
                }

                // Maintain focus while active/hovered
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
            else
            {
                // Only show the Selectable when not filtering
                std::string headerText = columns[i];
                if (!columnFilters[i].empty())
                {
                    headerText += " (*)";
                }

                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
                if (ImGui::Selectable(headerText.c_str(), false, ImGuiSelectableFlags_None, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight())))
                {
                    activeFilterColumn = i;
                    strncpy(filterBuffer, columnFilters[i].c_str(), sizeof(filterBuffer) - 1);
                }
                ImGui::PopStyleVar();
            }
        }

        // Rows
        for (int row = 0; row < rows.size(); row++)
        {
            if (!shouldShowRow(rows[row]))
                continue;
            ImGui::TableNextRow();
            for (int col = 0; col < columns.size(); col++)
            {
                ImGui::TableSetColumnIndex(col);

                // Create unique ID for each cell
                std::string cellId = "##" + currentTable + "_Cell_" + std::to_string(row) + "_" + std::to_string(col) + "_";
                std::string editId = "##" + currentTable + "_Edit_" + std::to_string(row) + "_" + std::to_string(col);

                if (isEditing && row == editRow && col == editCol)
                {
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

                    // Set initial focus if needed
                    if (!ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0))
                    {
                        ImGui::SetKeyboardFocusHere();
                    }

                    // Use InputTextMultiline for multi-line editing.
                    // Here we specify a height of 3 text lines; adjust as needed.
                    bool valueChanged = ImGui::InputTextMultiline(editId.c_str(), editBuffer, sizeof(editBuffer), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 3), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine);

                    ImGui::PopStyleVar();

                    // If Escape is pressed on deactivation, cancel the edit.
                    // Otherwise, if the widget loses focus, save the edit.
                    if (ImGui::IsItemDeactivated() && ImGui::IsKeyPressed(ImGuiKey_Escape))
                    {
                        cancelEdit();
                    }
                    else if (ImGui::IsItemDeactivated() && !ImGui::IsKeyPressed(ImGuiKey_Escape))
                    {
                        saveEdit();
                    }
                }
                else
                {
                    std::string value = rows[row][col];
                    // Extract only the first line if there is a newline
                    std::string firstLine = value;
                    size_t newlinePos = value.find('\n');
                    if (newlinePos != std::string::npos)
                    {
                        firstLine = value.substr(0, newlinePos);
                    }

                    ImVec2 pos = ImGui::GetCursorPos(); // Save current cursor position

                    // Render a selectable with a fixed height (one text line)
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
                    if (ImGui::Selectable(cellId.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight())))
                    {
                        if (ImGui::IsMouseDoubleClicked(0))
                        {
                            handleCellClick(row, col);
                        }
                    }
                    ImGui::PopStyleVar();

                    // Restore position and only display the first line of text
                    ImGui::SetCursorPos(pos);
                    ImGui::TextUnformatted(firstLine.c_str());
                }
            }
        }
        ImGui::EndTable();
    }

    // Pagination controls
    bool filteringActive = false;
    for (const auto &filter : columnFilters)
    {
        if (!filter.empty())
        {
            filteringActive = true;
            break;
        }
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::BeginChild("##Pagination", ImVec2(0, 30), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (filteringActive)
    {
        // Reset pagination when filtering is active.
        currentOffset = 0;
        ImGui::Text("Found %d matching rows", static_cast<int>(rows.size()));
        ImGui::SameLine();
        if (ImGui::Button("Clear Search"))
        {
            // Clear all search filters.
            for (auto &filter : columnFilters)
            {
                filter.clear();
            }
            reloadWithFilters();
        }
    }
    else
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

        // Now add a small combo box for sorting next to the pagination controls.
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100); // narrow combo box
        std::vector<const char *> colNames;
        for (const auto &col : columns)
            colNames.push_back(col.c_str());

        if (ImGui::Combo("Sort", &sortColumn, colNames.data(), (int)colNames.size()))
        {
            std::cout << "New sort column: " << columns[sortColumn] << std::endl;
            loadTableData(currentTable, currentOffset);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Rev", &sortAscending))
        {
            loadTableData(currentTable, currentOffset);
        }
    }
    ImGui::EndChild();
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

void Table::reloadWithFilters()
{
    if (!conn)
        return;

    rows.clear();

    std::string dataQuery = buildFilteredQuery();
    std::cout << "Executing filtered query: " << dataQuery << std::endl;

    PGresult *dataRes = PQexec(conn, dataQuery.c_str());

    if (PQresultStatus(dataRes) != PGRES_TUPLES_OK)
    {
        std::cerr << "Data query failed: " << PQerrorMessage(conn) << std::endl;
        PQclear(dataRes);
        return;
    }

    // Get row data
    int numRows = PQntuples(dataRes);
    int numDataCols = PQnfields(dataRes);

    rows.reserve(numRows);
    for (int i = 0; i < numRows; i++)
    {
        std::vector<std::string> row;
        row.reserve(numDataCols);
        for (int j = 0; j < numDataCols; j++)
        {
            if (PQgetisnull(dataRes, i, j))
            {
                row.push_back("NULL");
            }
            else
            {
                row.push_back(PQgetvalue(dataRes, i, j));
            }
        }
        rows.push_back(std::move(row));
    }

    PQclear(dataRes);
    checkForMoreRows();
}