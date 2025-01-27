#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <tchar.h>
#include <thread>
#include <vector>
#include <iomanip>
#include <sstream>
#include <sqlite3.h>


using namespace std;

#pragma comment(lib, "ws2_32.lib") //Links to the ws2_32.lib library


//Struct that is used for storing information about clients
typedef struct
{
	SOCKET client;
	string username;
}dict;

//Struct that is used for storing info gotten from database about previous chats of user
struct CallbackData {
    SOCKET client_socket;
    string message;
};



/*      Function prototypes     */

bool initialize();
void print_serveraddr(const string);
bool search_users(vector<dict>&, string);
void handleLogin(sqlite3*, SOCKET, const string&, const string&);
void insertMessage(sqlite3*, const string&, const string&, const string&);
static int callback(void*, int, char**, char**);
void getMessagesForUser(sqlite3*, const string&, const string&, SOCKET);
string format_time(const tm&);
void timed_msg(SOCKET, vector<dict>&, string, size_t, string);
void send_receive(SOCKET, vector<dict>&, sqlite3*);


/*      Main function           */

int main() 
{
    if (!initialize()) 
    {
        cout << "Winsock initialization failed..." << endl; //Checking for errors
        return -1;
    }
    cout << "Server" << endl;

    //Setting up the database for SQL queries
    sqlite3* db;
    int rc = sqlite3_open("msg.db", &db);
    if (rc) 
    {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        return -1;
    }
    else 
    {
        cout << "Opened database successfully" << endl;
    }

    const char* createMessagesTable =
        "CREATE TABLE IF NOT EXISTS Messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "sender TEXT NOT NULL,"
        "receiver TEXT NOT NULL, "
        "message TEXT NOT NULL, "
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";


    char* errMsg = 0;
    rc = sqlite3_exec(db, createMessagesTable, 0, 0, &errMsg);
    if (rc != SQLITE_OK) 
    {
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
    }
    else 
    {
        cout << "Table created successfully" << endl;
    }
    const char* createLoginTable =
        "CREATE TABLE IF NOT EXISTS Login ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "username TEXT NOT NULL,"
        "password TEXT NOT NULL"
        ");";

    rc = sqlite3_exec(db, createLoginTable, 0, 0, &errMsg);
    if (rc != SQLITE_OK)
    {
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
    }
    else 
    {
        cout << "Table created successfully" << endl;
    }

    //Creation of socket
    SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, 0); 
    if (listen_socket == INVALID_SOCKET) 
    {
        cout << "Socket creation failed..." << endl;
        return -1;
    }

    //Creating address
    sockaddr_in serveraddr; //Struct that will store server address info
    int port = 12345;
    serveraddr.sin_family = AF_INET; //Specifies the use of Ipv4 address
    serveraddr.sin_port = htons(port);

    //Converting the address info to the usable binary form
    if (InetPton(AF_INET, _T("0.0.0.0"), &serveraddr.sin_addr) != 1) 
    {
        cout << "Setting address failed..." << endl;
        closesocket(listen_socket);
        WSACleanup();
        return -1;
    }
    
    //Binding the socket to the address
    if (bind(listen_socket, reinterpret_cast<sockaddr*>(&serveraddr), sizeof(serveraddr)) == SOCKET_ERROR) 
    {
        cout << "Bind failed..." << endl;
        closesocket(listen_socket);
        WSACleanup();
        return -1;
    }

    //Listening for any connections
    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) 
    {
        cout << "Listen failed... " << endl; //Error during binding
        closesocket(listen_socket);
        WSACleanup();
        return -1;
    }

    cout << "Socket has started listening on port..." << endl;

    //Name of the device that the server is running on
    string hostname = "DESKTOP-39N5UHM";
    print_serveraddr(hostname);

    //Vector to store clients
    vector<dict> clients;

    //Continuously accept connections by clients till server closes
    while (true) 
    {
        SOCKET client_socket = accept(listen_socket, nullptr, nullptr);
        if (client_socket == INVALID_SOCKET) 
        {
            cout << "Invalid client socket" << endl;
            continue;
        }
        cout << "Client socket: " << client_socket << endl;

        //Execute send and receive function in a thread
        thread t1(send_receive, client_socket, ref(clients), db);
        t1.detach();
    }


    //Close the database and socket when quiting
    sqlite3_close(db);
    closesocket(listen_socket);
    WSACleanup();

    return 0;
}



/*          Definitions of functions that are used in the Main Window function and other areas              */


