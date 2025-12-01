#include "database.h"

// Helper function to handle database errors
void handle_db_error(sqlite3 *db, const char *errMsg) {
  fprintf(stderr, "Database error: %s\n", errMsg);
}

// Initialize the SQLite database connection
int init_db(sqlite3 **db, const char *db_name) {
  int rc = sqlite3_open(db_name, db);
  if (rc) {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(*db));
    return rc;
  }
  return SQLITE_OK;
}

// Create a new user in the database
int create_user(sqlite3 *db, const User *user) {
  const char *sql = "INSERT INTO user (username, password, score, isOnline) VALUES (?, ?, ?, ?)";
  sqlite3_stmt *stmt;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return rc;
  }

  sqlite3_bind_text(stmt, 1, user->username, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, user->password, -1, SQLITE_STATIC);

  int score = (user->score == -1) ? 0 : user->score;
  int is_online = (user->is_online == -1) ? 0 : user->is_online;

  sqlite3_bind_int(stmt, 3, score);
  sqlite3_bind_int(stmt, 4, is_online);

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    handle_db_error(db, sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return rc;
  }

  sqlite3_finalize(stmt);
  return SQLITE_OK;
}

// Read a user from the database by username
int read_user(sqlite3 *db, const char *username, User *user) {
  const char *sql = "SELECT id, username, password, score, isOnline FROM user WHERE username = ?";
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return rc;
  }

  sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    user->id = sqlite3_column_int(stmt, 0);
    const char *db_username = (const char *)sqlite3_column_text(stmt, 1);
    const char *db_password = (const char *)sqlite3_column_text(stmt, 2);
    if (db_username) {
      strncpy(user->username, db_username, sizeof(user->username) - 1);
      user->username[sizeof(user->username) - 1] = '\0';
    } else {
      user->username[0] = '\0';
    }
    if (db_password) {
      strncpy(user->password, db_password, sizeof(user->password) - 1);
      user->password[sizeof(user->password) - 1] = '\0';
    } else {
      user->password[0] = '\0';
    }
    user->score = sqlite3_column_int(stmt, 3);
    user->is_online = sqlite3_column_int(stmt, 4);
  } else if (rc == SQLITE_DONE) {
    fprintf(stderr, "User not found\n");
  } else {
    handle_db_error(db, sqlite3_errmsg(db));
  }

  sqlite3_finalize(stmt);
  return rc;
}

// Update an existing user's data in the database
int update_user(sqlite3 *db, const User *user) {
  const char *sql = "UPDATE user SET password = ?, score = ?, isOnline = ? WHERE id = ?";
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return rc;
  }

  sqlite3_bind_text(stmt, 1, user->password, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, user->score);
  sqlite3_bind_int(stmt, 3, user->is_online);
  sqlite3_bind_int(stmt, 4, user->id);

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    handle_db_error(db, sqlite3_errmsg(db));
  }

  sqlite3_finalize(stmt);
  return rc;
}

int update_user_score(sqlite3 *db, const char *username, int score) {
  const char *sql = "UPDATE user SET score = ? WHERE username = ?";
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return rc;
  }

  sqlite3_bind_int(stmt, 1, score); // Ensure score is properly bound
  sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);


  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    handle_db_error(db, sqlite3_errmsg(db));
  }

  sqlite3_finalize(stmt);
  return rc;
}

// Delete a user from the database
int delete_user(sqlite3 *db, int user_id) {
  const char *sql = "DELETE FROM user WHERE id = ?";
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return rc;
  }

  sqlite3_bind_int(stmt, 1, user_id);

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    handle_db_error(db, sqlite3_errmsg(db));
  }

  sqlite3_finalize(stmt);
  return rc;
}

