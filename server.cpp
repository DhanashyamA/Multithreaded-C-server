#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fstream>
#include <sstream>

// Import our custom headers
#include "threadpool.hpp"
#include "thread_safe_cache.hpp"

using namespace std;

const int PORT = 8080;

// Instantiate the global memory cache
ThreadSafeCache fileCache;

// Helper function to read raw files from the hard drive
string readFileFromDisk(const string& filepath) {
    // We assume static files live in a folder named 'public'
    ifstream file("public" + filepath); 
    if (!file.is_open()) return "";
    
    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// The Worker Thread Function
void handle_client(int client_socket) {
    char buffer[4096] = {0};
    read(client_socket, buffer, 4096);

    // 1. Basic HTTP Parsing (Extracting the requested path)
    string request(buffer);
    string method, path, protocol;
    istringstream request_stream(request);
    request_stream >> method >> path >> protocol;

    // Route default '/' to 'index.html'
    if (path == "/") path = "/index.html"; 

    string response_body;
    string http_status = "200 OK";

    // 2. Try the Memory Cache (The C++17 Optimization)
    auto cached_file = fileCache.getFile(path);
    
    if (cached_file.has_value()) {
        response_body = cached_file.value();
        cout << "[Thread " << this_thread::get_id() << "] CACHE HIT: " << path << "\n";
    } else {
        // 3. Cache Miss: Fall back to slow Disk I/O
        response_body = readFileFromDisk(path);
        
        if (response_body.empty()) {
            http_status = "404 Not Found";
            response_body = "<h1>404 Error: File Not Found</h1>";
            cout << "[Thread " << this_thread::get_id() << "] 404 ERROR: " << path << "\n";
        } else {
            // Success! Save it to RAM so the next user gets it instantly.
            fileCache.saveFile(path, response_body);
            cout << "[Thread " << this_thread::get_id() << "] CACHE MISS (Loaded from disk): " << path << "\n";
        }
    }

    // 4. Construct and send the HTTP Response
    string http_response = "HTTP/1.1 " + http_status + "\r\n"
                           "Content-Type: text/html\r\n"
                           "Content-Length: " + to_string(response_body.length()) + "\r\n"
                           "Connection: close\r\n\r\n" +
                           response_body;

    send(client_socket, http_response.c_str(), http_response.length(), 0);
    close(client_socket);
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // 1. Create POSIX Socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 2. Bind Socket to Port 8080
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // 3. Listen for traffic
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    cout << "🚀 Multithreaded C++ Server running on http://localhost:" << PORT << "\n";

    // 4. Initialize the Thread Pool (e.g., 8 Worker Threads)
    ThreadPool pool(8);

    // 5. The Boss Thread (Infinite Accept Loop)
    while (true) {
        int client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        // The Boss tosses the new client to the Worker Queue
        pool.enqueue([client_socket]() {
            handle_client(client_socket);
        });
    }

    return 0;
}