#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>

#define WORD_LENGTH 5
typedef struct
{
  char player_name[50];
  char guess[WORD_LENGTH + 1];
  char result[WORD_LENGTH + 1];
} PlayTurn;

typedef struct
{
  char game_id[20];
  char player1[50];
  char player2[50];
  int player1_score;
  int player2_score;
  char winner[51];
  char word[WORD_LENGTH + 1];
  PlayTurn moves[12];
  char start_time[20];
  char end_time[20];
} GameHistory;

int save_game_history(sqlite3 *db, GameHistory *game);

int get_game_history_by_player(sqlite3 *db, const char *player_name, GameHistory *response);

int create_table(sqlite3 *db)
{
  const char *sql =
      "CREATE TABLE IF NOT EXISTS user ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "username TEXT NOT NULL, "
      "password TEXT NOT NULL, "
      "score INTEGER NOT NULL, "
      "isOnline INTEGER NOT NULL);";

  char *errMsg = 0;
  int rc = sqlite3_exec(db, sql, 0, 0, &errMsg);

  if (rc != SQLITE_OK)
  {
    printf("SQL error: %s\n", errMsg);
    sqlite3_free(errMsg);
    return rc;
  }

  printf("Table 'user' created successfully or already exists.\n");
  return SQLITE_OK;
}

int insert_sample_data(sqlite3 *db)
{
  const char *sql_insert =
      "INSERT INTO user (username, password, score, isOnline) VALUES "
      "('ShadowHunter', '123', 100, 1), "
      "('BlazeFury', '123', 150, 0), "
      "('MysticKnight', '123', 200, 1), "
      "('LunarBlade', '123', 250, 1), "
      "('ArcaneMage', '123', 300, 0), "
      "('SteelTitan', '123', 350, 1), "
      "('StormRider', '123', 400, 0), "
      "('ViperVenom', '123', 450, 1), "
      "('DragonSoul', '123', 500, 0), "
      "('ThunderStrike', '123', 550, 1), "
      "('PhantomRogue', '123', 600, 1), "
      "('InfernoBlaze', '123', 650, 1), "
      "('CrystalSage', '123', 700, 1), "
      "('ShadowClaw', '123', 750, 1), "
      "('NightHowl', '123', 800, 1), "
      "('FrostFang', '123', 850, 1), "
      "('IronHammer', '123', 900, 1), "
      "('SkyBreaker', '123', 950, 1), "
      "('PhoenixWing', '123', 1000, 1), "
      "('SilentArrow', '123', 1050, 1), "
      "('EclipseShade', '123', 1100, 1), "
      "('SilverWolf', '123', 1150, 1), "
      "('GoldenStreak', '123', 1200, 1), "
      "('FlameHeart', '123', 1250, 1), "
      "('VenomBlade', '123', 1300, 1), "
      "('DuskWarden', '123', 1350, 1), "
      "('FireFury', '123', 1400, 1), "
      "('SpectralGhost', '123', 1450, 1), "
      "('SoulEater', '123', 1500, 1), "
      "('ShadowWraith', '123', 1550, 1), "
      "('OblivionKnight', '123', 1600, 1), "
      "('NebulaHunter', '123', 1650, 1), "
      "('GalacticRider', '123', 1700, 1), "
      "('StarlightSage', '123', 1750, 1), "
      "('SolarFlare', '123', 1800, 1), "
      "('FallenAngel', '123', 1850, 1), "
      "('BlizzardKing', '123', 1900, 1), "
      "('VoidWalker', '123', 1950, 1), "
      "('CrimsonFang', '123', 2000, 1);";

  char *errMsg = 0;
  int rc = sqlite3_exec(db, sql_insert, 0, 0, &errMsg);

  if (rc != SQLITE_OK)
  {
    printf("SQL error: %s\n", errMsg);
    sqlite3_free(errMsg);
    return rc;
  }

  printf("Sample data inserted successfully.\n");
  return SQLITE_OK;
}

int create_game_history_table(sqlite3 *db)
{
  const char *sql_create =
      "CREATE TABLE IF NOT EXISTS game_history ("
      "game_id TEXT PRIMARY KEY, "
      "player1 TEXT NOT NULL, "
      "player2 TEXT NOT NULL, "
      "player1_score INTEGER NOT NULL, "
      "player2_score INTEGER NOT NULL, "
      "winner TEXT NOT NULL, "
      "word TEXT NOT NULL, "
      "start_time TEXT NOT NULL, "
      "end_time TEXT NOT NULL);";

  char *errMsg = 0;
  int rc = sqlite3_exec(db, sql_create, 0, 0, &errMsg); // Create the table

  if (rc != SQLITE_OK)
  {
    printf("SQL error: %s\n", errMsg);
    sqlite3_free(errMsg);
    return rc;
  }

  printf("Table 'game_history' created successfully or already exists.\n");
  return SQLITE_OK;
}