// Check if a username exists in the database
int user_exists(sqlite3 *db, const char *username) {
  const char *sql = "SELECT COUNT(*) FROM user WHERE username = ?";
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
  rc = sqlite3_step(stmt);

  if (rc == SQLITE_ROW) {
    int count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count > 0;
  } else {
    handle_db_error(db, sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }
}

// Get user details by username
int get_user_by_username(sqlite3 *db, const char *username, User *user) {
  const char *sql = "SELECT id, username, password, score, isOnline FROM user WHERE username = ?";
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return rc;
  }

  sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
  rc = sqlite3_step(stmt);

  if (rc == SQLITE_ROW) {
    user->id = sqlite3_column_int(stmt, 0);
    const char *db_username = (const char *)sqlite3_column_text(stmt, 1);
    const char *db_password = (const char *)sqlite3_column_text(stmt, 2);
    if (db_username) {
      strncpy(user->username, db_username, sizeof(user->username) - 1);
      user->username[sizeof(user->username) - 1] = '\0';
    } else {
      user->username[0] = '\0';
    }
    if (db_password) {
      strncpy(user->password, db_password, sizeof(user->password) - 1);
      user->password[sizeof(user->password) - 1] = '\0';
    } else {
      user->password[0] = '\0';
    }
    user->score = sqlite3_column_int(stmt, 3);
    user->is_online = sqlite3_column_int(stmt, 4);
    sqlite3_finalize(stmt);
    return SQLITE_OK;
  } else if (rc == SQLITE_DONE) {
    fprintf(stderr, "User not found\n");
    sqlite3_finalize(stmt);
    return SQLITE_ERROR;
  } else {
    handle_db_error(db, sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return rc;
  }

}

int authenticate_user(sqlite3 *db, const char *username, const char *password) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT password FROM user WHERE username = ?";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
    return -1;
  }

  sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

  int rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    const char *stored_password = (const char *)sqlite3_column_text(stmt, 0);
    if (strcmp(stored_password, password) == 0) {
      sqlite3_finalize(stmt);
      return 1;
    }
  }

  sqlite3_finalize(stmt);
  return 0;
}

int update_user_online(sqlite3 *db, const char *username) {
  const char *sql = "UPDATE user SET isOnline = 1 WHERE username = ?";
  sqlite3_stmt *stmt;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return rc;
  }

  if (username == NULL || strlen(username) == 0) {
    fprintf(stderr, "Username is NULL or empty.\n");
    sqlite3_finalize(stmt);
    return SQLITE_MISUSE;
  }

  rc = sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind parameter: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return rc;
  }

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    fprintf(stderr, "Failed to update user status: %s\n", sqlite3_errmsg(db));
  }

  sqlite3_finalize(stmt);
  return rc;
}

int update_user_offline(sqlite3 *db, const char *username) {
  const char *sql = "UPDATE user SET isOnline = 0 WHERE username = ?";
  sqlite3_stmt *stmt;

  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return rc;
  }

  if (username == NULL || strlen(username) == 0) {
    fprintf(stderr, "Username is NULL or empty.\n");
    sqlite3_finalize(stmt);
    return SQLITE_MISUSE;
  }

  rc = sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to bind parameter: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return rc;
  }

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    fprintf(stderr, "Failed to update user status: %s\n", sqlite3_errmsg(db));
  }

  sqlite3_finalize(stmt);
  return rc;
}

int list_users_online(sqlite3 *db, User *users, int *user_count) {
  const char *sql = "SELECT id, username, score, isOnline FROM user WHERE isOnline = 1";
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return rc;
  }

  int count = 0;

  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    if (count >= 20) {
      break;
    }

    users[count].id = sqlite3_column_int(stmt, 0);
    strncpy(users[count].username, (const char *)sqlite3_column_text(stmt, 1), sizeof(users[count].username) - 1);
    users[count].score = sqlite3_column_int(stmt, 2);
    users[count].is_online = sqlite3_column_int(stmt, 3);

    count++;
  }

  *user_count = count;

  sqlite3_finalize(stmt);
  return SQLITE_OK;
}

int list_users_closest_score(sqlite3 *db, const char *target_username, User *users, int *user_count) {
  // Retrieve the score of the provided username
  const char *sql_get_score = "SELECT score FROM user WHERE username = ?";
  sqlite3_stmt *stmt;
  int target_score = 0;
  int rc = sqlite3_prepare_v2(db, sql_get_score, -1, &stmt, 0);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement for getting score: %s\n", sqlite3_errmsg(db));
    return rc;
  }

  // Bind the value to the query parameter
  sqlite3_bind_text(stmt, 1, target_username, -1, SQLITE_STATIC);

  // Retrieve the score of the user
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    target_score = sqlite3_column_int(stmt, 0);
  } else {
    sqlite3_finalize(stmt);
    fprintf(stderr, "Username not found or no score available.\n");
    return SQLITE_NOTFOUND;  // Return if the username does not exist
  }
  sqlite3_finalize(stmt);

  // Query the list of 20 users with scores closest to the target score
  const char *sql_get_closest_users =
    "SELECT id, username, score, isOnline "
    "FROM user "
    "WHERE username != ? "
    "ORDER BY ABS(score - ?) ASC LIMIT 20";

  rc = sqlite3_prepare_v2(db, sql_get_closest_users, -1, &stmt, 0);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement for getting closest users: %s\n", sqlite3_errmsg(db));
    return rc;
  }

  // Bind values to the query parameters
  sqlite3_bind_text(stmt, 1, target_username, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, target_score);

  int count = 0;

  // Iterate through the query results
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    if (count >= 20) {
      break;  // Limit to a maximum of 20 users
    }

    users[count].id = sqlite3_column_int(stmt, 0);
    strncpy(users[count].username, (const char *)sqlite3_column_text(stmt, 1), sizeof(users[count].username) - 1);
    users[count].score = sqlite3_column_int(stmt, 2);
    users[count].is_online = sqlite3_column_int(stmt, 3);

    count++;
  }

  *user_count = count;  // Return the number of users found

  sqlite3_finalize(stmt);
  return SQLITE_OK;
}

