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


/****************************Database Function*******************************/
int open_database(){
  int rc = sqlite3_open(DB_FILE, &db);
  if (rc)
  {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    return 1;
  }
  printf("Database opened successfully\n");
  return 0;
}

void close_database(){
  sqlite3_close(db);
  printf("Database closed successfully\n");
}
/***************************************************************************/

/*****************************Utils Function***********************************/
void get_time_as_string(char *buffer, size_t buffer_size) {
  time_t raw_time;
  struct tm *time_info;

  // Get current time
  time(&raw_time);

  // Convert time to `tm` structure
  time_info = localtime(&raw_time);

  // Format time as string
  strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", time_info);
}

void generate_game_id(char *game_id, size_t size){
  time_t now = time(NULL);
  snprintf(game_id, size, "GAME-%ld", now);
}

int add_player(const char *player_name, int player_sock){
  // Check if the array is full
  if (player_count >= MAX_PLAYERS)
  {
    printf("Player list is full!\n");
    return -1; // Player list is full
  }

  // Check if the player already exists
  for (int i = 0; i < player_count; i++)
  {
    if (strcmp(player_list[i].player_name, player_name) == 0)
    {
      printf("Player %s already exists!\n", player_name);
      return -1; // Player already exists
    }
  }

  // Add the new player to the array
  strcpy(player_list[player_count].player_name, player_name);
  player_list[player_count].player_sock = player_sock;
  player_count++;

  return 0; // Successfully added
}

int get_player_sock(const char *player_name){
  for (int i = 0; i < player_count; i++)
  {
    if (strcmp(player_list[i].player_name, player_name) == 0)
    {
      return player_list[i].player_sock; // Return the player's socket
    }
  }

  printf("Player %s not found!\n", player_name);
  return -1; // Player not found
}

// Create a new game session with Player1 and Player2
int create_game_session(const char *player1_name, const char *player2_name){
  for (int i = 0; i < MAX_SESSIONS; i++)
  {
    if (!game_sessions[i].game_active)
    {
      generate_game_id(game_sessions[i].game_id, sizeof(game_sessions[i].game_id));

      // Initialize the game session with both players' names
      strcpy(game_sessions[i].player1_name, player1_name);
      strcpy(game_sessions[i].player2_name, player2_name);
      game_sessions[i].player1_attempts = 0;
      game_sessions[i].player2_attempts = 0;
      game_sessions[i].current_player = 1; // Player 1 starts the game
      // For WordChain we don't preselect a target word; first played word starts the chain
      game_sessions[i].target_word[0] = '\0';
      game_sessions[i].player1_score = 0;
      game_sessions[i].player2_score = 0;
      // initialize other fields
      game_sessions[i].game_active = 1;
      game_sessions[i].current_attempts = 0;
      get_time_as_string(game_sessions[i].start_time, sizeof(game_sessions[i].start_time));
      return i; // Return the game session ID
    }
  }
  return -1; // No available space for a new game session
}

void clear_game_session(int session_id) {
  GameSession *session = &game_sessions[session_id];

  // Reset player information
  memset(session->player1_name, 0, sizeof(session->player1_name));
  memset(session->player2_name, 0, sizeof(session->player2_name));
  memset(session->target_word, 0, sizeof(session->target_word));

  // Reset game state
  session->current_player = 0;
  session->player1_attempts = 0;
  session->player2_attempts = 0;
  session->game_active = 0;
  session->current_attempts = 0;
  session->player1_score = 0;
  session->player2_score = 0;

  // Clear turns history
  memset(session->turns, 0, sizeof(session->turns));
  memset(session->start_time, 0, sizeof(session->start_time));
  memset(session->end_time, 0, sizeof(session->end_time));

  printf("Cleared game session %d\n", session_id);
}

// Function to find an existing game with Player1 and Player2
int find_existing_game(const char *player1_name, const char *player2_name){
  for (int i = 0; i < MAX_SESSIONS; i++)
  {
    if (game_sessions[i].game_active)
    {
      // Check if Player1 and Player2 are already in a game
      if ((strcmp(game_sessions[i].player1_name, player1_name) == 0 &&
           strcmp(game_sessions[i].player2_name, player2_name) == 0) ||
          (strcmp(game_sessions[i].player1_name, player2_name) == 0 &&
           strcmp(game_sessions[i].player2_name, player1_name) == 0))
      {
        return i; // Return the game session ID
      }
    }
  }
  return -1; // Game not found
}

