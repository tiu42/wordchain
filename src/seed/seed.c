#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>

#define WORD_LENGTH 5

// --- CẤU TRÚC DỮ LIỆU ---
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
  char word[WORD_LENGTH + 1]; // Lưu từ cuối cùng hoặc lý do thắng
  PlayTurn moves[12];
  char start_time[20];
  char end_time[20];
} GameHistory;

// --- HÀM HỖ TRỢ ---
void handle_db_error(sqlite3 *db, const char *msg)
{
  fprintf(stderr, "Error: %s (%s)\n", msg, sqlite3_errmsg(db));
}

// 1. TẠO LẠI BẢNG (Giữ User, Reset History)
int reset_tables(sqlite3 *db)
{
  // Xóa bảng cũ nếu cần thiết (để reset dữ liệu test)
  sqlite3_exec(db, "DROP TABLE IF EXISTS moves;", 0, 0, 0);
  sqlite3_exec(db, "DROP TABLE IF EXISTS game_history;", 0, 0, 0);
  sqlite3_exec(db, "DROP TABLE IF EXISTS user;", 0, 0, 0);

  // Tạo bảng User (Giữ nguyên cấu trúc cũ)
  const char *sql_user =
      "CREATE TABLE IF NOT EXISTS user ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "username TEXT NOT NULL, "
      "password TEXT NOT NULL, "
      "score INTEGER NOT NULL, "
      "isOnline INTEGER NOT NULL);";

  // Tạo bảng History (Cấu trúc mới)
  const char *sql_hist =
      "CREATE TABLE IF NOT EXISTS game_history ("
      "game_id TEXT PRIMARY KEY, "
      "player1 TEXT NOT NULL, "
      "player2 TEXT NOT NULL, "
      "player1_score INTEGER, "
      "player2_score INTEGER, "
      "winner TEXT, "
      "word TEXT, "
      "start_time TEXT, "
      "end_time TEXT);";

  // Tạo bảng Moves
  const char *sql_moves =
      "CREATE TABLE IF NOT EXISTS moves ("
      "move_id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "game_id TEXT, "
      "move_index INTEGER, "
      "player_name TEXT, "
      "guess TEXT, "
      "result TEXT, "
      "FOREIGN KEY (game_id) REFERENCES game_history(game_id));";

  if (sqlite3_exec(db, sql_user, 0, 0, 0) != SQLITE_OK)
    return 1;
  if (sqlite3_exec(db, sql_hist, 0, 0, 0) != SQLITE_OK)
    return 1;
  if (sqlite3_exec(db, sql_moves, 0, 0, 0) != SQLITE_OK)
    return 1;

  return SQLITE_OK;
}

// 2. CHÈN DỮ LIỆU USER (GIỮ NGUYÊN DANH SÁCH CŨ CỦA BẠN)
int insert_sample_users(sqlite3 *db)
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
  if (sqlite3_exec(db, sql_insert, 0, 0, &errMsg) != SQLITE_OK)
  {
    handle_db_error(db, errMsg);
    sqlite3_free(errMsg);
    return 1;
  }
  printf("Sample users inserted successfully.\n");
  return SQLITE_OK;
}

// 3. HÀM LƯU GAME (Helper)
int save_game_history(sqlite3 *db, GameHistory *game)
{
  const char *sql_insert =
      "INSERT INTO game_history (game_id, player1, player2, player1_score, player2_score, winner, word, start_time, end_time) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";

  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0);

  sqlite3_bind_text(stmt, 1, game->game_id, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, game->player1, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, game->player2, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 4, game->player1_score);
  sqlite3_bind_int(stmt, 5, game->player2_score);
  sqlite3_bind_text(stmt, 6, game->winner, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 7, game->word, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 8, game->start_time, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 9, game->end_time, -1, SQLITE_STATIC);

  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  // Save moves
  const char *sql_insert_move =
      "INSERT INTO moves (game_id, move_index, player_name, guess, result) "
      "VALUES (?, ?, ?, ?, ?);";

  for (int i = 0; i < 12; i++)
  {
    if (strlen(game->moves[i].guess) == 0)
      break;

    sqlite3_prepare_v2(db, sql_insert_move, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, game->game_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, i);
    sqlite3_bind_text(stmt, 3, game->moves[i].player_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, game->moves[i].guess, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, game->moves[i].result, -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }
  return SQLITE_OK;
}

