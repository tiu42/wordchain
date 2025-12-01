# wordle-game

This is a project for the network programming practice course.

## Project structure

```sh
├── README.md
└── src
    ├── client
    ├── client.c
    ├── client.o
    ├── database.c
    ├── database.db
    ├── database.h
    ├── database.o
    ├── Makefile
    ├── message.o
    ├── model
    │   ├── message.c
    │   ├── message.h
    │   └── message.o
    ├── seed
    │   ├── run_seed.c
    │   ├── seed
    │   └── seed.c
    ├── server
    ├── server.c
    ├── server.o
    ├── valid_guesses.txt
    ├── valid_solutions.txt
    ├── wordle.glade
```

This project depends on the following libraries and tools:

- **libgtk-3-dev**: Development files for GTK+ 3, used for building the graphical user interface.
- **sqlite3**: Command-line tool for interacting with SQLite databases.
- **libsqlite3-dev**: Development library to link C applications with SQLite.
- **glade**: Tool for designing GTK GUIs in a visual way.

### Installing Dependencies

To install the required libraries, run the following commands:

```bash
sudo apt-get install sqlite3 libsqlite3-dev libgtk-3-dev glade
```

To open an interactive shell for working with the database.db SQLite database:

```bash
sqlite3 database.db
```

To compile C programs that use SQLite by linking the sqlite3 library:
```bash
gcc -o seed seed.c -lsqlite3
```

> When you run the seed program, it will automatically create the database.db file and populate it with initial sample data. Therefore, if you delete the database.db file, you can simply run `./seed` again to regenerate the database with the sample data.

To open glade, run:
```bash
glade wordle.glade
```

To run app, go to `./src` then run:
```bash
make
```

Then, run `./server` and `./client`
