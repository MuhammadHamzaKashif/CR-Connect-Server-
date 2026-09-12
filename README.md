# CR-Connect Server

The server for CR-Connect, a chat service with timed (self-destructing) messages. Written in C++ for Windows using WinSock and SQLite.

![C++](https://img.shields.io/badge/C++-17-00599C?style=flat-square&logo=cplusplus&logoColor=white)
![WinSock](https://img.shields.io/badge/WinSock2-TCP-0078D4?style=flat-square&logo=windows&logoColor=white)
![SQLite](https://img.shields.io/badge/SQLite-003B57?style=flat-square&logo=sqlite&logoColor=white)

## What it does

- Accepts multiple TCP clients, each handled on its own thread
- Handles login and keeps an in-memory list of connected users
- Persists messages to SQLite and replays history when a user connects
- Supports timed messages that expire after a set duration
- Routes messages between users by username

## Protocol

Messages are sent over raw TCP as plain strings, with the server routing commands in `send_receive` and handling expiry in `timed_msg`. There is no length prefix or message envelope yet: see the notes below before extending it.

## Stack

- C++ on Windows, `ws2_32.lib`
- SQLite (`sqlite3`) for message storage
- Standard library threads for concurrency

## Layout

```
Source.cpp        server, socket loop, SQLite access, message routing
chat.sln          Visual Studio solution
chat.vcxproj      project file
msg.db            SQLite database
db/               database scripts and sample data
```

## Database

The server creates the `Messages` table on startup if it does not exist:

```sql
CREATE TABLE IF NOT EXISTS Messages (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  sender TEXT NOT NULL,
  receiver TEXT NOT NULL,
  message TEXT NOT NULL,
  timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

## Building

Open `chat.sln` in Visual Studio (Desktop development with C++ workload) and build, or compile `Source.cpp` directly. Link `ws2_32.lib` and the SQLite library.

## Notes

- The committed `msg.db`, `try.exe`, and `code.txt` / `code2.txt` files are development leftovers and database state. Consider removing them and gitignoring the database and build output.
- Credentials are compared in the login handler; review how passwords are stored before using this beyond a course project.
- TCP messages have no length prefix. Adding a small framed envelope (length + type) would make parsing robust and let the protocol evolve safely.