// Function to save a game history into the database
int save_game_history(sqlite3 *db, GameHistory *game) {
  const char *sql_insert =
    "INSERT INTO game_history (game_id, player1, player2, player1_score, player2_score, winner, word, start_time, end_time) "
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0);
  if (rc != SQLITE_OK) {
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
  sqlite3_bind_text(stmt, 8, game->start_time, -1, SQLITE_STATIC);  // Bind start_time
  sqlite3_bind_text(stmt, 9, game->end_time, -1, SQLITE_STATIC);    // Bind end_time

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
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

  for (int i = 0; i < 12; i++) {
    if (strlen(game->moves[i].guess) == 0) {
      break;  // No more moves to save
    }

    rc = sqlite3_prepare_v2(db, sql_insert_move, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
      printf("Failed to prepare statement for moves: %s\n", sqlite3_errmsg(db));
      return rc;
    }

    sqlite3_bind_text(stmt, 1, game->game_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, i);  // Move index
    sqlite3_bind_text(stmt, 3, game->moves[i].player_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, game->moves[i].guess, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, game->moves[i].result, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
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
int get_game_history_by_player(sqlite3 *db, const char *player_name, GameHistory *response) {
  const char *sql_select =
    "SELECT game_id, player1, player2, player1_score, player2_score, winner, word, start_time, end_time "
    "FROM game_history "
    "WHERE player1 = ? OR player2 = ? "
    "ORDER BY game_id DESC LIMIT 1;";  // Get the most recent game

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql_select, -1, &stmt, 0);
  if (rc != SQLITE_OK) {
    printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return rc;
  }

  sqlite3_bind_text(stmt, 1, player_name, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, player_name, -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    // Populate the GameHistory response
    strncpy(response->game_id, (const char *)sqlite3_column_text(stmt, 0), sizeof(response->game_id) - 1);
    strncpy(response->player1, (const char *)sqlite3_column_text(stmt, 1), sizeof(response->player1) - 1);
    strncpy(response->player2, (const char *)sqlite3_column_text(stmt, 2), sizeof(response->player2) - 1);
    response->player1_score = sqlite3_column_int(stmt, 3);
    response->player2_score = sqlite3_column_int(stmt, 4);
    strncpy(response->winner, (const char *)sqlite3_column_text(stmt, 5), sizeof(response->winner) - 1);
    strncpy(response->word, (const char *)sqlite3_column_text(stmt, 6), sizeof(response->word) - 1);
    strncpy(response->start_time, (const char *)sqlite3_column_text(stmt, 7), sizeof(response->start_time) - 1);  // Get start_time
    strncpy(response->end_time, (const char *)sqlite3_column_text(stmt, 8), sizeof(response->end_time) - 1);      // Get end_time

    sqlite3_finalize(stmt);

    // Get moves for the game
    const char *sql_select_moves =
      "SELECT player_name, guess, result FROM moves "
      "WHERE game_id = ? "
      "ORDER BY move_index ASC;";

    rc = sqlite3_prepare_v2(db, sql_select_moves, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
      printf("Failed to prepare statement for moves: %s\n", sqlite3_errmsg(db));
      return rc;
    }

    sqlite3_bind_text(stmt, 1, response->game_id, -1, SQLITE_STATIC);
    int move_count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && move_count < 12) {
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
  return SQLITE_DONE;  // No game found
}

int get_game_histories_by_player(sqlite3 *db, const char *player_name, GameHistory *history_list, int *history_count) {
  const char *sql_select =
    "SELECT game_id, player1, player2, player1_score, player2_score, winner, word, start_time, end_time "
    "FROM game_history "
    "WHERE player1 = ? OR player2 = ? "
    "ORDER BY start_time DESC;";  // Fetch all matching records

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql_select, -1, &stmt, 0);
  if (rc != SQLITE_OK) {
    printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return rc;
  }

  // Bind the player_name to the SQL query
  sqlite3_bind_text(stmt, 1, player_name, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, player_name, -1, SQLITE_STATIC);

  int count = 0;

  // Iterate through the result set
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    // Dynamically grow the array if needed (caller must handle pre-allocation)
    strncpy(history_list[count].game_id, (const char *)sqlite3_column_text(stmt, 0), sizeof(history_list[count].game_id) - 1);
    strncpy(history_list[count].player1, (const char *)sqlite3_column_text(stmt, 1), sizeof(history_list[count].player1) - 1);
    strncpy(history_list[count].player2, (const char *)sqlite3_column_text(stmt, 2), sizeof(history_list[count].player2) - 1);
    history_list[count].player1_score = sqlite3_column_int(stmt, 3);
    history_list[count].player2_score = sqlite3_column_int(stmt, 4);
    strncpy(history_list[count].winner, (const char *)sqlite3_column_text(stmt, 5), sizeof(history_list[count].winner) - 1);
    strncpy(history_list[count].word, (const char *)sqlite3_column_text(stmt, 6), sizeof(history_list[count].word) - 1);
    strncpy(history_list[count].start_time, (const char *)sqlite3_column_text(stmt, 7), sizeof(history_list[count].start_time) - 1);
    strncpy(history_list[count].end_time, (const char *)sqlite3_column_text(stmt, 8), sizeof(history_list[count].end_time) - 1);

    count++;
  }

  sqlite3_finalize(stmt);

  *history_count = count;  // Return the number of records found

  return SQLITE_OK;
}

int get_game_history_by_id(sqlite3 *db, const char *game_id, GameHistory *game_details) {
  // SQL query to fetch game details by game_id
  const char *sql =
    "SELECT game_id, player1, player2, player1_score, player2_score, winner, word, start_time, end_time "
    "FROM game_history WHERE game_id = ?;";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
  if (rc != SQLITE_OK) {
    printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return rc;
  }

  sqlite3_bind_text(stmt, 1, game_id, -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    // Populate GameHistory struct
    strncpy(game_details->game_id, (const char *)sqlite3_column_text(stmt, 0), sizeof(game_details->game_id) - 1);
    strncpy(game_details->player1, (const char *)sqlite3_column_text(stmt, 1), sizeof(game_details->player1) - 1);
    strncpy(game_details->player2, (const char *)sqlite3_column_text(stmt, 2), sizeof(game_details->player2) - 1);
    game_details->player1_score = sqlite3_column_int(stmt, 3);
    game_details->player2_score = sqlite3_column_int(stmt, 4);
    strncpy(game_details->winner, (const char *)sqlite3_column_text(stmt, 5), sizeof(game_details->winner) - 1);
    strncpy(game_details->word, (const char *)sqlite3_column_text(stmt, 6), sizeof(game_details->word) - 1);
    strncpy(game_details->start_time, (const char *)sqlite3_column_text(stmt, 7), sizeof(game_details->start_time) - 1);
    strncpy(game_details->end_time, (const char *)sqlite3_column_text(stmt, 8), sizeof(game_details->end_time) - 1);

    // Fetch moves (simplified, assuming a moves table exists)
    const char *sql_moves =
      "SELECT player_name, guess, result FROM moves WHERE game_id = ? ORDER BY move_index ASC;";
    sqlite3_stmt *stmt_moves;

    rc = sqlite3_prepare_v2(db, sql_moves, -1, &stmt_moves, 0);
    if (rc != SQLITE_OK) {
      printf("Failed to prepare statement for moves: %s\n", sqlite3_errmsg(db));
      return rc;
    }

    sqlite3_bind_text(stmt_moves, 1, game_id, -1, SQLITE_STATIC);

    int move_count = 0;
    while (sqlite3_step(stmt_moves) == SQLITE_ROW && move_count < 12) {
      strncpy(game_details->moves[move_count].player_name, (const char *)sqlite3_column_text(stmt_moves, 0), sizeof(game_details->moves[move_count].player_name) - 1);
      strncpy(game_details->moves[move_count].guess, (const char *)sqlite3_column_text(stmt_moves, 1), sizeof(game_details->moves[move_count].guess) - 1);
      strncpy(game_details->moves[move_count].result, (const char *)sqlite3_column_text(stmt_moves, 2), sizeof(game_details->moves[move_count].result) - 1);
      move_count++;
    }

    sqlite3_finalize(stmt_moves);
    sqlite3_finalize(stmt);
    return SQLITE_OK;
  }

  sqlite3_finalize(stmt);
  return SQLITE_DONE; // Game not found
}

int get_score_by_username(sqlite3 *db, const char *username, int *score) {
  const char *sql = "SELECT score FROM user WHERE username = ?";
  sqlite3_stmt *stmt;

  // Prepare the SQL query
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    return rc;
  }

  // Bind the value to the query parameter
  sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

  // Execute the query
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    *score = sqlite3_column_int(stmt, 0);  // Retrieve the score from column 0
    rc = SQLITE_OK;
  } else {
    rc = SQLITE_NOTFOUND;  // User not found
  }

  // Clean up
  sqlite3_finalize(stmt);
  return rc;
}