int create_moves_table(sqlite3 *db)
{
  const char *sql_create =
      "CREATE TABLE IF NOT EXISTS moves ("
      "move_id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "game_id TEXT, "
      "move_index INTEGER, "
      "player_name TEXT, "
      "guess TEXT, "
      "result TEXT, "
      "FOREIGN KEY (game_id) REFERENCES game_history(game_id));";

  char *errMsg = 0;
  int rc = sqlite3_exec(db, sql_create, 0, 0, &errMsg); // Create the table
  if (rc != SQLITE_OK)
  {
    printf("SQL error: %s\n", errMsg);
    sqlite3_free(errMsg);
    return rc;
  }

  printf("Table 'moves' created successfully or already exists.\n");
  return SQLITE_OK;
}

int main()
{
  sqlite3 *db;
  int rc = sqlite3_open("../database.db", &db);

  if (rc)
  {
    printf("Can't open database: %s\n", sqlite3_errmsg(db));
    return 0;
  }
  else
  {
    printf("Opened database successfully.\n");
  }

  // Create table if it does not exist
  if (create_table(db) != SQLITE_OK)
  {
    sqlite3_close(db);
    return 1;
  }

  // Insert sample data
  if (insert_sample_data(db) != SQLITE_OK)
  {
    sqlite3_close(db);
    return 1;
  }

  // Create the "game_history" table if it does not exist
  if (create_game_history_table(db) != SQLITE_OK)
  {
    sqlite3_close(db);
    return 1;
  }

  // Create the "moves_table" table if it does not exist
  if (create_moves_table(db) != SQLITE_OK)
  {
    sqlite3_close(db);
    return 1;
  }

  // Example GameHistory data
  GameHistory new_game = {0};
  strcpy(new_game.game_id, "game001");
  strcpy(new_game.player1, "BlazeFury");
  strcpy(new_game.player2, "ThunderStrike");
  new_game.player1_score = 100;
  new_game.player2_score = 90;
  strcpy(new_game.winner, "BlazeFury");
  strcpy(new_game.word, "apple");
  strcpy(new_game.start_time, "2024-12-13 14:30:00"); // Example start time
  strcpy(new_game.end_time, "2024-12-13 14:45:00");   // Example end time

  // Record the moves in the moves array (using PlayTurn)
  strcpy(new_game.moves[0].player_name, "BlazeFury");
  strcpy(new_game.moves[0].guess, "alpha");
  strcpy(new_game.moves[0].result, "XXXXX"); // Example result

  strcpy(new_game.moves[1].player_name, "ThunderStrike");
  strcpy(new_game.moves[1].guess, "bravo");
  strcpy(new_game.moves[1].result, "GGGYY"); // Example result

  // Save the game history and moves
  if (save_game_history(db, &new_game) != SQLITE_OK)
  {
    printf("Failed to save game history and moves\n");
  }

  // Retrieve the game history for a specific player
  GameHistory retrieved_game = {0};
  if (get_game_history_by_player(db, "BlazeFury", &retrieved_game) == SQLITE_OK)
  {
    printf("Game ID: %s\n", retrieved_game.game_id);
    printf("Player1: %s\n", retrieved_game.player1);
    printf("Player2: %s\n", retrieved_game.player2);
    printf("Winner: %s\n", retrieved_game.winner);
    printf("Word: %s\n", retrieved_game.word);
    printf("Start Time: %s\n", retrieved_game.start_time);
    printf("End Time: %s\n", retrieved_game.end_time);

    for (int i = 0; i < 12; i++)
    {
      if (strlen(retrieved_game.moves[i].guess) > 0)
      { // Ensure there's a move at index i
        printf("Move %d by %s: Guess = %s, Result = %s\n", i + 1, retrieved_game.moves[i].player_name, retrieved_game.moves[i].guess, retrieved_game.moves[i].result);
      }
    }
  }
  else
  {
    printf("No game found for player 'BlazeFury'.\n");
  }

  // Close database
  sqlite3_close(db);
  printf("Database connection closed.\n");

  return 0;
}

