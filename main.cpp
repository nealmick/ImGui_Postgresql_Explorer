#define GL_SILENCE_DEPRECATION
#include "Table.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <libpq-fe.h>
#include <memory>
#include <stdio.h>
#include <string>
#include <vector>

class DatabaseState
{
  public:
    char connStr[1024] = "";
    bool showPassword = false;
    PGconn *conn = nullptr;
    std::vector<std::string> tables;
    std::string selectedTable;
    std::unique_ptr<Table> tableView;
    bool isConnected() const { return conn != nullptr; }
    std::string connectedHost;
    std::string connectedUser;
    std::string connectedPort;
    double connectionTimeMs = 0.0;
    std::vector<std::string> fetchTables()
    {
        std::vector<std::string> tables;
        if (!conn)
            return tables;
        const char *query = "SELECT table_name FROM information_schema.tables WHERE table_schema = 'public'";
        PGresult *res = PQexec(conn, query);
        if (PQresultStatus(res) != PGRES_TUPLES_OK)
        {
            PQclear(res);
            return tables;
        }
        int rows = PQntuples(res);
        for (int i = 0; i < rows; i++)
        {
            tables.push_back(PQgetvalue(res, i, 0));
        }
        PQclear(res);
        return tables;
    }
    void connect()
    {
        if (conn)
            return;

        // Record start time
        auto startTime = std::chrono::high_resolution_clock::now();

        conn = PQconnectdb(connStr);
        if (PQstatus(conn) == CONNECTION_OK)
        {
            // Calculate connection time
            auto endTime = std::chrono::high_resolution_clock::now();
            connectionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

            // Get connection info
            connectedHost = PQhost(conn) ? PQhost(conn) : "localhost";
            connectedUser = PQuser(conn) ? PQuser(conn) : "unknown";
            connectedPort = PQport(conn) ? PQport(conn) : "5432";

            tables = fetchTables();
            tableView = std::make_unique<Table>(conn);
        }
        else
        {
            std::cerr << "Connection failed: " << PQerrorMessage(conn) << std::endl;
            PQfinish(conn);
            conn = nullptr;
        }
    }

    void disconnect()
    {
        if (conn)
        {
            tableView.reset();
            PQfinish(conn);
            conn = nullptr;
            tables.clear();
            selectedTable.clear();
            connectedHost.clear();
            connectedUser.clear();
            connectedPort.clear();
            connectionTimeMs = 0.0;
        }
    }

    ~DatabaseState() { disconnect(); }
};

int main()
{
    if (!glfwInit())
        return 1;
    const char *glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    GLFWwindow *window = glfwCreateWindow(1280, 720, "Database Explorer", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    DatabaseState dbState;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Set up the main window without padding
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Database Explorer", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::PopStyleVar();

        // --- Top Connection Area ---
        // Increase frame padding for taller elements
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 8)); // Increased Y padding from 2 to 8

        // Add padding by adjusting the position and size of TopBar
        float topPadding = 8.0f;   // Padding for top
        float sidePadding = 10.0f; // Padding for sides

        // Calculate the adjusted width and position
        float windowWidth = ImGui::GetWindowWidth();
        float contentWidth = windowWidth - (2 * sidePadding);

        // Set the position with left padding
        ImGui::SetCursorPos(ImVec2(5.0f, topPadding));

        // Begin child with adjusted width
        ImGui::BeginChild("TopBar", ImVec2(contentWidth, 33), false);
        // Compute widths for full width usage
        float totalWidth = ImGui::GetWindowWidth();
        float buttonWidth = 120;                                // adjust as needed
        float inputWidth = totalWidth - (buttonWidth * 2 + 20); // 20 pixels for spacing

        // Define the flags for connection input
        ImGuiInputTextFlags flags = dbState.showPassword ? 0 : ImGuiInputTextFlags_Password;

        // Push font scaling to make text bigger (optional)
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::SetNextItemWidth(inputWidth);

        // Use placeholder text with custom height
        ImGui::InputTextWithHint("##ConnStr", "Type connection string here...", dbState.connStr, sizeof(dbState.connStr), flags);

        ImGui::SameLine();
        // Make buttons taller by specifying height
        if (ImGui::Button(dbState.showPassword ? "Hide" : "Show", ImVec2(buttonWidth, ImGui::GetFrameHeight())))
        {
            dbState.showPassword = !dbState.showPassword;
        }

        ImGui::SameLine();
        if (dbState.isConnected())
        {
            // Push red color for disconnect button
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f)); // Red tint
            if (ImGui::Button("Disconnect", ImVec2(buttonWidth, ImGui::GetFrameHeight())))
            {
                dbState.disconnect();
            }
            ImGui::PopStyleColor();
        }
        else
        {
            if (ImGui::Button("Connect", ImVec2(buttonWidth, ImGui::GetFrameHeight())))
            {
                dbState.connect();
            }
        }
        ImGui::PopStyleVar(); // Pop frame border size

        ImGui::EndChild();
        ImGui::PopStyleVar(); // Pop frame padding

        // Connection Info Bar
        if (dbState.isConnected())
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7, 3));
            ImGui::BeginChild("ConnectionInfo", ImVec2(contentWidth, 25), false);

            // Use a dimmed text color
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));

            // Format connection string nicely with spacing
            std::string connInfo = "User: " + dbState.connectedUser + "  |  Port: " + dbState.connectedPort + "  |  Conn Time: " + std::to_string(static_cast<int>(dbState.connectionTimeMs)) + "ms" + "  |  Host: " + dbState.connectedHost;

            ImGui::SetCursorPos(ImVec2(10, 4)); // Add some padding
            ImGui::Text("%s", connInfo.c_str());

            ImGui::PopStyleColor();
            ImGui::EndChild();
            ImGui::PopStyleVar();
        }

        // --- Content Area ---
        // Begin main content area with no border.
        ImGui::BeginChild("Content", ImVec2(0, 0), false);

        // Left Panel: Table List (only if connected)
        if (dbState.isConnected())
        {
            ImGui::BeginChild("LeftPanel", ImVec2(200, 0), true);
            ImGui::Text("Tables");
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 2.0f));
            for (const auto &table : dbState.tables)
            {
                if (ImGui::Selectable(table.c_str(), dbState.selectedTable == table))
                {
                    dbState.selectedTable = table;
                }
            }
            ImGui::EndChild();
        }
        else
        {
            // When not connected, hide table list completely.
            ImGui::BeginChild("LeftPanel", ImVec2(200, 0), false);
            ImGui::EndChild();
        }
        ImGui::SameLine();

        // Main Panel: Table Display and Editor
        ImGui::BeginChild("MainPanel", ImVec2(0, 0), false);
        if (!dbState.selectedTable.empty() && dbState.tableView)
        {
            // Add padding at the top of the panel
            ImGui::Dummy(ImVec2(0, 4.0f)); // This adds 3 pixels of vertical space

            // Display table title (capitalize first letter)
            std::string title = dbState.selectedTable;
            if (!title.empty())
                title[0] = std::toupper(title[0]);
            title += " Table";
            ImGui::Text("%s", title.c_str());
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 2.0f));
            static std::string lastTable;
            if (lastTable != dbState.selectedTable)
            {
                dbState.tableView->loadTableData(dbState.selectedTable, 0);
                lastTable = dbState.selectedTable;
            }
            dbState.tableView->render();
        }
        ImGui::EndChild();

        ImGui::EndChild(); // End Content
        ImGui::End();      // End main window

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}