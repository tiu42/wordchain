#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#define USER_TABLE "user"
#define MAX_USERNAME_LEN 50
#define MAX_PASSWORD_LEN 50
// Maximum characters for a word (Vietnamese words may include spaces/diacritics)
#define MAX_WORD_LEN 20
#define MAX_WORDS 15000
#define MAX_ATTEMPTS 100
// Game-specific rules for "nối chữ" (Vietnamese chain-word game)
#define REQUIRED_SYLLABLES 2       // each word must have 2 syllables
#define TURN_TIME_SECONDS 30      // 30 seconds per player's turn
#define MAX_ATTEMPTS_PER_TURN 3   // up to 3 attempts per turn

typedef struct {
  int id;
  char username[MAX_USERNAME_LEN];
  char password[MAX_PASSWORD_LEN];
  int score;
  int is_online;
} User;

typedef struct
{
  char player_name[50];
  // the guessed word (may contain spaces/diacritics)
  char guess[MAX_WORD_LEN + 1];
  // result or feedback text (e.g., "ok", "invalid", "timeout")
  char result[MAX_WORD_LEN + 1];
  int attempt_number;
  int time_taken_seconds;    // time used for this attempt in seconds
  int is_valid;              // 1 = valid move, 0 = invalid
} PlayTurn;

typedef struct
{
  char game_id[20];
  char player1_name[50];
  char player2_name[50];
  int current_player; // 1 or 2
  int player1_attempts;
  int player2_attempts;
  int player1_score;
  int player2_score;
  // quick reference to the last syllable used (for validation)
  char last_syllable[10];
  int game_active;
  int current_attempts; // attempts in current turn
  // rule settings (stored per session so rules can vary in future)
  int turn_time_limit;       // seconds (default TURN_TIME_SECONDS)
  int max_attempts_per_turn; // default MAX_ATTEMPTS_PER_TURN
  int required_syllables;    // default REQUIRED_SYLLABLES
  time_t turn_start_time;    // timestamp when current turn started
  char start_time [20];
  char end_time [20];
  PlayTurn turns[MAX_ATTEMPTS];
} GameSession;

typedef struct {
  char game_id[20];
  char player1[50];
  char player2[50];
  int player1_score;
  int player2_score;
  char winner[51];
  char word[MAX_WORD_LEN + 1];
  PlayTurn moves[12];
  char start_time[20];
  char end_time[20];
} GameHistory;

typedef struct
{
  char player_name[50]; // Player's name
  int player_sock;      // Player's socket
} PlayerInfo;

// Game state structure
typedef struct
{
  char target_word[MAX_WORD_LEN + 1];
  int attempts_left;
  int game_won;
} GameState;

int init_db(sqlite3 **db, const char *db_name);

int create_user(sqlite3 *db, const User *user);

int read_user(sqlite3 *db, const char *username, User *user);

int update_user(sqlite3 *db, const User *user);

int delete_user(sqlite3 *db, int user_id);

int user_exists(sqlite3 *db, const char *username);

int get_user_by_username(sqlite3 *db, const char *username, User *user);

void handle_db_error(sqlite3 *db, const char *errMsg);

int authenticate_user(sqlite3 *db, const char *username, const char *password);

int update_user_online(sqlite3 *db, const char *username);

int update_user_offline(sqlite3 *db, const char *username);

int update_user_score(sqlite3 *db, const char *username, int score);

int list_users_online(sqlite3 *db, User *users, int *user_count);

int list_users_closest_score(sqlite3 *db, const char *target_username, User *users, int *user_count);

int save_game_history(sqlite3 *db, GameHistory *game);

int get_game_history_by_player(sqlite3 *db, const char *player_name, GameHistory *response);

int get_game_histories_by_player(sqlite3 *db, const char *player_name, GameHistory *history_list, int *history_count);

int get_game_history_by_id(sqlite3 *db, const char *game_id, GameHistory *game_details);

int get_score_by_username(sqlite3 *db, const char *username, int *score);

#endif
