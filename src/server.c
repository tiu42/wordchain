#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>
#include <sqlite3.h>
#include <time.h>
#include "database.h"
#include "./model/message.h"

#define PORT 8080
#define MAX_CLIENTS 30
#define BUFFER_SIZE 1024
#define MAX_PLAYERS 100
#define MAX_SESSIONS 15
#define DB_FILE "database.db"

volatile sig_atomic_t got_signal = 0;
sqlite3 *db;

GameSession game_sessions[MAX_SESSIONS];

PlayerInfo player_list[MAX_PLAYERS]; // Array to store player information
int player_count = 0;                // Current number of players

// --- SỬA ĐỔI: CHỈ DÙNG 1 DANH SÁCH TỪ ---
char valid_words[MAX_WORDS][WORD_LENGTH + 1];
int word_count = 0;

/****************************Word Function*******************************/

// Hàm xáo trộn (dùng cho gợi ý nếu cần)
void scramble_string(char *str)
{
  int n = strlen(str);
  for (int i = n - 1; i > 0; i--)
  {
    int j = rand() % (i + 1);
    char temp = str[i];
    str[i] = str[j];
    str[j] = temp;
  }
}

// Lấy từ ngẫu nhiên từ danh sách duy nhất
char *get_random_word()
{
  if (word_count == 0)
    return "apple"; // Fallback nếu chưa load
  int index = rand() % word_count;
  return valid_words[index];
}

void check_guess(const char *guess, const char *target, char *result)
{
  if (strcmp(guess, target) == 0)
  {
    strcpy(result, "CORRECT");
  }
  else
  {
    strcpy(result, "WRONG");
  }
}

// Hàm load từ chung
int load_words(const char *filename, char words[][WORD_LENGTH + 1])
{
  FILE *file = fopen(filename, "r");
  if (file == NULL)
  {
    perror("Error opening file");
    return -1;
  }
  int count = 0;
  while (fscanf(file, "%5s", words[count]) == 1)
  {
    count++;
    if (count >= MAX_WORDS)
      break;
  }
  fclose(file);
  return count;
}

// SỬA ĐỔI: Chỉ load valid_words.txt
void init_wordle()
{
  srand(time(NULL));

  word_count = load_words("valid_words.txt", valid_words);
  printf("Loaded %d words from valid_words.txt\n", word_count);

  if (word_count == -1 || word_count == 0)
  {
    printf("Failed to load word list or list is empty.\n");
    exit(1);
  }
}

// SỬA ĐỔI: Chỉ kiểm tra trong valid_words
int is_valid_guess(const char *guess)
{
  for (int i = 0; i < word_count; i++)
  {
    if (strcmp(guess, valid_words[i]) == 0)
    {
      return 1; // Hợp lệ
    }
  }
  return 0; // Không tìm thấy
}
/***************************************************************************/

/****************************Database Function*******************************/
int open_database()
{
  int rc = sqlite3_open(DB_FILE, &db);
  if (rc)
  {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    return 1;
  }
  printf("Database opened successfully\n");
  return 0;
}

void close_database()
{
  sqlite3_close(db);
  printf("Database closed successfully\n");
}
/***************************************************************************/

/*****************************Utils Function***********************************/
void get_time_as_string(char *buffer, size_t buffer_size)
{
  time_t raw_time;
  struct tm *time_info;
  time(&raw_time);
  time_info = localtime(&raw_time);
  strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", time_info);
}

void generate_game_id(char *game_id, size_t size)
{
  time_t now = time(NULL);
  snprintf(game_id, size, "GAME-%ld", now);
}

int add_player(const char *player_name, int player_sock)
{
  if (player_count >= MAX_PLAYERS)
  {
    printf("Player list is full!\n");
    return -1;
  }
  for (int i = 0; i < player_count; i++)
  {
    if (strcmp(player_list[i].player_name, player_name) == 0)
    {
      printf("Player %s already exists!\n", player_name);
      return -1;
    }
  }
  strcpy(player_list[player_count].player_name, player_name);
  player_list[player_count].player_sock = player_sock;
  player_count++;
  return 0;
}

int get_player_sock(const char *player_name)
{
  for (int i = 0; i < player_count; i++)
  {
    if (strcmp(player_list[i].player_name, player_name) == 0)
    {
      return player_list[i].player_sock;
    }
  }
  printf("Player %s not found!\n", player_name);
  return -1;
}