// 4. CHÈN DỮ LIỆU GAME (GAME NỐI TỪ)
int insert_sample_games(sqlite3 *db)
{
  GameHistory g;

  // --- GAME 1: BlazeFury vs ThunderStrike (BlazeFury Thắng) ---
  memset(&g, 0, sizeof(g));
  strcpy(g.game_id, "GAME-SEED-001");
  strcpy(g.player1, "BlazeFury");
  strcpy(g.player2, "ThunderStrike");
  g.player1_score = 30; // 3 từ đúng
  g.player2_score = 20; // 2 từ đúng
  strcpy(g.winner, "BlazeFury");
  strcpy(g.word, "TIMEOUT");
  strcpy(g.start_time, "2024-12-13 14:30:00");
  strcpy(g.end_time, "2024-12-13 14:35:00");

  // Moves: apple -> eagle -> exist -> toast -> train
  strcpy(g.moves[0].player_name, "BlazeFury");
  strcpy(g.moves[0].guess, "apple");
  strcpy(g.moves[0].result, "VALID");
  strcpy(g.moves[1].player_name, "ThunderStrike");
  strcpy(g.moves[1].guess, "eagle");
  strcpy(g.moves[1].result, "VALID");
  strcpy(g.moves[2].player_name, "BlazeFury");
  strcpy(g.moves[2].guess, "exist");
  strcpy(g.moves[2].result, "VALID");
  strcpy(g.moves[3].player_name, "ThunderStrike");
  strcpy(g.moves[3].guess, "toast");
  strcpy(g.moves[3].result, "VALID");
  strcpy(g.moves[4].player_name, "BlazeFury");
  strcpy(g.moves[4].guess, "train");
  strcpy(g.moves[4].result, "VALID");

  save_game_history(db, &g);

  // --- GAME 2: ShadowHunter vs MysticKnight (MysticKnight Thắng) ---
  memset(&g, 0, sizeof(g));
  strcpy(g.game_id, "GAME-SEED-002");
  strcpy(g.player1, "ShadowHunter");
  strcpy(g.player2, "MysticKnight");
  g.player1_score = 10;
  g.player2_score = 50;
  strcpy(g.winner, "MysticKnight");
  strcpy(g.word, "TIMEOUT");
  strcpy(g.start_time, "2024-12-14 10:00:00");
  strcpy(g.end_time, "2024-12-14 10:02:00");

  // Moves: hello -> order -> robot
  strcpy(g.moves[0].player_name, "ShadowHunter");
  strcpy(g.moves[0].guess, "hello");
  strcpy(g.moves[0].result, "VALID");
  strcpy(g.moves[1].player_name, "MysticKnight");
  strcpy(g.moves[1].guess, "order");
  strcpy(g.moves[1].result, "VALID");
  strcpy(g.moves[2].player_name, "ShadowHunter");
  strcpy(g.moves[2].guess, "robot");
  strcpy(g.moves[2].result, "VALID");

  save_game_history(db, &g);

  printf("Sample games inserted successfully.\n");
  return SQLITE_OK;
}

int main()
{
  sqlite3 *db;
  int rc = sqlite3_open("../database.db", &db); // Lưu ý đường dẫn DB

  if (rc)
  {
    printf("Can't open database: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  // Bật Foreign Key
  sqlite3_exec(db, "PRAGMA foreign_keys = ON;", 0, 0, 0);

  // Tạo lại cấu trúc bảng
  if (reset_tables(db) != SQLITE_OK)
  {
    sqlite3_close(db);
    return 1;
  }

  // Chèn dữ liệu
  insert_sample_users(db);
  insert_sample_games(db);

  sqlite3_close(db);
  printf("Seed data completed successfully.\n");
  return 0;
}