void handle_client_disconnect(int client_sock) {
  char disconnected_player[50];
  int player_index = -1;

  // Find the disconnected player
  for (int i = 0; i < player_count; i++) {
    if (player_list[i].player_sock == client_sock) {
      strcpy(disconnected_player, player_list[i].player_name);
      player_index = i;
      break;
    }
  }

  if (player_index == -1) {
    printf("Disconnected player not found\n");
    return;
  }

  // Remove the player from the player list
  for (int i = player_index; i < player_count - 1; i++) {
    player_list[i] = player_list[i + 1];
  }
  player_count--;

  // Check if the player was in an active game session
  for (int i = 0; i < MAX_SESSIONS; i++) {
    GameSession *session = &game_sessions[i];
    if (session->game_active &&
        (strcmp(session->player1_name, disconnected_player) == 0 ||
         strcmp(session->player2_name, disconnected_player) == 0)) {

      // Determine the opponent
      const char *opponent = strcmp(session->player1_name, disconnected_player) == 0 ?
                             session->player2_name : session->player1_name;
      int opponent_sock = get_player_sock(opponent);

      // Notify the opponent that they win
      Message message;
      message.message_type = GAME_END;
      message.status = SUCCESS;
      sprintf(message.payload, "%s", disconnected_player);
      send(opponent_sock, &message, sizeof(Message), 0);

      // Save game history
      GameHistory game_history;
      strcpy(game_history.game_id, session->game_id);
      strcpy(game_history.player1, session->player1_name);
      strcpy(game_history.player2, session->player2_name);
      strcpy(game_history.word, session->target_word);
      game_history.player1_score = session->player1_score;
      game_history.player2_score = session->player2_score;

      if (strcmp(disconnected_player, session->player1_name) == 0) {
        strcpy(game_history.winner, session->player2_name);
      } else {
        strcpy(game_history.winner, session->player1_name);
      }

      strcpy(game_history.start_time, session->start_time);
      strcpy(game_history.end_time, session->end_time);

      // Copy the turns from GameSession to GameHistory
      for (int j = 0; j < MAX_ATTEMPTS; j++) {
        if (strlen(session->turns[j].guess) == 0) {
          break;
        }
        strcpy(game_history.moves[j].player_name, session->turns[j].player_name);
        strcpy(game_history.moves[j].guess, session->turns[j].guess);
        strcpy(game_history.moves[j].result, session->turns[j].result);
      }

      // Save game history and moves to the database
      int rc = save_game_history(db, &game_history);
      if (rc != SQLITE_OK) {
        printf("Failed to save game history to the database: %d\n", rc);
      } else {
        printf("Game history saved successfully.\n");
      }

      // Clear the game session
      clear_game_session(i);
      break;
    }
  }

  printf("Player %s disconnected\n", disconnected_player);
  close(client_sock);
}
/***************************************************************************/





/*****************************INIT SERVER*************************************/

// Signal handler to handle interruptions (SIGINT)
void signal_handler(int sig){
  got_signal = 1;
  printf("Caught signal %d\n", sig);
}

// Setup the signal handler for SIGINT
void setup_signal_handler(){
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
}

void init_game_sessions(){
  for (int i = 0; i < MAX_SESSIONS; i++)
  {
    game_sessions[i].game_active = 0;
  }
}