int create_game_session(const char *player1_name, const char *player2_name)
{
  for (int i = 0; i < MAX_SESSIONS; i++)
  {
    if (!game_sessions[i].game_active)
    {
      generate_game_id(game_sessions[i].game_id, sizeof(game_sessions[i].game_id));
      strcpy(game_sessions[i].player1_name, player1_name);
      strcpy(game_sessions[i].player2_name, player2_name);

      // --- LOGIC NỐI TỪ ---
      game_sessions[i].current_player = 1;
      memset(game_sessions[i].last_word, 0, sizeof(game_sessions[i].last_word));

      // Đặt bằng 0 để đánh dấu là lượt đầu tiên chưa tính giờ
      game_sessions[i].last_move_time = 0;

      game_sessions[i].player1_score = 0;
      game_sessions[i].player2_score = 0;

      game_sessions[i].game_active = 1;
      game_sessions[i].current_attempts = 0;
      get_time_as_string(game_sessions[i].start_time, sizeof(game_sessions[i].start_time));
      return i;
    }
  }
  return -1;
}

void clear_game_session(int session_id)
{
  // Xóa sạch session
  memset(&game_sessions[session_id], 0, sizeof(GameSession));
  printf("Cleared game session %d\n", session_id);
}

int find_existing_game(const char *player1_name, const char *player2_name)
{
  for (int i = 0; i < MAX_SESSIONS; i++)
  {
    if (game_sessions[i].game_active)
    {
      if ((strcmp(game_sessions[i].player1_name, player1_name) == 0 &&
           strcmp(game_sessions[i].player2_name, player2_name) == 0) ||
          (strcmp(game_sessions[i].player1_name, player2_name) == 0 &&
           strcmp(game_sessions[i].player2_name, player1_name) == 0))
      {
        return i;
      }
    }
  }
  return -1;
}

User *find_user_by_username(User users[], int size, const char *username)
{
  for (int i = 0; i < size; i++)
  {
    if (strncmp(users[i].username, username, 50) == 0)
    {
      return &users[i];
    }
  }
  return NULL;
}

void send_score_update(GameSession *session)
{
  Message message;
  message.message_type = GAME_SCORE;
  sprintf(message.payload, "%s|%d|%s|%d", session->player1_name, session->player1_score, session->player2_name, session->player2_score);
  message.status = SUCCESS;
  printf("Sending score update to %s and %s\n", session->player1_name, session->player2_name);
  send(get_player_sock(session->player1_name), &message, sizeof(Message), 0);
  send(get_player_sock(session->player2_name), &message, sizeof(Message), 0);
}

void handle_client_disconnect(int client_sock)
{
  char disconnected_player[50];
  int player_index = -1;

  for (int i = 0; i < player_count; i++)
  {
    if (player_list[i].player_sock == client_sock)
    {
      strcpy(disconnected_player, player_list[i].player_name);
      player_index = i;
      break;
    }
  }

  if (player_index == -1)
  {
    printf("Disconnected player not found\n");
    return;
  }

  for (int i = player_index; i < player_count - 1; i++)
  {
    player_list[i] = player_list[i + 1];
  }
  player_count--;

  for (int i = 0; i < MAX_SESSIONS; i++)
  {
    GameSession *session = &game_sessions[i];
    if (session->game_active &&
        (strcmp(session->player1_name, disconnected_player) == 0 ||
         strcmp(session->player2_name, disconnected_player) == 0))
    {
      const char *opponent = strcmp(session->player1_name, disconnected_player) == 0 ? session->player2_name : session->player1_name;
      int opponent_sock = get_player_sock(opponent);

      Message message;
      message.message_type = GAME_END;
      message.status = SUCCESS;
      sprintf(message.payload, "%s", disconnected_player); // Thông báo đối thủ out
      send(opponent_sock, &message, sizeof(Message), 0);

      // Lưu lịch sử (đối thủ out thì người còn lại thắng)
      GameHistory game_history;
      strcpy(game_history.game_id, session->game_id);
      strcpy(game_history.player1, session->player1_name);
      strcpy(game_history.player2, session->player2_name);
      strcpy(game_history.word, session->last_word);
      game_history.player1_score = session->player1_score;
      game_history.player2_score = session->player2_score;

      if (strcmp(disconnected_player, session->player1_name) == 0)
        strcpy(game_history.winner, session->player2_name);
      else
        strcpy(game_history.winner, session->player1_name);

      strcpy(game_history.start_time, session->start_time);
      strcpy(game_history.end_time, session->end_time);

      for (int j = 0; j < MAX_ATTEMPTS; j++)
      {
        if (strlen(session->turns[j].guess) == 0)
          break;
        strcpy(game_history.moves[j].player_name, session->turns[j].player_name);
        strcpy(game_history.moves[j].guess, session->turns[j].guess);
        strcpy(game_history.moves[j].result, session->turns[j].result);
      }

      save_game_history(db, &game_history);
      clear_game_session(i);
      break;
    }
  }

  printf("Player %s disconnected\n", disconnected_player);
  close(client_sock);
}
/***************************************************************************/

