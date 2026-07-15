#include "game.h"

#include <algorithm>
#include <cstring>
#include <sstream>

#include "data.h"
#include "progress.h"

namespace {
// Seven tetrominoes, four rotations, four blocks, expressed in a 4x4 box.
constexpr int shapes[7][4][4][2] = {{{{0, 1}, {1, 1}, {2, 1}, {3, 1}},
                                     {{2, 0}, {2, 1}, {2, 2}, {2, 3}},
                                     {{0, 2}, {1, 2}, {2, 2}, {3, 2}},
                                     {{1, 0}, {1, 1}, {1, 2}, {1, 3}}},
                                    {{{1, 0}, {2, 0}, {1, 1}, {2, 1}},
                                     {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
                                     {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
                                     {{1, 0}, {2, 0}, {1, 1}, {2, 1}}},
                                    {{{1, 0}, {0, 1}, {1, 1}, {2, 1}},
                                     {{1, 0}, {1, 1}, {2, 1}, {1, 2}},
                                     {{0, 1}, {1, 1}, {2, 1}, {1, 2}},
                                     {{1, 0}, {0, 1}, {1, 1}, {1, 2}}},
                                    {{{1, 0}, {2, 0}, {0, 1}, {1, 1}},
                                     {{1, 0}, {1, 1}, {2, 1}, {2, 2}},
                                     {{1, 1}, {2, 1}, {0, 2}, {1, 2}},
                                     {{0, 0}, {0, 1}, {1, 1}, {1, 2}}},
                                    {{{0, 0}, {1, 0}, {1, 1}, {2, 1}},
                                     {{2, 0}, {1, 1}, {2, 1}, {1, 2}},
                                     {{0, 1}, {1, 1}, {1, 2}, {2, 2}},
                                     {{1, 0}, {0, 1}, {1, 1}, {0, 2}}},
                                    {{{0, 0}, {0, 1}, {1, 1}, {2, 1}},
                                     {{1, 0}, {2, 0}, {1, 1}, {1, 2}},
                                     {{0, 1}, {1, 1}, {2, 1}, {2, 2}},
                                     {{1, 0}, {1, 1}, {0, 2}, {1, 2}}},
                                    {{{2, 0}, {0, 1}, {1, 1}, {2, 1}},
                                     {{1, 0}, {1, 1}, {1, 2}, {2, 2}},
                                     {{0, 1}, {1, 1}, {2, 1}, {0, 2}},
                                     {{0, 0}, {1, 0}, {1, 1}, {1, 2}}}};
} // namespace

main::Game::Game() {
  handlers_["body"] = [this](const char *command, const char *) {
    if (std::strcmp(command, "ready") == 0)
      Setup();
    else if (std::strcmp(command, "back") == 0)
      Escape();
  };
  handlers_["game"] = [this](const char *command, const char *info) {
    if (std::strcmp(command, "action") == 0)
      HandleAction(info);
    else if (std::strcmp(command, "render") == 0)
      Render();
    else if (std::strcmp(command, "audio") == 0)
      PlayAudio(info);
  };

  if (!RestoreData()) {
    board_ = {};
    data_.score_ = 0;
    data_.lines_ = 0;
    paused_ = true;
    game_over_ = false;
    next_piece_ = RandomPiece();
    SpawnPiece();
    SyncData();
  }
  bridge::LoadView(index_,
                   static_cast<std::int32_t>(core::VIEW_INFO::Landscape) |
                       static_cast<std::int32_t>(core::VIEW_INFO::ScreenOn) |
                       static_cast<std::int32_t>(core::VIEW_INFO::AudioNoSolo),
                   "game");
}

main::Game::~Game() {
  {
    std::lock_guard<std::mutex> guard(lock_);
    SyncData();
    run_ = false;
  }
  waiter_.notify_all();
  if (worker_.joinable())
    worker_.join();
}

void main::Game::Setup() {
  bridge::CallFunction("setup()");
  Render();
  Run();
}

void main::Game::Run() {
  if (worker_.joinable())
    return;
  worker_ = std::thread([this] {
    std::unique_lock<std::mutex> guard(lock_);
    while (run_) {
      // Input may notify the condition variable, but only elapsed time advances
      // gravity. The predicate prevents player actions from becoming extra
      // ticks.
      if (waiter_.wait_for(guard, std::chrono::milliseconds(50),
                           [this] { return !run_; }))
        break;
      if (paused_ || game_over_)
        continue;
      if (++frame_ >= GravityFrames()) {
        frame_ = 0;
        Step();
        bridge::AsyncMessage(index_, "game", "render", "");
      }
    }
  });
}

void main::Game::Step() {
  if (!Move(0, 1))
    LockPiece();
}

void main::Game::HandleAction(const char *action) {
  bool changed = false;
  const char *sound = nullptr;
  {
    std::lock_guard<std::mutex> guard(lock_);
    if (std::strcmp(action, "back") == 0) {
      // Escape outside the lock because destroying this stage joins the worker.
    } else if (std::strcmp(action, "pause") == 0) {
      if (!game_over_) {
        paused_ = !paused_;
        frame_ = 0;
        changed = true;
      }
    } else if (std::strcmp(action, "restart") == 0) {
      board_ = {};
      data_.score_ = 0;
      data_.lines_ = 0;
      paused_ = false;
      game_over_ = false;
      next_piece_ = RandomPiece();
      SpawnPiece();
      changed = true;
    } else if (!paused_ && !game_over_) {
      if (std::strcmp(action, "left") == 0) {
        changed = Move(-1, 0);
        sound = changed ? "move" : nullptr;
      } else if (std::strcmp(action, "right") == 0) {
        changed = Move(1, 0);
        sound = changed ? "move" : nullptr;
      } else if (std::strcmp(action, "rotate") == 0) {
        changed = Rotate();
        sound = changed ? "turn" : nullptr;
      } else if (std::strcmp(action, "down") == 0) {
        changed = Move(0, 1);
        if (changed) {
          ++data_.score_;
          sound = "move";
        }
      } else if (std::strcmp(action, "drop") == 0) {
        HardDrop();
        changed = true;
      }
    }
  }
  if (std::strcmp(action, "back") == 0) {
    Escape();
    return;
  }
  if (changed)
    Render();
  if (sound && data_.sound_)
    bridge::AsyncMessage(index_, "game", "audio", sound);
}

bool main::Game::Fits(int type, int rotation, int x, int y) const {
  for (const auto &block : shapes[type][rotation]) {
    const int bx = x + block[0];
    const int by = y + block[1];
    if (bx < 0 || bx >= width_ || by < 0 || by >= height_ || board_[by][bx])
      return false;
  }
  return true;
}

bool main::Game::Move(int dx, int dy) {
  if (!Fits(piece_, rotation_, piece_x_ + dx, piece_y_ + dy))
    return false;
  piece_x_ += dx;
  piece_y_ += dy;
  return true;
}

bool main::Game::Rotate() {
  const int next = (rotation_ + 1) % 4;
  for (int kick : {0, -1, 1, -2, 2}) {
    if (Fits(piece_, next, piece_x_ + kick, piece_y_)) {
      rotation_ = next;
      piece_x_ += kick;
      return true;
    }
  }
  return false;
}

void main::Game::HardDrop() {
  int distance = 0;
  while (Move(0, 1))
    ++distance;
  data_.score_ += distance * 2;
  LockPiece();
}

void main::Game::LockPiece() {
  for (const auto &block : shapes[piece_][rotation_]) {
    const int y = piece_y_ + block[1];
    board_[y][piece_x_ + block[0]] = piece_ + 1;
  }
  ClearLines();
  const bool above_visible_board = std::any_of(
      board_.begin(), board_.begin() + hidden_rows_, [](const auto &row) {
        return std::any_of(row.begin(), row.end(),
                           [](int cell) { return cell != 0; });
      });
  if (above_visible_board) {
    game_over_ = true;
    if (data_.sound_)
      bridge::AsyncMessage(index_, "game", "audio", "die");
    return;
  }
  SpawnPiece();
  if (data_.sound_)
    bridge::AsyncMessage(index_, "game", "audio", game_over_ ? "die" : "turn");
}

void main::Game::ClearLines() {
  int cleared = 0;
  for (int y = height_ - 1; y >= 0;) {
    if (std::all_of(board_[y].begin(), board_[y].end(),
                    [](int cell) { return cell != 0; })) {
      for (int row = y; row > 0; --row)
        board_[row] = board_[row - 1];
      board_[0].fill(0);
      ++cleared;
    } else {
      --y;
    }
  }
  if (!cleared)
    return;
  static constexpr int points[] = {0, 100, 300, 500, 800};
  data_.score_ += points[cleared] * Level();
  data_.lines_ += cleared;
  if (data_.sound_)
    bridge::AsyncMessage(index_, "game", "audio",
                         cleared == 4 ? "win" : "food");
}

void main::Game::SpawnPiece() {
  piece_ = next_piece_;
  next_piece_ = RandomPiece();
  rotation_ = 0;
  piece_x_ = 3;
  piece_y_ = hidden_rows_ - 2;
  frame_ = 0;
  if (!Fits(piece_, rotation_, piece_x_, piece_y_)) {
    game_over_ = true;
  }
}

int main::Game::RandomPiece() {
  if (bag_index_ == bag_.size()) {
    std::shuffle(bag_.begin(), bag_.end(), random_);
    bag_index_ = 0;
  }
  return bag_[bag_index_++];
}

int main::Game::Level() const { return data_.lines_ / 10 + 1; }

int main::Game::GravityFrames() const { return std::max(2, 18 - Level()); }

bool main::Game::RestoreData() {
  if (!data_.game_initialized_)
    return false;
  board_ = data_.board_;
  piece_ = data_.piece_;
  next_piece_ = data_.next_piece_;
  rotation_ = data_.rotation_;
  piece_x_ = data_.piece_x_;
  piece_y_ = data_.piece_y_;
  paused_ = true;
  game_over_ = data_.game_over_;
  return game_over_ || Fits(piece_, rotation_, piece_x_, piece_y_);
}

void main::Game::SyncData() {
  data_.game_initialized_ = true;
  data_.board_ = board_;
  data_.piece_ = piece_;
  data_.next_piece_ = next_piece_;
  data_.rotation_ = rotation_;
  data_.piece_x_ = piece_x_;
  data_.piece_y_ = piece_y_;
  data_.paused_ = paused_;
  data_.game_over_ = game_over_;
}

std::string main::Game::BoardState() const {
  auto visible = board_;
  if (!game_over_) {
    for (const auto &block : shapes[piece_][rotation_])
      visible[piece_y_ + block[1]][piece_x_ + block[0]] = piece_ + 1;
  }
  std::string state;
  state.reserve(width_ * visible_height_);
  for (int y = hidden_rows_; y < height_; ++y)
    for (int cell : visible[y])
      state.push_back(static_cast<char>('0' + cell));
  return state;
}

std::string main::Game::NextState() const {
  std::string state(16, '0');
  for (const auto &block : shapes[next_piece_][0])
    state[block[1] * 4 + block[0]] = static_cast<char>('1' + next_piece_);
  return state;
}

void main::Game::Render() {
  std::string board;
  std::string next;
  int score;
  int lines;
  int level;
  bool paused;
  bool game_over;
  {
    std::lock_guard<std::mutex> guard(lock_);
    SyncData();
    board = BoardState();
    next = NextState();
    score = data_.score_;
    lines = data_.lines_;
    level = Level();
    paused = paused_;
    game_over = game_over_;
  }
  std::ostringstream js;
  js << "renderGame('" << board << "','" << next << "'," << score << ','
     << lines << ',' << level << ',' << (paused ? "true" : "false") << ','
     << (game_over ? "true" : "false") << ')';
  bridge::CallFunction(js.str().c_str());
}

void main::Game::PlayAudio(const char *audio) {
  std::ostringstream js;
  js << "playAudio('" << audio << "');";
  bridge::CallFunction(js.str().c_str());
}

void main::Game::Escape() {
  progress_ = PROGRESS::MENU;
  bridge::NeedRestart();
}

void main::Game::FeedUri(
    const char *, std::function<void(const std::vector<unsigned char> &)> &&) {}
