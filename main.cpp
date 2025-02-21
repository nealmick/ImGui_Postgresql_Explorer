#define GL_SILENCE_DEPRECATION
#include "Table.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
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
        conn = PQconnectdb(connStr);
        if (PQstatus(conn) == CONNECTION_OK)
        {
            tables = fetchTables();
            tableView = std::make_unique<Table>(conn); // Add this line
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
            tableView.reset(); // Add this line
            PQfinish(conn);
            conn = nullptr;
            tables.clear();
            selectedTable.clear();
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
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  // Enable docking

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

        // Create a full window without padding
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGui::Begin("Database Explorer", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImGui::PopStyleVar();

        // Top bar for connection
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));
        ImGui::BeginChild("TopBar", ImVec2(ImGui::GetWindowWidth(), 60), true);

        // Connection string input with show/hide
        ImGuiInputTextFlags flags = dbState.showPassword ? 0 : ImGuiInputTextFlags_Password;
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 200);
        ImGui::InputText("##ConnStr", dbState.connStr, sizeof(dbState.connStr), flags);
        ImGui::SameLine();
        if (ImGui::Button(dbState.showPassword ? "Hide" : "Show"))
        {
            dbState.showPassword = !dbState.showPassword;
        }
        ImGui::SameLine();
        if (ImGui::Button(dbState.isConnected() ? "Disconnect" : "Connect"))
        {
            if (dbState.isConnected())
            {
                dbState.disconnect();
            }
            else
            {
                dbState.connect();
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();

        // Main content area with table list on left
        ImGui::BeginChild("Content", ImVec2(0, 0), true);

        // Left panel for table list
        ImGui::BeginChild("LeftPanel", ImVec2(200, 0), true);
        for (const auto &table : dbState.tables)
        {
            if (ImGui::Selectable(table.c_str(), dbState.selectedTable == table))
            {
                dbState.selectedTable = table;
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // Main panel
        ImGui::BeginChild("MainPanel", ImVec2(0, 0), true);
        if (!dbState.selectedTable.empty() && dbState.tableView)
        {
            static std::string lastTable;
            if (lastTable != dbState.selectedTable)
            {
                dbState.tableView->loadTableData(dbState.selectedTable, 0); // Reset to first page
                lastTable = dbState.selectedTable;
            }
            dbState.tableView->render();
        }
        ImGui::EndChild();

        ImGui::EndChild();
        ImGui::End();

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