// Function to save a game history into the database
int save_game_history(sqlite3 *db, GameHistory *game)
{
  const char *sql_insert =
      "INSERT INTO game_history (game_id, player1, player2, player1_score, player2_score, winner, word, start_time, end_time) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0);
  if (rc != SQLITE_OK)
  {
    printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return rc;
  }

  // Bind values to the query
  sqlite3_bind_text(stmt, 1, game->game_id, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, game->player1, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, game->player2, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 4, game->player1_score);
  sqlite3_bind_int(stmt, 5, game->player2_score);
  sqlite3_bind_text(stmt, 6, game->winner, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 7, game->word, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 8, game->start_time, -1, SQLITE_STATIC); // Bind start_time
  sqlite3_bind_text(stmt, 9, game->end_time, -1, SQLITE_STATIC);   // Bind end_time

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE)
  {
    printf("Failed to insert game history: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return rc;
  }

  sqlite3_finalize(stmt);
  printf("Game history saved successfully.\n");

  // Save moves after the game history has been inserted
  const char *sql_insert_move =
      "INSERT INTO moves (game_id, move_index, player_name, guess, result) "
      "VALUES (?, ?, ?, ?, ?);";

  for (int i = 0; i < 12; i++)
  {
    if (strlen(game->moves[i].guess) == 0)
    {
      break; // No more moves to save
    }

    rc = sqlite3_prepare_v2(db, sql_insert_move, -1, &stmt, 0);
    if (rc != SQLITE_OK)
    {
      printf("Failed to prepare statement for moves: %s\n", sqlite3_errmsg(db));
      return rc;
    }

    sqlite3_bind_text(stmt, 1, game->game_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, i); // Move index
    sqlite3_bind_text(stmt, 3, game->moves[i].player_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, game->moves[i].guess, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, game->moves[i].result, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
      printf("Failed to insert move: %s\n", sqlite3_errmsg(db));
      sqlite3_finalize(stmt);
      return rc;
    }

    sqlite3_finalize(stmt);
  }

  printf("Moves saved successfully.\n");
  return SQLITE_OK;
}

// Function to get game history by player name
int get_game_history_by_player(sqlite3 *db, const char *player_name, GameHistory *response)
{
  const char *sql_select =
      "SELECT game_id, player1, player2, player1_score, player2_score, winner, word, start_time, end_time "
      "FROM game_history "
      "WHERE player1 = ? OR player2 = ? "
      "ORDER BY game_id DESC LIMIT 1;"; // Get the most recent game

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql_select, -1, &stmt, 0);
  if (rc != SQLITE_OK)
  {
    printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return rc;
  }

  sqlite3_bind_text(stmt, 1, player_name, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, player_name, -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW)
  {
    // Populate the GameHistory response
    strncpy(response->game_id, (const char *)sqlite3_column_text(stmt, 0), sizeof(response->game_id) - 1);
    strncpy(response->player1, (const char *)sqlite3_column_text(stmt, 1), sizeof(response->player1) - 1);
    strncpy(response->player2, (const char *)sqlite3_column_text(stmt, 2), sizeof(response->player2) - 1);
    response->player1_score = sqlite3_column_int(stmt, 3);
    response->player2_score = sqlite3_column_int(stmt, 4);
    strncpy(response->winner, (const char *)sqlite3_column_text(stmt, 5), sizeof(response->winner) - 1);
    strncpy(response->word, (const char *)sqlite3_column_text(stmt, 6), sizeof(response->word) - 1);
    strncpy(response->start_time, (const char *)sqlite3_column_text(stmt, 7), sizeof(response->start_time) - 1); // Get start_time
    strncpy(response->end_time, (const char *)sqlite3_column_text(stmt, 8), sizeof(response->end_time) - 1);     // Get end_time

    sqlite3_finalize(stmt);

    // Get moves for the game
    const char *sql_select_moves =
        "SELECT player_name, guess, result FROM moves "
        "WHERE game_id = ? "
        "ORDER BY move_index ASC;";

    rc = sqlite3_prepare_v2(db, sql_select_moves, -1, &stmt, 0);
    if (rc != SQLITE_OK)
    {
      printf("Failed to prepare statement for moves: %s\n", sqlite3_errmsg(db));
      return rc;
    }

    sqlite3_bind_text(stmt, 1, response->game_id, -1, SQLITE_STATIC);
    int move_count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && move_count < 12)
    {
      const char *player_name = (const char *)sqlite3_column_text(stmt, 0);
      const char *guess = (const char *)sqlite3_column_text(stmt, 1);
      const char *result = (const char *)sqlite3_column_text(stmt, 2);

      strncpy(response->moves[move_count].player_name, player_name, sizeof(response->moves[move_count].player_name) - 1);
      strncpy(response->moves[move_count].guess, guess, sizeof(response->moves[move_count].guess) - 1);
      strncpy(response->moves[move_count].result, result, sizeof(response->moves[move_count].result) - 1);

      move_count++;
    }

    sqlite3_finalize(stmt);

    return SQLITE_OK;
  }

  sqlite3_finalize(stmt);
  return SQLITE_DONE; // No game found
}