int initialize_server(int *server_sock, struct sockaddr_in *server_addr){
  // Create the server socket
  if ((*server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0)
  {
    perror("Socket failed");
    exit(EXIT_FAILURE);
  }

  server_addr->sin_family = AF_INET;
  server_addr->sin_addr.s_addr = INADDR_ANY;
  server_addr->sin_port = htons(PORT);

  // Bind the socket to the specified IP address and port
  if (bind(*server_sock, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)
  {
    perror("Bind failed");
    exit(EXIT_FAILURE);
  }

  // Start listening for incoming client connections
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

// Helper: find session index by game_id
int find_session_by_id(const char *game_id) {
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (game_sessions[i].game_active && strcmp(game_sessions[i].game_id, game_id) == 0) {
      return i;
    }
  }
  return -1;
}

// Helper: get first token (syllable) before first space
void get_first_syllable(const char *word, char *out, size_t out_size) {
  if (!word || !out) return;
  while (*word == ' ') word++;
  const char *p = strchr(word, ' ');
  if (!p) {
    strncpy(out, word, out_size - 1);
    out[out_size - 1] = '\0';
    return;
  }
  size_t len = p - word;
  if (len >= out_size) len = out_size - 1;
  strncpy(out, word, len);
  out[len] = '\0';
}

// Helper: get last token (syllable) after last space
void get_last_syllable(const char *word, char *out, size_t out_size) {
  if (!word || !out) return;
  const char *p = strrchr(word, ' ');
  if (!p) {
    // single token
    strncpy(out, word, out_size - 1);
    out[out_size - 1] = '\0';
    return;
  }
  p++; // move past space
  strncpy(out, p, out_size - 1);
  out[out_size - 1] = '\0';
}

// Helper: count tokens separated by spaces
int count_syllables(const char *word) {
  if (!word) return 0;
  int count = 0;
  const char *p = word;
  char last = ' ';
  while (*p) {
    if (*p != ' ' && last == ' ') count++;
    last = *p;
    p++;
  }
  return count;
}

// Helper: safe string copy
void safe_strcpy(char *dst, const char *src, size_t dst_size) {
  if (!dst) return;
  if (!src) {
    dst[0] = '\0';
    return;
  }
  strncpy(dst, src, dst_size - 1);
  dst[dst_size - 1] = '\0';
}

int main(){
  int server_sock, new_sock, client_socks[MAX_CLIENTS] = {0};
  struct sockaddr_in server_addr, client_addr;
  fd_set readfds;
  socklen_t addr_len = sizeof(client_addr);
  sigset_t block_mask, orig_mask;

  int rc = open_database();
  if (rc)
  {
    return 1;
  }

  // Seed the users
  // seed_users(users, &size);

  // Set up the signal handler
  setup_signal_handler();

  // Block SIGINT signal to handle it later
  sigemptyset(&block_mask);
  sigaddset(&block_mask, SIGINT);
  sigprocmask(SIG_BLOCK, &block_mask, &orig_mask);

  // Initialize server socket and bind to address
  initialize_server(&server_sock, &server_addr);

  while (1)
  {
    FD_ZERO(&readfds);
    FD_SET(server_sock, &readfds);
    int max_sd = server_sock;

    // Add client sockets to the read set
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
      int sock = client_socks[i];
      if (sock > 0)
        FD_SET(sock, &readfds);
      if (sock > max_sd)
        max_sd = sock;
    }

    // Wait for activity on any of the sockets
    int ready = pselect(max_sd + 1, &readfds, NULL, NULL, NULL, &orig_mask);
    if (ready == -1)
    {
      if (errno == EINTR)
      {
        printf("pselect() interrupted by signal.\n");
        if (got_signal)
        {
          printf("Received SIGINT, shutting down server.\n");
          break; // Exit the loop when SIGINT is received
        }
        continue;
      }
      else
      {
        perror("pselect");
        exit(EXIT_FAILURE);
      }
    }

    // Check if there's a new incoming connection
    if (FD_ISSET(server_sock, &readfds))
    {
      if ((new_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len)) < 0)
      {
        perror("Accept failed");
        continue;
      }
      printf("New connection, socket fd is %d\n", new_sock);

      // Add the new socket to the client sockets array
      for (int i = 0; i < MAX_CLIENTS; i++)
      {
        if (client_socks[i] == 0)
        {
          client_socks[i] = new_sock;
          break;
        }
      }
    }

    // Handle messages from connected clients
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
          // Client disconnected
          close(sock);
          client_socks[i] = 0;
        }
        else
        {
          // Process the received message
          handle_message(sock, &message);
        }
      }
    }

    // Check if a signal was received after each pselect call
    if (got_signal)
    {
      printf("Received SIGINT, shutting down server.\n");
      break;
    }
  }

  // Close the database connection and the server socket before exiting
  close_database();
  close(server_sock);
  printf("Server stopped.\n");
  return 0;
}

