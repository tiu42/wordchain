# Wordchain

Đây là dự án bài tập cho môn thực hành **Lập trình mạng**. Ứng dụng là một trò chơi nối từ nhiều người chơi (Multiplayer Word Chain) sử dụng kiến trúc Client-Server.

## Cấu trúc dự án

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

## Yêu cầu hệ thống

Dự án phụ thuộc vào các thư viện và công cụ sau:

- `libgtk-3-dev`: Thư viện phát triển cho GTK+ 3, dùng để xây dựng giao diện đồ họa người dùng (GUI).

- `sqlite3`: Công cụ dòng lệnh để quản lý cơ sở dữ liệu SQLite.

- `libsqlite3-dev`: Thư viện phát triển để liên kết ứng dụng C với SQLite.

- `glade`: Công cụ thiết kế giao diện GTK trực quan (kéo thả).

## Cài đặt thư viện

Để cài đặt các gói cần thiết trên Ubuntu, chạy lệnh sau:

```bash
sudo apt-get install sqlite3 libsqlite3-dev libgtk-3-dev glade
```

## Khởi tạo Cơ sở dữ liệu

Trước khi chạy Server, bạn cần tạo cơ sở dữ liệu và nạp dữ liệu mẫu (Seed data).

Di chuyển vào thư mục seed (hoặc biên dịch ngay tại thư mục chứa file seed):

```bash
cd src
gcc -o seed seed/seed.c -lsqlite3
./seed
```

Truy cập vào database (Nếu cần)

```bash
sqlite3 database.db
```

> Khi bạn chạy chương trình seed, nó sẽ tự động tạo tệp database.db và điền dữ liệu mẫu ban đầu vào đó. Do đó, nếu bạn xóa tệp database.db, bạn chỉ cần chạy lại lệnh `./seed` để tạo lại cơ sở dữ liệu với dữ liệu mẫu.

## Data

- Dữ liệu mẫu trong `seed/seed.c`

- Dữ liệu từ vựng 5 chữ trong `valid_words.txt` tổng hợp từ kaggle

## Run app

Biên dịch (tại `./src`)

```bash
make
```

Xóa file.o (tại `./src`)

```bash
make clean
```

Chạy Server (tại `./src`)

```bash
./server
```

Chạy Client (tại `./src`)

```bash
./client
```

## UI

Để chạy glade để chỉnh giao diện (Nếu cần):

```bash
glade wordle.glade
```

## Các công việc còn lại

- Comment code sao cho hợp lý

- Xóa các hàm không cần thiết

- Sửa các dòng warning khi biên dịch make (nếu cần)

- Sửa các tên biến

- Sửa các logic sai nếu phát hiện

- Xem cách tính điểm (Sửa nếu cần)

- Sắp xếp lại file `valid_words`

- Thay đổi dữ liệu seed trong `seed.c`

- Test thử nghiệm