//Initialize Winsock library to be used
bool initialize()
{
    WSADATA data;

    // Initialize Winsock library version 2.2
    // Returns true if the initialization is successful, otherwise false
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}


//Function to print server address on the console
void print_serveraddr(const string hostname)
{
    addrinfo hints = { 0 };    //inialize a struct addrinfo to values of 0
    addrinfo* res = nullptr;   //pointer to addrinfo, will store the result

    hints.ai_family = AF_INET; //for ipv4 address usage
    hints.ai_socktype = SOCK_STREAM; //for TCP connections

    if (getaddrinfo(hostname.c_str(), nullptr, &hints, &res) != 0) //If getaddrinfo does not run correctly
    {
        cout << "Failure while getting server address: " << WSAGetLastError << endl;
        return;
    }

    for (addrinfo* p = res; p != nullptr; p = p->ai_next)  //go through the res pointer
    {
        sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(p->ai_addr);
        char ip[INET_ADDRSTRLEN]; //list of char(string) thatll store the address
        inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof ip);
        cout << "Server ip address: " << ip << endl;
    }

    freeaddrinfo(res); //free memory used by res

}

bool search_users(vector<dict>& clients, string username)
{
    for (size_t i = 0; i < clients.size(); i++)
        if (clients[i].username == username)
            return true;
    return false;
}



//Function to handle the users loging in
void handleLogin(sqlite3* db, SOCKET client_socket, const string& username, const string& password)
{
    string query = "SELECT password FROM Login WHERE username = '" + username + "';";
    char* errMsg = 0;
    char** result = NULL;
    int rows, columns;
    int rc = sqlite3_get_table(db, query.c_str(), &result, &rows, &columns, &errMsg);
    if (rc != SQLITE_OK)
    {
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
        return;
    }
    if (rows == 1)
    {
        string stored_password = result[1];
        cout << "Stored password: " << stored_password << endl;
        if (stored_password == password)
        {
            string success = "#.#"; //message to alert correct login
            send(client_socket, success.c_str(), success.length(), 0);
        }
        else
        {
            string fail = "Invalid password!";
            send(client_socket, fail.c_str(), fail.length(), 0);
        }
    }
    else
    {
        string insert_query = "INSERT INTO Login (username, password) VALUES ('" + username + "', '" + password + "');";
        rc = sqlite3_exec(db, insert_query.c_str(), 0, 0, &errMsg);
        if (rc != SQLITE_OK)
        {
            cerr << "SQL error: " << errMsg << endl;
            sqlite3_free(errMsg);
        }
        else
        {
            string success = "#.#";
            send(client_socket, success.c_str(), success.length(), 0);
        }
    }
    sqlite3_free_table(result);
}



//Function to insert messages in the database
void insertMessage(sqlite3* db, const string& sender, const string& receiver, const string& message)
{
    //SQL query
    string sql = "INSERT INTO Messages (sender, receiver, message) VALUES ('" + sender + "', '" + receiver + "', '" + message + "');";

    char* errMsg = 0;
    int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &errMsg);
    if (rc != SQLITE_OK)
    {
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
    }
    else
    {
        cout << "Message: " + message + ": inserted successfully" << endl;
    }
}


//Function to get messages separately from the database and send it to the client
static int callback(void* data, int argc, char** argv, char** azColName)
{
    CallbackData* callbackData = static_cast<CallbackData*>(data);
    SOCKET client_socket = callbackData->client_socket;

    string message;
    for (int i = 0; i < argc; i++)
    {
        if (argv[i]) {
            if (string(azColName[i]) == "message")
            {
                message += string(argv[i]);
            }
        }
    }
    cout << "message: " << message << endl;

    // Sending the constructed message to the client
    int sent = send(client_socket, message.c_str(), message.length(), 0);
    if (sent == SOCKET_ERROR)
    {
        cerr << "Send error: " << WSAGetLastError() << endl;
    }

    return 0;
}


//Function to get previous messages from the database and calling callback function to send them
void getMessagesForUser(sqlite3* db, const string& user, const string& sec_user, SOCKET client_socket)
{
    string sql = "SELECT sender, receiver, message FROM Messages WHERE " "(sender = '" + user + "' AND receiver = '" + sec_user + "') OR " "(sender = '" + sec_user + "' AND receiver = '" + user + "') " "ORDER BY timestamp;";
    char* errMsg = 0;

    // Prepare the data to be passed to the callback
    CallbackData callbackData = { client_socket, "" };

    //cout << "Executing query: " << sql << endl;
    cout << "Previous messages:" << endl;
    int rc = sqlite3_exec(db, sql.c_str(), callback, &callbackData, &errMsg);
    if (rc != SQLITE_OK) {
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
    }
    else {
        cout << "Previous messages sent to user " << user << endl;
    }
}