void handle_message(int client_sock, Message *message){
  switch(message->message_type){
    case SIGNUP_REQUEST:
    {
      char username[50], password[50];
      sscanf(message->payload, "%49[^|]|%49s", username, password);

      if (username[0] == '\0' || password[0] == '\0') {
        message->status = BAD_REQUEST;
        strcpy(message->payload, "Username or password is missing.");
      } else if (user_exists(db, username)) {
        message->status = BAD_REQUEST;
        strcpy(message->payload, "Username already exists.");
      } else {
        User new_user;
        strncpy(new_user.username, username, sizeof(new_user.username) - 1);
        new_user.username[sizeof(new_user.username) - 1] = '\0';
        strncpy(new_user.password, password, sizeof(new_user.password) - 1);
        new_user.password[sizeof(new_user.password) - 1] = '\0';
        new_user.score = 0;
        new_user.is_online = 0;

        int rc = create_user(db, &new_user);
        if (rc == SQLITE_OK) {
          message->status = SUCCESS;
          strcpy(message->payload, "User registered successfully.");
        } else {
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

      if (login_status == 1) {
        int update_status = update_user_online(db, username);
        if (update_status != SQLITE_DONE) {
          message->status = INTERNAL_SERVER_ERROR;
          strcpy(message->payload, "Failed to update user status");
        } else {
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
      } else if (login_status == 0) {
        message->status = UNAUTHORIZED;
        strcpy(message->payload, "Invalid username or password");
      } else {
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
      if (auth_status == 1) {
          int update_status = update_user_offline(db, username);
          if (update_status == SQLITE_DONE || update_status == SQLITE_OK) {
              message->status = SUCCESS;
              strcpy(message->payload, "Logout successful");
              // Clear PlayerInfo
              for(int i = 0; i < MAX_PLAYERS;i++){
                if(strcmp(player_list[i].player_name,username) == 0){
                    player_list[i].player_sock = -1; // Clear the player's socket
                    memset(player_list[i].player_name, 0, sizeof(player_list[i].player_name)); // Clear the player's name
                    break;
                }
            }
          } else {
              message->status = INTERNAL_SERVER_ERROR;
              strcpy(message->payload, "Failed to update user status");
          }
      } else if (auth_status == 0) {
          message->status = UNAUTHORIZED;
          strcpy(message->payload, "Invalid username or password");
      } else {
          message->status = INTERNAL_SERVER_ERROR;
          strcpy(message->payload, "Logout failed");
      }
      send(client_sock, message, sizeof(Message), 0);
      break;
    }
    case GET_SCORE_BY_USER_REQUEST:
    {
      // TODO: Implement get score by username
      break;
    }
    case LIST_USER:
    {
      // TODO: Implement list users online
      break;
    }
    case CHALLANGE_REQUEST:
    {
      char player1[50], player2[50];
      sscanf(message->payload, "CHALLANGE_REQUEST|%49[^|]|%49s", player1, player2);
      int player1_sock = get_player_sock(player1);
      int player2_sock = get_player_sock(player2);
      // Check if players are online
      if(player1_sock == -1 || player2_sock == -1){
        message->status = BAD_REQUEST;
        strcpy(message->payload, "Player not found");
        send(client_sock, message, sizeof(Message), 0);
        return;
      }
       // Check if players are already in a game
      for (int i = 0; i < MAX_SESSIONS; i++) {
          GameSession *session = &game_sessions[i];
          if (session->game_active &&
              (strcmp(session->player1_name, player1) == 0 || strcmp(session->player2_name, player1) == 0 ||
              strcmp(session->player1_name, player2) == 0 || strcmp(session->player2_name, player2) == 0)) {
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
      if(player1_sock == -1 || player2_sock == -1){
        message->status = BAD_REQUEST;
        strcpy(message->payload, "Player not found");
        send(client_sock, message, sizeof(Message), 0);
        return;
      }
      if(strcmp(response, "ACCEPT") == 0){
        message->status = SUCCESS;
        send(player1_sock, message, sizeof(Message), 0);

        send(player2_sock, message, sizeof(Message), 0);
      }else{
        message->status = BAD_REQUEST;
        strcpy(message->payload, "Challange rejected");
        send(player1_sock, message, sizeof(Message), 0);
      }
      break;
    }
    case GAME_START:
    {
      // Expect payload: "GAME_START|player1|player2" or "player1|player2"
      {
        char player1[50] = {0}, player2[50] = {0};
        if (sscanf(message->payload, "GAME_START|%49[^|]|%49s", player1, player2) < 2) {
          sscanf(message->payload, "%49[^|]|%49s", player1, player2);
        }

        if (player1[0] == '\0' || player2[0] == '\0') {
          message->status = BAD_REQUEST;
          strcpy(message->payload, "Missing players");
          send(client_sock, message, sizeof(Message), 0);
          break;
        }

        // create session
        int sid = create_game_session(player1, player2);
        if (sid < 0) {
          message->status = INTERNAL_SERVER_ERROR;
          strcpy(message->payload, "Failed to create game session");
          send(client_sock, message, sizeof(Message), 0);
          break;
        }

        GameSession *session = &game_sessions[sid];
        // initialize rule settings
        session->turn_time_limit = TURN_TIME_SECONDS;
        session->max_attempts_per_turn = MAX_ATTEMPTS_PER_TURN;
        session->required_syllables = REQUIRED_SYLLABLES;
        session->last_syllable[0] = '\0';
        session->turn_start_time = time(NULL);

        // notify both players
        Message out;
        out.message_type = GAME_START;
        out.status = SUCCESS;
        snprintf(out.payload, sizeof(out.payload), "%s|%s|%d", session->game_id, session->target_word, session->current_player);
        int p1_sock = get_player_sock(player1);
        int p2_sock = get_player_sock(player2);
        if (p1_sock != -1) send(p1_sock, &out, sizeof(Message), 0);
        if (p2_sock != -1) send(p2_sock, &out, sizeof(Message), 0);
      }
      break;
    }
    case GAME_GET_TARGET:
    {
      // Expect payload: "GAME_GET_TARGET|<game_id>" or just "<game_id>"
      {
        char game_id[32] = {0};
        if (sscanf(message->payload, "GAME_GET_TARGET|%31s", game_id) < 1) {
          sscanf(message->payload, "%31s", game_id);
        }
        if (game_id[0] == '\0') {
          message->status = BAD_REQUEST;
          strcpy(message->payload, "Missing game_id");
          send(client_sock, message, sizeof(Message), 0);
          break;
        }
        int sid = find_session_by_id(game_id);
        if (sid < 0) {
          message->status = NOT_FOUND;
          strcpy(message->payload, "Game not found");
          send(client_sock, message, sizeof(Message), 0);
          break;
        }
        GameSession *session = &game_sessions[sid];
        message->status = SUCCESS;
        snprintf(message->payload, sizeof(message->payload), "%s|%d", session->target_word, session->current_player);
        send(client_sock, message, sizeof(Message), 0);
      }
      break;
    }
    case GAME_GUESS:
    {
      // Expect payload: "GAME_GUESS|<game_id>|<player_name>|<guess>"
      {
        char game_id[32] = {0};
        char player_name[50] = {0};
        char guess[MAX_WORD_LEN + 1] = {0};
        // Build a safe scanf format using MAX_WORD_LEN
        char fmt[64];
        snprintf(fmt, sizeof(fmt), "%%31[^|]|%%49[^|]|%%%d[^|]", MAX_WORD_LEN);
        const char *p = message->payload;
        if (strncmp(p, "GAME_GUESS|", 11) == 0) p += 11;
        if (sscanf(p, fmt, game_id, player_name, guess) < 3) {
          message->status = BAD_REQUEST;
          strcpy(message->payload, "Invalid guess payload");
          send(client_sock, message, sizeof(Message), 0);
          break;
        }

        int sid = find_session_by_id(game_id);
        if (sid < 0) {
          message->status = NOT_FOUND;
          strcpy(message->payload, "Game not found");
          send(client_sock, message, sizeof(Message), 0);
          break;
        }

        GameSession *session = &game_sessions[sid];

        // Check player's turn
        int expected_player = (strcmp(session->player1_name, player_name) == 0) ? 1 : ((strcmp(session->player2_name, player_name) == 0) ? 2 : 0);
        if (expected_player == 0 || expected_player != session->current_player) {
          message->status = FORBIDDEN;
          strcpy(message->payload, "Not your turn");
          send(client_sock, message, sizeof(Message), 0);
          break;
        }

        // Check timeout
        time_t now = time(NULL);
        double elapsed = difftime(now, session->turn_start_time);
        if (elapsed > session->turn_time_limit) {
          // timeout -> opponent wins
          const char *winner = (session->current_player == 1) ? session->player2_name : session->player1_name;
          // update DB score
          int cur_score = 0;
          if (get_score_by_username(db, winner, &cur_score) == SQLITE_OK) {
            update_user_score(db, winner, cur_score + 1);
          }

          // Prepare game history
          GameHistory game_history;
          safe_strcpy(game_history.game_id, session->game_id, sizeof(game_history.game_id));
          safe_strcpy(game_history.player1, session->player1_name, sizeof(game_history.player1));
          safe_strcpy(game_history.player2, session->player2_name, sizeof(game_history.player2));
          safe_strcpy(game_history.word, session->target_word, sizeof(game_history.word));
          game_history.player1_score = session->player1_score;
          game_history.player2_score = session->player2_score;
          safe_strcpy(game_history.winner, winner, sizeof(game_history.winner));
          safe_strcpy(game_history.start_time, session->start_time, sizeof(game_history.start_time));
          safe_strcpy(game_history.end_time, session->end_time, sizeof(game_history.end_time));
          // copy moves
          for (int j = 0; j < MAX_ATTEMPTS && j < 12; j++) {
            safe_strcpy(game_history.moves[j].player_name, session->turns[j].player_name, sizeof(game_history.moves[j].player_name));
            safe_strcpy(game_history.moves[j].guess, session->turns[j].guess, sizeof(game_history.moves[j].guess));
            safe_strcpy(game_history.moves[j].result, session->turns[j].result, sizeof(game_history.moves[j].result));
          }
          save_game_history(db, &game_history);

          // notify players
          Message out;
          out.message_type = GAME_END;
          out.status = SUCCESS;
          snprintf(out.payload, sizeof(out.payload), "%s", winner);
          int s1 = get_player_sock(session->player1_name);
          int s2 = get_player_sock(session->player2_name);
          if (s1 != -1) send(s1, &out, sizeof(Message), 0);
          if (s2 != -1) send(s2, &out, sizeof(Message), 0);

          clear_game_session(sid);
          break;
        }

        // Validate guess: must have required syllables
        int sylls = count_syllables(guess);
        if (sylls != session->required_syllables) {
          // invalid format/length
          session->current_attempts++;
          // record attempt
          for (int i = 0; i < MAX_ATTEMPTS; i++) {
            if (session->turns[i].guess[0] == '\0') {
              safe_strcpy(session->turns[i].player_name, player_name, sizeof(session->turns[i].player_name));
              safe_strcpy(session->turns[i].guess, guess, sizeof(session->turns[i].guess));
              safe_strcpy(session->turns[i].result, "INVALID_SYLLABLE_COUNT", sizeof(session->turns[i].result));
              session->turns[i].attempt_number = session->current_attempts;
              session->turns[i].time_taken_seconds = (int)elapsed;
              session->turns[i].is_valid = 0;
              break;
            }
          }
          // check attempts
          if (session->current_attempts >= session->max_attempts_per_turn) {
            const char *winner = (session->current_player == 1) ? session->player2_name : session->player1_name;
            int cur_score = 0;
            if (get_score_by_username(db, winner, &cur_score) == SQLITE_OK) {
              update_user_score(db, winner, cur_score + 1);
            }
            // save history
            GameHistory game_history;
            safe_strcpy(game_history.game_id, session->game_id, sizeof(game_history.game_id));
            safe_strcpy(game_history.player1, session->player1_name, sizeof(game_history.player1));
            safe_strcpy(game_history.player2, session->player2_name, sizeof(game_history.player2));
            safe_strcpy(game_history.word, session->target_word, sizeof(game_history.word));
            game_history.player1_score = session->player1_score;
            game_history.player2_score = session->player2_score;
            safe_strcpy(game_history.winner, winner, sizeof(game_history.winner));
            safe_strcpy(game_history.start_time, session->start_time, sizeof(game_history.start_time));
            safe_strcpy(game_history.end_time, session->end_time, sizeof(game_history.end_time));
            for (int j = 0; j < MAX_ATTEMPTS && j < 12; j++) {
              safe_strcpy(game_history.moves[j].player_name, session->turns[j].player_name, sizeof(game_history.moves[j].player_name));
              safe_strcpy(game_history.moves[j].guess, session->turns[j].guess, sizeof(game_history.moves[j].guess));
              safe_strcpy(game_history.moves[j].result, session->turns[j].result, sizeof(game_history.moves[j].result));
            }
            save_game_history(db, &game_history);

            Message out;
            out.message_type = GAME_END;
            out.status = SUCCESS;
            snprintf(out.payload, sizeof(out.payload), "%s", winner);
            int s1 = get_player_sock(session->player1_name);
            int s2 = get_player_sock(session->player2_name);
            if (s1 != -1) send(s1, &out, sizeof(Message), 0);
            if (s2 != -1) send(s2, &out, sizeof(Message), 0);

            clear_game_session(sid);
            break;
          }
          // else inform player of invalid attempt
          message->status = BAD_REQUEST;
          strcpy(message->payload, "Invalid guess format or syllable count");
          send(client_sock, message, sizeof(Message), 0);
          break;
        }

        // If target empty, accept as first move
        if (session->target_word[0] == '\0') {
          safe_strcpy(session->target_word, guess, sizeof(session->target_word));
          get_last_syllable(guess, session->last_syllable, sizeof(session->last_syllable));
          session->current_attempts = 0;
          // record turn
          for (int i = 0; i < MAX_ATTEMPTS; i++) {
            if (session->turns[i].guess[0] == '\0') {
              safe_strcpy(session->turns[i].player_name, player_name, sizeof(session->turns[i].player_name));
              safe_strcpy(session->turns[i].guess, guess, sizeof(session->turns[i].guess));
              safe_strcpy(session->turns[i].result, "OK", sizeof(session->turns[i].result));
              session->turns[i].attempt_number = 1;
              session->turns[i].time_taken_seconds = (int)elapsed;
              session->turns[i].is_valid = 1;
              break;
            }
          }
          // switch player
          session->current_player = (session->current_player == 1) ? 2 : 1;
          session->turn_start_time = time(NULL);

          // notify both players
          Message out;
          out.message_type = GAME_UPDATE;
          out.status = SUCCESS;
          snprintf(out.payload, sizeof(out.payload), "OK|%s|%d", guess, session->current_player);
          int s1 = get_player_sock(session->player1_name);
          int s2 = get_player_sock(session->player2_name);
          if (s1 != -1) send(s1, &out, sizeof(Message), 0);
          if (s2 != -1) send(s2, &out, sizeof(Message), 0);
          break;
        }

        // Validate chain: first syllable of guess must equal last_syllable
        char first[64];
        get_first_syllable(guess, first, sizeof(first));
        if (strcasecmp(first, session->last_syllable) != 0) {
          // mismatch
          session->current_attempts++;
          for (int i = 0; i < MAX_ATTEMPTS; i++) {
            if (session->turns[i].guess[0] == '\0') {
              safe_strcpy(session->turns[i].player_name, player_name, sizeof(session->turns[i].player_name));
              safe_strcpy(session->turns[i].guess, guess, sizeof(session->turns[i].guess));
              safe_strcpy(session->turns[i].result, "MISMATCH", sizeof(session->turns[i].result));
              session->turns[i].attempt_number = session->current_attempts;
              session->turns[i].time_taken_seconds = (int)elapsed;
              session->turns[i].is_valid = 0;
              break;
            }
          }
          if (session->current_attempts >= session->max_attempts_per_turn) {
            const char *winner = (session->current_player == 1) ? session->player2_name : session->player1_name;
            int cur_score = 0;
            if (get_score_by_username(db, winner, &cur_score) == SQLITE_OK) {
              update_user_score(db, winner, cur_score + 1);
            }
            // save history
            GameHistory game_history;
            safe_strcpy(game_history.game_id, session->game_id, sizeof(game_history.game_id));
            safe_strcpy(game_history.player1, session->player1_name, sizeof(game_history.player1));
            safe_strcpy(game_history.player2, session->player2_name, sizeof(game_history.player2));
            safe_strcpy(game_history.word, session->target_word, sizeof(game_history.word));
            game_history.player1_score = session->player1_score;
            game_history.player2_score = session->player2_score;
            safe_strcpy(game_history.winner, winner, sizeof(game_history.winner));
            safe_strcpy(game_history.start_time, session->start_time, sizeof(game_history.start_time));
            safe_strcpy(game_history.end_time, session->end_time, sizeof(game_history.end_time));
            for (int j = 0; j < MAX_ATTEMPTS && j < 12; j++) {
              safe_strcpy(game_history.moves[j].player_name, session->turns[j].player_name, sizeof(game_history.moves[j].player_name));
              safe_strcpy(game_history.moves[j].guess, session->turns[j].guess, sizeof(game_history.moves[j].guess));
              safe_strcpy(game_history.moves[j].result, session->turns[j].result, sizeof(game_history.moves[j].result));
            }
            save_game_history(db, &game_history);

            Message out;
            out.message_type = GAME_END;
            out.status = SUCCESS;
            snprintf(out.payload, sizeof(out.payload), "%s", winner);
            int s1 = get_player_sock(session->player1_name);
            int s2 = get_player_sock(session->player2_name);
            if (s1 != -1) send(s1, &out, sizeof(Message), 0);
            if (s2 != -1) send(s2, &out, sizeof(Message), 0);

            clear_game_session(sid);
            break;
          }
          // else inform player of mismatch
          message->status = BAD_REQUEST;
          strcpy(message->payload, "Guess does not chain with previous word");
          send(client_sock, message, sizeof(Message), 0);
          break;
        }

        // Accept valid chained guess
        safe_strcpy(session->target_word, guess, sizeof(session->target_word));
        get_last_syllable(guess, session->last_syllable, sizeof(session->last_syllable));
        session->current_attempts = 0;
        for (int i = 0; i < MAX_ATTEMPTS; i++) {
          if (session->turns[i].guess[0] == '\0') {
            safe_strcpy(session->turns[i].player_name, player_name, sizeof(session->turns[i].player_name));
            safe_strcpy(session->turns[i].guess, guess, sizeof(session->turns[i].guess));
            safe_strcpy(session->turns[i].result, "OK", sizeof(session->turns[i].result));
            session->turns[i].attempt_number = 1;
            session->turns[i].time_taken_seconds = (int)elapsed;
            session->turns[i].is_valid = 1;
            break;
          }
        }
        // switch player and reset timer
        session->current_player = (session->current_player == 1) ? 2 : 1;
        session->turn_start_time = time(NULL);

        // notify both players
        Message out;
        out.message_type = GAME_UPDATE;
        out.status = SUCCESS;
        snprintf(out.payload, sizeof(out.payload), "OK|%s|%d", guess, session->current_player);
        int s1 = get_player_sock(session->player1_name);
        int s2 = get_player_sock(session->player2_name);
        if (s1 != -1) send(s1, &out, sizeof(Message), 0);
        if (s2 != -1) send(s2, &out, sizeof(Message), 0);
      }
      break;
    }
    case GAME_UPDATE:
    {
      // For now, clients receive GAME_UPDATE pushed from server; no client-initiated update required
      message->status = NOT_IMPLEMENTED;
      strcpy(message->payload, "Client-initiated GAME_UPDATE not supported");
      send(client_sock, message, sizeof(Message), 0);
      break;
    }
    case LIST_GAME_HISTORY:
    {
      //TODO: Implement list game history
      break;
    }
    case GAME_DETAIL_REQUEST:
    {
      //TODO: Implement game detail request
      break;
    }
    case GAME_END:
    {
      // Expect payload: "GAME_END|<game_id>|<winner>" or "<game_id>"
      {
        char game_id[32] = {0};
        char winner[50] = {0};
        if (sscanf(message->payload, "GAME_END|%31[^|]|%49[^"]"", game_id, winner) < 1) {
          sscanf(message->payload, "%31s", game_id);
        }
        int sid = find_session_by_id(game_id);
        if (sid < 0) {
          message->status = NOT_FOUND;
          strcpy(message->payload, "Game not found");
          send(client_sock, message, sizeof(Message), 0);
          break;
        }
        GameSession *session = &game_sessions[sid];
        const char *declared_winner = (winner[0] != '\0') ? winner : session->player1_name;
        int cur_score = 0;
        if (get_score_by_username(db, declared_winner, &cur_score) == SQLITE_OK) {
          update_user_score(db, declared_winner, cur_score + 1);
        }

        GameHistory game_history;
        safe_strcpy(game_history.game_id, session->game_id, sizeof(game_history.game_id));
        safe_strcpy(game_history.player1, session->player1_name, sizeof(game_history.player1));
        safe_strcpy(game_history.player2, session->player2_name, sizeof(game_history.player2));
        safe_strcpy(game_history.word, session->target_word, sizeof(game_history.word));
        game_history.player1_score = session->player1_score;
        game_history.player2_score = session->player2_score;
        safe_strcpy(game_history.winner, declared_winner, sizeof(game_history.winner));
        safe_strcpy(game_history.start_time, session->start_time, sizeof(game_history.start_time));
        safe_strcpy(game_history.end_time, session->end_time, sizeof(game_history.end_time));
        for (int j = 0; j < MAX_ATTEMPTS && j < 12; j++) {
          safe_strcpy(game_history.moves[j].player_name, session->turns[j].player_name, sizeof(game_history.moves[j].player_name));
          safe_strcpy(game_history.moves[j].guess, session->turns[j].guess, sizeof(game_history.moves[j].guess));
          safe_strcpy(game_history.moves[j].result, session->turns[j].result, sizeof(game_history.moves[j].result));
        }
        save_game_history(db, &game_history);

        Message out;
        out.message_type = GAME_END;
        out.status = SUCCESS;
        snprintf(out.payload, sizeof(out.payload), "%s", declared_winner);
        int s1 = get_player_sock(session->player1_name);
        int s2 = get_player_sock(session->player2_name);
        if (s1 != -1) send(s1, &out, sizeof(Message), 0);
        if (s2 != -1) send(s2, &out, sizeof(Message), 0);

        clear_game_session(sid);
      }
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