/*****************************INIT SERVER*************************************/

void signal_handler(int sig)
{
  got_signal = 1;
  printf("Caught signal %d\n", sig);
}

void setup_signal_handler()
{
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
}

int initialize_server(int *server_sock, struct sockaddr_in *server_addr)
{
  if ((*server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0)
  {
    perror("Socket failed");
    exit(EXIT_FAILURE);
  }

  server_addr->sin_family = AF_INET;
  server_addr->sin_addr.s_addr = INADDR_ANY;
  server_addr->sin_port = htons(PORT);

  if (bind(*server_sock, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)
  {
    perror("Bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(*server_sock, 30) < 0)
  {
    perror("Listen failed");
    exit(EXIT_FAILURE);
  }

  printf("Server listening on port %d\n", PORT);
  return 0;
}

/***************************************************************************/

void handle_message(int client_sock, Message *message);

int main()
{
  int server_sock, new_sock, client_socks[MAX_CLIENTS] = {0};
  struct sockaddr_in server_addr, client_addr;
  fd_set readfds;
  socklen_t addr_len = sizeof(client_addr);
  sigset_t block_mask, orig_mask;

  int rc = open_database();
  if (rc)
    return 1;

  init_wordle();
  setup_signal_handler();

  sigemptyset(&block_mask);
  sigaddset(&block_mask, SIGINT);
  sigprocmask(SIG_BLOCK, &block_mask, &orig_mask);

  initialize_server(&server_sock, &server_addr);

  while (1)
  {
    FD_ZERO(&readfds);
    FD_SET(server_sock, &readfds);
    int max_sd = server_sock;

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
      int sock = client_socks[i];
      if (sock > 0)
        FD_SET(sock, &readfds);
      if (sock > max_sd)
        max_sd = sock;
    }

    int ready = pselect(max_sd + 1, &readfds, NULL, NULL, NULL, &orig_mask);
    if (ready == -1)
    {
      if (errno == EINTR)
      {
        if (got_signal)
          break;
        continue;
      }
      else
      {
        perror("pselect");
        exit(EXIT_FAILURE);
      }
    }

    if (FD_ISSET(server_sock, &readfds))
    {
      if ((new_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len)) < 0)
      {
        perror("Accept failed");
        continue;
      }
      for (int i = 0; i < MAX_CLIENTS; i++)
      {
        if (client_socks[i] == 0)
        {
          client_socks[i] = new_sock;
          break;
        }
      }
    }

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
      int sock = client_socks[i];
      if (FD_ISSET(sock, &readfds))
      {
        Message message;
        int read_size = recv(sock, &message, sizeof(Message), 0);
        if (read_size == 0)
        {
          handle_client_disconnect(sock);
          close(sock);
          client_socks[i] = 0;
        }
        else
        {
          handle_message(sock, &message);
        }
      }
    }

    if (got_signal)
      break;
  }

  close_database();
  close(server_sock);
  printf("Server stopped.\n");
  return 0;
}

void handle_message(int client_sock, Message *message)
{
  switch (message->message_type)
  {
  case SIGNUP_REQUEST:
  {
    char username[50], password[50];
    sscanf(message->payload, "%49[^|]|%49s", username, password);

    if (username == NULL || strlen(username) == 0 || password == NULL || strlen(password) == 0)
    {
      message->status = BAD_REQUEST;
      strcpy(message->payload, "Username or password is missing.");
    }
    else if (user_exists(db, username))
    {
      message->status = BAD_REQUEST;
      strcpy(message->payload, "Username already exists.");
    }
    else
    {
      User new_user;
      strncpy(new_user.username, username, sizeof(new_user.username) - 1);
      new_user.username[sizeof(new_user.username) - 1] = '\0';
      strncpy(new_user.password, password, sizeof(new_user.password) - 1);
      new_user.password[sizeof(new_user.password) - 1] = '\0';
      new_user.score = 0;
      new_user.is_online = 0;

      int rc = create_user(db, &new_user);
      if (rc == SQLITE_OK)
      {
        message->status = SUCCESS;
        strcpy(message->payload, "User registered successfully.");
      }
      else
      {
        message->status = INTERNAL_SERVER_ERROR;
        strcpy(message->payload, "Error occurred while registering user.");
      }
    }
    send(client_sock, message, sizeof(Message), 0);
    break;
  }

  case LOGIN_REQUEST:
  {
    char username[50], password[50];
    sscanf(message->payload, "%49[^|]|%49s", username, password);

    int login_status = authenticate_user(db, username, password);

    if (login_status == 1)
    {
      int update_status = update_user_online(db, username);
      if (update_status != SQLITE_DONE)
      {
        message->status = INTERNAL_SERVER_ERROR;
        strcpy(message->payload, "Failed to update user status");
      }
      else
      {
        message->status = SUCCESS;
        strcpy(message->payload, "Login successful");
        if (add_player(username, client_sock) == 0)
        {
          printf("Player %s connected with socket %d\n", username, client_sock);
        }
        else
        {
          printf("Failed to add player %s\n", username);
          close(client_sock);
        }
      }
    }
    else if (login_status == 0)
    {
      message->status = UNAUTHORIZED;
      strcpy(message->payload, "Invalid username or password");
    }
    else
    {
      message->status = INTERNAL_SERVER_ERROR;
      strcpy(message->payload, "Login failed");
    }
    send(client_sock, message, sizeof(Message), 0);
    break;
  }
  case LOGOUT_REQUEST:
  {
    char username[50], password[50];
    sscanf(message->payload, "%49[^|]|%49s", username, password);
    printf("Logout request from user: %s\n", username);
    int auth_status = authenticate_user(db, username, password);
    if (auth_status == 1)
    {
      int update_status = update_user_offline(db, username);
      if (update_status == SQLITE_DONE || update_status == SQLITE_OK)
      {
        message->status = SUCCESS;
        strcpy(message->payload, "Logout successful");
        // Clear PlayerInfo
        for (int i = 0; i < MAX_PLAYERS; i++)
        {
          if (strcmp(player_list[i].player_name, username) == 0)
          {
            player_list[i].player_sock = -1;                                           // Clear the player's socket
            memset(player_list[i].player_name, 0, sizeof(player_list[i].player_name)); // Clear the player's name
            break;
          }
        }
      }
      else
      {
        message->status = INTERNAL_SERVER_ERROR;
        strcpy(message->payload, "Failed to update user status");
      }
    }
    else if (auth_status == 0)
    {
      message->status = UNAUTHORIZED;
      strcpy(message->payload, "Invalid username or password");
    }
    else
    {
      message->status = INTERNAL_SERVER_ERROR;
      strcpy(message->payload, "Logout failed");
    }
    send(client_sock, message, sizeof(Message), 0);
    break;
  }
  case GET_SCORE_BY_USER_REQUEST:
  {
    char client_name[50] = {0};
    sscanf(message->payload, "%s", client_name);

    int score = 0;
    int rc = get_score_by_username(db, client_name, &score);

    if (rc == SQLITE_OK)
    {
      message->status = SUCCESS;
      snprintf(message->payload, sizeof(message->payload), "%d", score);
    }
    else if (rc == SQLITE_NOTFOUND)
    {
      message->status = NOT_FOUND;
      strcpy(message->payload, "User not found");
    }
    else
    {
      message->status = INTERNAL_SERVER_ERROR;
      strcpy(message->payload, "Error retrieving score");
    }

    send(client_sock, message, sizeof(Message), 0);
    break;
  }
  case LIST_USER:
  {
    char username[50];
    sscanf(message->payload, "%s", username);

    User users[20];
    int user_count = 0;

    int rc = list_users_closest_score(db, username, users, &user_count);
    if (rc == SQLITE_OK)
    {
      char response[2048] = {0};
      char buffer[128];

      for (int i = 0; i < user_count; i++)
      {
        snprintf(buffer, sizeof(buffer), "ID: %d, Username: %s, Score: %d, Online: %d\n",
                 users[i].id, users[i].username, users[i].score, users[i].is_online);
        strncat(response, buffer, 2048 - strlen(response) - 1);
      }

      if (strlen(response) > 1)
      {
        response[strlen(response) - 1] = '\0';
      }
      strcpy(message->payload, response);
      message->status = SUCCESS;
    }
    else if (rc == NOT_FOUND)
    {
      message->status = NOT_FOUND;
      strcpy(message->payload, "No online users found.");
    }
    else
    {
      message->status = INTERNAL_SERVER_ERROR;
      strcpy(message->payload, "Internal server error occurred.");
    }

    send(client_sock, message, sizeof(Message), 0);
    break;
  }
  case CHALLANGE_REQUEST:
  {
    char player1[50], player2[50];
    sscanf(message->payload, "CHALLANGE_REQUEST|%49[^|]|%49s", player1, player2);
    int player1_sock = get_player_sock(player1);
    int player2_sock = get_player_sock(player2);
    // Check if players are online
    if (player1_sock == -1 || player2_sock == -1)
    {
      message->status = BAD_REQUEST;
      strcpy(message->payload, "Player not found");
      send(client_sock, message, sizeof(Message), 0);
      return;
    }
    // Check if players are already in a game
    for (int i = 0; i < MAX_SESSIONS; i++)
    {
      GameSession *session = &game_sessions[i];
      if (session->game_active &&
          (strcmp(session->player1_name, player1) == 0 || strcmp(session->player2_name, player1) == 0 ||
           strcmp(session->player1_name, player2) == 0 || strcmp(session->player2_name, player2) == 0))
      {
        message->status = BAD_REQUEST;
        strcpy(message->payload, "One or both players are already in a game");
        send(client_sock, message, sizeof(Message), 0);
        return;
      }
    }
    message->status = SUCCESS;
    send(player1_sock, message, sizeof(Message), 0);
    send(player2_sock, message, sizeof(Message), 0);
    break;
  }
  case CHALLANGE_RESPONSE:
  {
    char player1[50], player2[50], response[50];
    sscanf(message->payload, "CHALLANGE_RESPONSE|%49[^|]|%49[^|]|%49s", player1, player2, response);
    int player1_sock = get_player_sock(player1);
    int player2_sock = get_player_sock(player2);
    if (player1_sock == -1 || player2_sock == -1)
    {
      message->status = BAD_REQUEST;
      strcpy(message->payload, "Player not found");
      send(client_sock, message, sizeof(Message), 0);
      return;
    }
    if (strcmp(response, "ACCEPT") == 0)
    {
      message->status = SUCCESS;
      send(player1_sock, message, sizeof(Message), 0);

      send(player2_sock, message, sizeof(Message), 0);
    }
    else
    {
      message->status = BAD_REQUEST;
      strcpy(message->payload, "Challange rejected");
      send(player1_sock, message, sizeof(Message), 0);
    }
    break;
  }
  case GAME_START:
  {
    printf("Received game request\n");

    char player1_name[50], player2_name[50];
    sscanf(message->payload, "%49[^|]|%49s", player1_name, player2_name); // Lấy tên của cả 2 người chơi

    // Kiểm tra nếu người chơi là Player 1
    if (client_sock == get_player_sock(player1_name))
    {
      int session_id = find_existing_game(player1_name, player2_name);
      if (session_id != -1)
      {
        printf("Game session found with ID %d between %s and %s\n", session_id, player1_name, player2_name);
        message->status = SUCCESS;
        GameSession *session = &game_sessions[session_id];
        int player_num = (strcmp(player1_name, session->player1_name) == 0) ? 1 : 2;
        sprintf(message->payload, "%d|%d", session_id, player_num);
      }
      else
      {
        printf("Creating game session between %s and %s\n", player1_name, player2_name);
        session_id = create_game_session(player1_name, player2_name);
        if (session_id != -1)
        {
          message->status = SUCCESS;
          GameSession *session = &game_sessions[session_id];
          int player_num = (strcmp(player1_name, session->player1_name) == 0) ? 1 : 2;
          sprintf(message->payload, "%d|%d", session_id, player_num);
        }
        else
        {
          message->status = INTERNAL_SERVER_ERROR;
          strcpy(message->payload, "Failed to create game session");
        }
      }
    }
    else if (client_sock == get_player_sock(player2_name))
    {
      int session_id = find_existing_game(player1_name, player2_name);
      printf("Game session found with ID %d between %s and %s\n", session_id, player1_name, player2_name);
      if (session_id != -1)
      {
        message->status = SUCCESS;
        sprintf(message->payload, "%d", session_id);
      }
      else
      {
        message->status = BAD_REQUEST;
        strcpy(message->payload, "Game not found");
      }
    }
    // Gửi phản hồi cho cả hai người chơi
    send(client_sock, message, sizeof(Message), 0);
    break;
  }
  case GAME_GET_TARGET:
  {
    int session_id;
    sscanf(message->payload, "%d", &session_id);

    if (session_id >= 0 && session_id < MAX_SESSIONS && game_sessions[session_id].game_active)
    {
      // --- SỬA ĐỔI: Tạo từ xáo trộn ---
      char hint_word[WORD_LENGTH + 1];
      strcpy(hint_word, game_sessions[session_id].last_word);
      scramble_string(hint_word); // Xáo trộn

      // Gửi từ xáo trộn cho Client
      strcpy(message->payload, hint_word);
      message->status = SUCCESS;
    }
    else
    {
      strcpy(message->payload, "Invalid session");
      message->status = INTERNAL_SERVER_ERROR;
    }
    send(client_sock, message, sizeof(Message), 0);
    break;
  }
  case GAME_GUESS:
  {
    int session_id;
    char guess[WORD_LENGTH + 1];
    char player_name[50];
    sscanf(message->payload, "%d|%49[^|]|%49s", &session_id, player_name, guess);

    GameSession *session = &game_sessions[session_id];
    int player_num = (strcmp(player_name, session->player1_name) == 0) ? 1 : 2;

    // 1. Kiểm tra lượt
    if (player_num != session->current_player)
    {
      strcpy(message->payload, "Not your turn");
      message->status = BAD_REQUEST;
      send(client_sock, message, sizeof(Message), 0);
      return;
    }

    // 2. Kiểm tra thời gian (Timeout)
    if (session->current_attempts > 0)
    {
      time_t now = time(NULL);
      if (difftime(now, session->last_move_time) > 12.0)
      {
        message->status = SUCCESS;
        sprintf(message->payload, "TIMEOUT_LOSE|%s", player_name);
        int s1 = get_player_sock(session->player1_name);
        int s2 = get_player_sock(session->player2_name);
        if (s1 != -1)
          send(s1, message, sizeof(Message), 0);
        if (s2 != -1)
          send(s2, message, sizeof(Message), 0);
        clear_game_session(session_id);
        return;
      }
    }

    // 3. KIỂM TRA TỪ CÓ TRONG TỪ ĐIỂN KHÔNG (QUAN TRỌNG)
    if (!is_valid_guess(guess))
    {
      strcpy(message->payload, "Invalid word (Not in dictionary)!");
      message->status = BAD_REQUEST;
      send(client_sock, message, sizeof(Message), 0);
      return; // Dừng ngay nếu từ không hợp lệ
    }

    // 4. Kiểm tra từ đã dùng chưa (Duplicate)
    int is_duplicate = 0;
    for (int i = 0; i < session->current_attempts; i++)
    {
      if (strcmp(session->turns[i].guess, guess) == 0)
      {
        is_duplicate = 1;
        break;
      }
    }
    if (is_duplicate)
    {
      strcpy(message->payload, "Word already used!");
      message->status = BAD_REQUEST;
      send(client_sock, message, sizeof(Message), 0);
      return;
    }

    // 5. Kiểm tra luật nối từ (Ký tự đầu trùng ký tự cuối)
    if (session->current_attempts > 0)
    {
      char required_char = session->last_word[WORD_LENGTH - 1];
      if (guess[0] != required_char)
      {
        char err_msg[100];
        sprintf(err_msg, "Word must start with '%c'", required_char);
        strcpy(message->payload, err_msg);
        message->status = BAD_REQUEST;
        send(client_sock, message, sizeof(Message), 0);
        return;
      }
    }

    // --- HỢP LỆ ---
    strcpy(session->last_word, guess);
    session->last_move_time = time(NULL);

    if (player_num == 1)
      session->player1_score += 10;
    else
      session->player2_score += 10;

    session->current_player = (player_num == 1) ? 2 : 1;

    strcpy(session->turns[session->current_attempts].player_name, player_name);
    strcpy(session->turns[session->current_attempts].guess, guess);
    strcpy(session->turns[session->current_attempts].result, "VALID");
    session->current_attempts++;

    sprintf(message->payload, "CONTINUE|%d|%s|%d|%d",
            session->current_player, session->last_word,
            session->player1_score, session->player2_score);
    message->status = SUCCESS;

    int s1 = get_player_sock(session->player1_name);
    int s2 = get_player_sock(session->player2_name);
    if (s1 != -1)
      send(s1, message, sizeof(Message), 0);
    if (s2 != -1)
      send(s2, message, sizeof(Message), 0);

    break;
  }
  case GAME_UPDATE:
  {
    printf("Received game update\n");
    printf("Payload: %s\n", message->payload);
    int session_id;
    char player_name[50];
    sscanf(message->payload, "%d|%49s", &session_id, player_name);
    break;
  }
  case LIST_GAME_HISTORY:
  {
    char client_name[50] = {0};
    sscanf(message->payload, "%49s", client_name);

    GameHistory history_list[10];
    int history_count = 0;

    int rc = get_game_histories_by_player(db, client_name, history_list, &history_count);

    if (rc == SQLITE_OK)
    {
      char response[2048] = {0};
      char buffer[256];

      for (int i = 0; i < history_count; i++)
      {
        // --- SỬA ĐỔI FORMAT GỬI VỀ ---
        // Format mới: GameID|Player1|Player2|Winner
        snprintf(buffer, sizeof(buffer),
                 "%s|%s|%s|%s\n",
                 history_list[i].game_id,
                 history_list[i].player1,
                 history_list[i].player2,
                 history_list[i].winner);

        strncat(response, buffer, sizeof(response) - strlen(response) - 1);
      }

      if (strlen(response) > 0)
      {
        response[strlen(response) - 1] = '\0'; // Remove trailing newline
      }
      else
      {
        strcpy(response, "No history found");
      }

      strcpy(message->payload, response);
      message->status = SUCCESS;
    }
    else if (rc == NOT_FOUND)
    {
      message->status = NOT_FOUND;
      strcpy(message->payload, "No game history found.");
    }
    else
    {
      message->status = INTERNAL_SERVER_ERROR;
      strcpy(message->payload, "Internal server error occurred.");
    }

    send(client_sock, message, sizeof(Message), 0);
    break;
  }
  case GAME_DETAIL_REQUEST:
  {
    char game_id[20] = {0};
    sscanf(message->payload, "%19s", game_id);

    GameHistory game_details;
    if (get_game_history_by_id(db, game_id, &game_details) != SQLITE_OK)
    {
      message->status = NOT_FOUND;
      snprintf(message->payload, sizeof(message->payload), "Game ID %s not found.", game_id);
    }
    else
    {
      // Serialize game details into the payload
      char response[2048] = {0};
      snprintf(response, sizeof(response),
               "%s|%s|%s|%d|%d|%s|%s|%s|%s\nMoves:\n",
               game_details.game_id, game_details.player1, game_details.player2,
               game_details.player1_score, game_details.player2_score,
               game_details.winner, game_details.word,
               game_details.start_time, game_details.end_time);

      for (int i = 0; i < 12; i++)
      {
        if (strlen(game_details.moves[i].guess) == 0)
          break;
        char move[256];
        snprintf(move, sizeof(move), "%s|%s|%s\n",
                 game_details.moves[i].player_name,
                 game_details.moves[i].guess,
                 game_details.moves[i].result);
        strncat(response, move, sizeof(response) - strlen(response) - 1);
      }

      strcpy(message->payload, response);
      message->status = SUCCESS;
    }

    send(client_sock, message, sizeof(Message), 0);
    break;
  }
  case GAME_END:
  {
    printf("Received game end\n");
    printf("Payload: %s\n", message->payload);
    int session_id;
    char player_name[50];
    sscanf(message->payload, "%d|%s", &session_id, player_name);
    GameSession *session = &game_sessions[session_id];
    if (strcmp(player_name, session->player1_name) == 0 || strcmp(player_name, session->player2_name) == 0)
    {
      session->game_active = 0;
      // Send a final turn update to both players
      Message turn_message;
      turn_message.message_type = GAME_TURN;
      sprintf(turn_message.payload, "%d", 0);
      turn_message.status = SUCCESS;
      send(get_player_sock(session->player1_name), &turn_message, sizeof(Message), 0);
      send(get_player_sock(session->player2_name), &turn_message, sizeof(Message), 0);
      get_time_as_string(session->end_time, sizeof(session->end_time));
      // Update score for player win
      User user;
      char win_player[50];
      if (strcmp(player_name, session->player1_name) == 0)
      {
        strcpy(win_player, session->player2_name);
      }
      else
      {
        strcpy(win_player, session->player1_name);
      }
      int get_user = get_user_by_username(db, win_player, &user);
      if (get_user == SQLITE_OK)
      {
        printf("User found: %s\n", user.username);
      }
      else
      {
        printf("User not found\n");
      }
      user.score += 10;
      int upd_score = update_user_score(db, user.username, user.score);
      if (upd_score != SQLITE_OK)
      {
        printf("Failed to update user score\n");
      }
      else
      {
        printf("User score updated\n");
      }

      Message end_message;
      end_message.message_type = GAME_END;
      end_message.status = SUCCESS;
      sprintf(end_message.payload, "%s", player_name);
      send(get_player_sock(session->player1_name), &end_message, sizeof(Message), 0);
      send(get_player_sock(session->player2_name), &end_message, sizeof(Message), 0);

      // Save game history
      GameHistory game_history;
      strcpy(game_history.game_id, session->game_id);
      strcpy(game_history.player1, session->player1_name);
      strcpy(game_history.player2, session->player2_name);
      strcpy(game_history.word, session->last_word);
      game_history.player1_score = session->player1_score;
      game_history.player2_score = session->player2_score;

      if (strcmp(player_name, session->player1_name) == 0)
      {
        strcpy(game_history.winner, session->player2_name);
      }
      else
      {
        strcpy(game_history.winner, session->player1_name);
      }

      strcpy(game_history.start_time, session->start_time);
      strcpy(game_history.end_time, session->end_time);

      // Copy the turns from GameSession to GameHistory
      for (int i = 0; i < MAX_ATTEMPTS; i++)
      {
        if (strlen(session->turns[i].guess) == 0)
        {
          break;
        }

        strcpy(game_history.moves[i].player_name, session->turns[i].player_name);
        strcpy(game_history.moves[i].guess, session->turns[i].guess);
        strcpy(game_history.moves[i].result, session->turns[i].result);
      }

      // Save game history and moves to the database
      int rc = save_game_history(db, &game_history);
      if (rc != SQLITE_OK)
      {
        printf("Failed to save game history to the database: %d\n", rc);
      }
      else
      {
        printf("Game history saved successfully.\n");
      }

      clear_game_session(session_id);
    }
    break;
  }
  case GAME_TIMEOUT:
  {
    int session_id;
    char loser_name[50];
    sscanf(message->payload, "%d|%49s", &session_id, loser_name);

    GameSession *session = &game_sessions[session_id];
    if (!session->game_active)
      break;

    // Xác định người thắng
    char winner_name[50];
    int winner_sock, loser_sock;
    int loser_num; // 1 hoặc 2

    if (strcmp(loser_name, session->player1_name) == 0)
    {
      strcpy(winner_name, session->player2_name);
      loser_sock = get_player_sock(session->player1_name);
      winner_sock = get_player_sock(session->player2_name);
      loser_num = 1;
    }
    else
    {
      strcpy(winner_name, session->player1_name);
      loser_sock = get_player_sock(session->player2_name);
      winner_sock = get_player_sock(session->player1_name);
      loser_num = 2;
    }

    // --- TÍNH ĐIỂM (Càng thắng nhanh càng nhiều điểm) ---
    // Công thức: 50 điểm gốc + (200 / số lượt).
    int turns = (session->current_attempts > 0) ? session->current_attempts : 1;
    int score_change = 50 + (200 / turns);

    // Cập nhật DB
    User winner_user, loser_user;
    if (get_user_by_username(db, winner_name, &winner_user) == SQLITE_OK)
    {
      update_user_score(db, winner_name, winner_user.score + score_change);
    }
    if (get_user_by_username(db, loser_name, &loser_user) == SQLITE_OK)
    {
      int new_score = (loser_user.score - score_change < 0) ? 0 : (loser_user.score - score_change);
      update_user_score(db, loser_name, new_score);
    }

    // Gửi kết quả
    Message end_msg;
    end_msg.message_type = GAME_END;
    end_msg.status = SUCCESS;
    // Payload: WINNER_NAME | SCORE_CHANGE
    sprintf(end_msg.payload, "%s|%d", winner_name, score_change);

    if (winner_sock != -1)
      send(winner_sock, &end_msg, sizeof(Message), 0);
    if (loser_sock != -1)
      send(loser_sock, &end_msg, sizeof(Message), 0);

    // Lưu lịch sử và dọn dẹp
    session->game_active = 0;

    GameHistory game_history;
    strcpy(game_history.game_id, session->game_id);
    strcpy(game_history.player1, session->player1_name);
    strcpy(game_history.player2, session->player2_name);
    strcpy(game_history.winner, winner_name);
    game_history.player1_score = (loser_num == 2) ? score_change : 0;
    game_history.player2_score = (loser_num == 1) ? score_change : 0;

    get_time_as_string(game_history.end_time, sizeof(game_history.end_time));
    strcpy(game_history.start_time, session->start_time);
    strcpy(game_history.word, "TIMEOUT");

    // Copy moves (tối đa 12)
    for (int i = 0; i < 12; i++)
    {
      if (i >= session->current_attempts)
        break;
      game_history.moves[i] = session->turns[i];
    }

    save_game_history(db, &game_history);
    clear_game_session(session_id);
    break;
  }
  default:
  {
    message->status = BAD_REQUEST;
    strcpy(message->payload, "Invalid message type");
    send(client_sock, message, sizeof(Message), 0);
    break;
  }
  }
}
