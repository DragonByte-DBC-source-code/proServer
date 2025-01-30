#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <sstream>
#include <thread>
#include <sqlite3.h> // Include SQLite library

// SQLite database pointer
sqlite3 *db;

// Named constants
const int PORT = 8080;
const int BUFFER_SIZE = 4096;

// Function to execute SQL commands
int executeSQL(const std::string &sql)
{
    char *errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK)
    {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
    return rc;
}

// Function to fetch all IDs from the database
std::vector<string> fetchIDs()
{
    std::vector<string> ids;
    const char *query = "SELECT id FROM ids";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);

    if (rc != SQLITE_OK)
    {
        std::cerr << "SQL error: " << sqlite3_errmsg(db) << std::endl;
        return ids;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ids.push_back(sqlite3_column_int(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return ids;
}

// Function to parse HTTP requests and determine the route
std::string handleRequest(const std::string &request)
{
    std::string response;

    if (request.find("GET / ") == 0)
    {
        response = "HTTP/1.1 200 OK\r\nContent-Length: 12\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nYahel is GAY";
    }
    else if (request.find("POST /add") == 0)
    {
        size_t body_start = request.find("\r\n\r\n") + 4;
        std::string body = request.substr(body_start);
        int id = std::stoi(body);

        std::ostringstream sql;
        sql << "INSERT INTO ids (id) VALUES (" << id << ")";
        executeSQL(sql.str());

        response = "HTTP/1.1 200 OK\r\nContent-Length: 17\r\nConnection: close\r\n\r\nID added successfully";
    }
    else if (request.find("GET /ids") == 0)
    {
        std::vector<string> ids = fetchIDs();
        std::ostringstream json;
        json << "[";
        for (size_t i = 0; i < ids.size(); ++i)
        {
            json << ids[i];
            if (i < ids.size() - 1)
                json << ",";
        }
        json << "]";

        std::string jsonStr = json.str();
        response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                   std::to_string(jsonStr.size()) + "\r\nConnection: close\r\n\r\n" + jsonStr;
    }
    else
    {
        response = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\nConnection: close\r\n\r\n404 Not Found";
    }

    return response;
}

// Function to handle each client in a separate thread
void handleClient(int client_fd)
{
    char buffer[BUFFER_SIZE];
    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';
        std::string request(buffer);
        std::string response = handleRequest(request);
        send(client_fd, response.c_str(), response.size(), 0);
    }
    close(client_fd);
}

int main()
{
    // Open SQLite database
    int rc = sqlite3_open("ids.db", &db);
    if (rc)
    {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    // Create the 'ids' table if it doesn't exist
    const char *create_table_sql = "CREATE TABLE IF NOT EXISTS ids (id TEXT PRIMARY KEY);";
    executeSQL(create_table_sql);

    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        std::cerr << "Socket creation failed\n";
        sqlite3_close(db);
        return 1;
    }

    // Set up address structure
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind socket to address
    if (bind(server_fd, (sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        std::cerr << "Bind failed\n";
        close(server_fd);
        sqlite3_close(db);
        return 1;
    }

    // Start listening for connections
    if (listen(server_fd, SOMAXCONN) == -1)
    {
        std::cerr << "Listen failed\n";
        close(server_fd);
        sqlite3_close(db);
        return 1;
    }

    std::cout << "Server is running on port " << PORT << "\n";

    while (true)
    {
        // Accept incoming connection
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd == -1)
        {
            std::cerr << "Client accept failed\n";
            continue;
        }

        // Handle the client in a separate thread
        std::thread clientThread(handleClient, client_fd);
        clientThread.detach(); // Detach the thread to allow it to run independently
    }

    close(server_fd);
    sqlite3_close(db);
    return 0;
}