//Function to format time
string format_time(const tm& time)
{
    ostringstream oss;
    oss << put_time(&time, "%a %b %d %H:%M:%S %Y");
    return oss.str();
}


//Function to handle Timed Messages

/*
* First it parses the string and gets the required time from it
* Then it gets the current time
* Starts an infinite loop till the time reaches the required time by getting current time after a pause of 1 sec
* When required time to send becomes the same as current time, it sends the message to the other user
* After that the infinite loop breaks
*/
void timed_msg(SOCKET client_socket, vector<dict>& clients, string message, size_t pos, string receiver_name)
{
    string time = message.substr(pos + 6);
    time = time.substr(0, 24);
    cout << "Chosen time: " << time << endl;



    while (true)
    {
        auto now = chrono::system_clock::now();
        time_t currentTime = chrono::system_clock::to_time_t(now);

        tm localTime;
        localtime_s(&localTime, &currentTime);


        string currentTimeStr = format_time(localTime);
        cout << "Current time: " << currentTimeStr << endl;


        if (currentTimeStr == time)
        {
            for (auto client : clients)
            {
                if (client.username == receiver_name)
                {
                    size_t p = message.find(":");
                    message = message.substr(0, p + 1) + message.substr(31 + p + 1);
                    send(client.client, message.c_str(), message.length(), 0);
                }
            }
            break;
        }

        this_thread::sleep_for(chrono::seconds(1));
    }

}


//Function to send and receive messages

/*
* First it receives (if any) messaged sent by any client
* If there is a message that is received, it identifies what type of message it is and works accordngly
* It removes the name of receiver from message from which it identifies whom to send
* Adds the message to the database to store chat history
* Finally it sends the message to receiver
*/
void send_receive(SOCKET client_socket, vector<dict>& clients, sqlite3* db)
{
    bool running = true;
    char buffer[4096];

    while (running)
    {
        int bytesreceived = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytesreceived <= 0)
        {
            cout << "Client disconnected..." << endl;
            break;
        }

        string message(buffer, bytesreceived);
        cout << "message received: " + message << endl;

        if (message.find("LOGIN:") != string::npos)
        {
            cout << "message received for login: " + message << endl;
            int p = message.find(":");
            int p2 = message.find(" ");
            string username = message.substr(p + 1, p2 - p - 1);
            cout << "login attempt by: " << username << endl;
            string password = message.substr(p2 + 1);
            cout << "password entered: " << password << endl;

            handleLogin(db, client_socket, username, password);
            continue;

        }

        //Tells with which user the client is chatting with and sends its history
        if (message.find("chat:") != string::npos)
        {
            int p = message.find(":");
            string sec_user = message.substr(0, p);
            message = message.substr(p + 1);
            p = message.find(":");
            string user = message.substr(0, p - 1);
            getMessagesForUser(db, user, sec_user, client_socket);
            cout << "Previous messages sent to " << user << endl;
            continue;
        }

        //If ":" is not present in the message then it is the username of the client
        if (message.find(":") == string::npos)
        {
            string username = message;
            for (size_t i = 0; i < clients.size(); i++)
                if (clients[i].client == client_socket)
                    clients.erase(clients.begin() + i);
            if (!search_users(clients, username))
            {
                cout << username << endl;
                clients.push_back({ client_socket, username });
            }
            else
            {
                string x = "Invalid Login!";
                send(client_socket, x.c_str(), x.length(), 0);
            }

            continue;
        }


        size_t p = message.find(":");
        string receiver_name = message.substr(0, p);
        message = message.substr(p + 1);
        string sender_name = message.substr(0, message.find(":") - 1);
        cout << "Message received: " << message << endl;

        insertMessage(db, sender_name, receiver_name, message);

        //Checking for Timed Message
        size_t pos = message.find("TIME: ");
        if (!(pos != string::npos))
        {
            for (auto& client : clients)
            {
                if (client.username == receiver_name)
                {
                    send(client.client, message.c_str(), message.length(), 0);
                }
            }
        }
        else
        {
            thread timed_messg(timed_msg, client_socket, ref(clients), message, pos, receiver_name);
            timed_messg.detach();
        }
    }

    auto it = find_if(clients.begin(), clients.end(), [client_socket](const dict& d) { return d.client == client_socket; });
    if (it != clients.end())
    {
        clients.erase(it);
    }
    closesocket(client_socket);
}