# Postgres Database Explorer

A GUI application for exploring and managing PostgreSQL databases. Built with Dear ImGui and OpenGL, this application provides an intuitive interface for viewing, editing, and analyzing database tables.


## Features

- **Database Connection**
  - Simple connection string interface
  - Connection info display (host, user, port, connection time)
  - Secure password input

- **Table Management**
  - Dynamic table listing
  - Real-time table data viewing
  - Column reordering and resizing
  - Multi-page navigation for large datasets

- **Data Interaction**
  - Double-click cell editing
  - Multi-line text support
  - Automatic data refresh
  - Changes persist directly to database

- **Search and Filter**
  - Per-column filtering
  - Case-insensitive search
  - Real-time results
  - Clear filter option

- **Sorting**
  - Sort by any column
  - Ascending/descending toggle
  - Maintains filters while sorting

https://github.com/user-attachments/assets/3b24d806-ca63-4a9b-8640-16edfc8119e8

## Requirements
- OpenGL 3.2+
- GLFW3
- PostgreSQL client library (libpq)
- C++17 compiler
- CMake 3.10+
- Dear ImGui (included as git submodule)



## Building from Source


```bash
git clone https://github.com/nealmick/DB_Explorer.git
cd DB_Explorer
git submodule init
git submodule update

brew install postgresql libpq glfw

mkdir build
cd build

cmake ..

make

./db_explorer
```

## Connection String Format

The application accepts PostgreSQL connection strings in URI format:
```bash
postgresql://username:password@hostname:port/dbname?sslmode=require
```

## Platform Support

Currently tested on:
- macOS Sonoma with Intel CPU.
- Apple Silicon is currently untested.

## Development

The application is structured into three main components:
- `Main.cpp`: Window and OpenGL setup
- `DBE` class: Core application logic and UI management
- `Table` class: Table rendering and data management

## Contributing

Feel free to submit issues, fork the repository, and create pull requests for any improvements.




## Acknowledgments

- Dear ImGui library and contributors
- PostgreSQL community

