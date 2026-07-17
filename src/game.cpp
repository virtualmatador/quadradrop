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

  if (data_.game_initialized_)
    data_.paused_ = true;
  if (!ValidateData()) {
    data_.board_ = {};
    data_.score_ = 0;
    data_.lines_ = 0;
    data_.paused_ = true;
    data_.game_over_ = false;
    data_.next_piece_ = RandomPiece();
    SpawnPiece();
    data_.game_initialized_ = true;
  } else if (!data_.game_over_ && !data_.cleanup_phase_ &&
             !PieceEnteredView())
    preview_pending_ = true;
  bridge::LoadView(index_,
                   static_cast<std::int32_t>(core::VIEW_INFO::ScreenOn) |
                       static_cast<std::int32_t>(core::VIEW_INFO::AudioNoSolo),
                   "game");
}

main::Game::~Game() {
  {
    std::lock_guard<std::mutex> guard(lock_);
    run_ = false;
  }
  waiter_.notify_all();
  if (worker_.joinable())
    worker_.join();
}

void main::Game::Setup() {
  std::ostringstream js;
  js << "setup(" << (data_.show_controls_ ? "true" : "false") << ")";
  bridge::CallFunction(js.str().c_str());
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
      if (data_.paused_ || data_.game_over_)
        continue;
      if (data_.cleanup_phase_) {
        if (++frame_ >= 5) {
          frame_ = 0;
          AdvanceCleanup();
          bridge::AsyncMessage(index_, "game", "render", "");
        }
        continue;
      }
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
      if (!data_.game_over_) {
        data_.paused_ = !data_.paused_;
        frame_ = 0;
        changed = true;
      }
    } else if (std::strcmp(action, "restart") == 0) {
      data_.board_ = {};
      data_.score_ = 0;
      data_.lines_ = 0;
      data_.paused_ = false;
      data_.game_over_ = false;
      data_.cleanup_phase_ = 0;
      data_.cleanup_count_ = 0;
      data_.next_piece_ = RandomPiece();
      SpawnPiece();
      changed = true;
    } else if (!data_.paused_ && !data_.game_over_ &&
               !data_.cleanup_phase_) {
      if (std::strcmp(action, "left") == 0) {
        changed = Move(-1, 0);
        sound = changed ? "move" : nullptr;
      } else if (std::strcmp(action, "right") == 0) {
        changed = Move(1, 0);
        sound = changed ? "move" : nullptr;
      } else if (std::strcmp(action, "rotate") == 0 ||
                 std::strcmp(action, "rotate-right") == 0) {
        changed = Rotate(1);
        sound = changed ? "turn" : nullptr;
      } else if (std::strcmp(action, "rotate-left") == 0) {
        changed = Rotate(-1);
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
    if (bx < 0 || bx >= width_ || by < 0 || by >= height_ ||
        data_.board_[by][bx])
      return false;
  }
  return true;
}

bool main::Game::Move(int dx, int dy) {
  if (!Fits(data_.piece_, data_.rotation_, data_.piece_x_ + dx,
            data_.piece_y_ + dy))
    return false;
  data_.piece_x_ += dx;
  data_.piece_y_ += dy;
  UpdatePreview();
  return true;
}

bool main::Game::Rotate(int direction) {
  const int next = (data_.rotation_ + direction + 4) % 4;
  for (int kick : {0, -1, 1, -2, 2}) {
    if (Fits(data_.piece_, next, data_.piece_x_ + kick, data_.piece_y_)) {
      data_.rotation_ = next;
      data_.piece_x_ += kick;
      UpdatePreview();
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
  for (const auto &block : shapes[data_.piece_][data_.rotation_]) {
    const int y = data_.piece_y_ + block[1];
    data_.board_[y][data_.piece_x_ + block[0]] = data_.piece_ + 1;
  }
  if (!BeginCleanup())
    SpawnPiece();
  if (data_.sound_)
    bridge::AsyncMessage(index_, "game", "audio",
                         data_.game_over_ ? "die" : "lock");
}

int main::Game::FindFullRow() const {
  for (int y = height_ - 1; y >= hidden_rows_; --y) {
    if (std::all_of(data_.board_[y].begin(), data_.board_[y].end(),
                    [](int cell) { return cell != 0; }))
      return y;
  }
  return -1;
}

bool main::Game::BeginCleanup() {
  const int row = FindFullRow();
  if (row < 0)
    return false;
  data_.cleanup_phase_ = 1;
  data_.cleanup_row_ = row;
  data_.cleanup_count_ = 0;
  frame_ = 0;
  return true;
}

void main::Game::AdvanceCleanup() {
  if (data_.cleanup_phase_ == 1) {
    data_.board_[data_.cleanup_row_].fill(0);
    data_.cleanup_phase_ = 2;
    if (data_.sound_)
      bridge::AsyncMessage(index_, "game", "audio", "food");
    return;
  }
  for (int row = data_.cleanup_row_; row > 0; --row)
    data_.board_[row] = data_.board_[row - 1];
  data_.board_[0].fill(0);
  ++data_.cleanup_count_;
  ++data_.lines_;
  const int row = FindFullRow();
  if (row >= 0) {
    data_.cleanup_row_ = row;
    data_.cleanup_phase_ = 1;
    return;
  }
  static constexpr int points[] = {0, 100, 300, 500, 800};
  const int cleared = std::min(data_.cleanup_count_, 4);
  const int cleanup_level = (data_.lines_ - data_.cleanup_count_) / 10 + 1;
  data_.score_ += points[cleared] * cleanup_level;
  data_.cleanup_phase_ = 0;
  data_.cleanup_count_ = 0;
  SpawnPiece();
  if (data_.sound_ && cleared == 4)
    bridge::AsyncMessage(index_, "game", "audio", "win");
}

void main::Game::SpawnPiece() {
  data_.piece_ = data_.next_piece_;
  data_.rotation_ = 0;
  data_.piece_x_ = 3;
  data_.piece_y_ = hidden_rows_ - 2;
  preview_pending_ = true;
  frame_ = 0;
  if (!Fits(data_.piece_, data_.rotation_, data_.piece_x_, data_.piece_y_)) {
    data_.game_over_ = true;
  }
}

bool main::Game::PieceEnteredView() const {
  return std::any_of(shapes[data_.piece_][data_.rotation_],
                     shapes[data_.piece_][data_.rotation_] + 4,
                     [this](const int (&block)[2]) {
                       return data_.piece_y_ + block[1] >= hidden_rows_;
                     });
}

void main::Game::UpdatePreview() {
  if (preview_pending_ && PieceEnteredView()) {
    data_.next_piece_ = RandomPiece();
    preview_pending_ = false;
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

bool main::Game::ValidateData() const {
  if (!data_.game_initialized_)
    return false;
  if (data_.cleanup_phase_)
    return data_.cleanup_phase_ <= 2 && data_.cleanup_row_ >= 0 &&
           data_.cleanup_row_ < height_;
  return data_.game_over_ ||
         Fits(data_.piece_, data_.rotation_, data_.piece_x_, data_.piece_y_);
}

std::string main::Game::BoardState() const {
  auto visible = data_.board_;
  if (!data_.game_over_ && !data_.cleanup_phase_) {
    for (const auto &block : shapes[data_.piece_][data_.rotation_])
      visible[data_.piece_y_ + block[1]][data_.piece_x_ + block[0]] =
          data_.piece_ + 1;
  }
  std::string state;
  state.reserve(width_ * visible_height_);
  for (int y = hidden_rows_; y < height_; ++y)
    for (int cell : visible[y])
      state.push_back(static_cast<char>('0' + cell));
  return state;
}

std::string main::Game::ActiveState() const {
  std::string state(width_ * visible_height_, '0');
  if (data_.game_over_ || data_.cleanup_phase_)
    return state;
  for (const auto &block : shapes[data_.piece_][data_.rotation_]) {
    const int x = data_.piece_x_ + block[0];
    const int y = data_.piece_y_ + block[1] - hidden_rows_;
    if (y >= 0 && y < visible_height_)
      state[y * width_ + x] = '1';
  }
  return state;
}

std::string main::Game::NextState() const {
  std::string state(16, '0');
  for (const auto &block : shapes[data_.next_piece_][0])
    state[block[1] * 4 + block[0]] = static_cast<char>('1' + data_.next_piece_);
  return state;
}

void main::Game::Render() {
  std::string board;
  std::string active;
  std::string next;
  int cleanup_phase;
  int cleanup_row;
  int score;
  int lines;
  int level;
  bool paused;
  bool game_over;
  {
    std::lock_guard<std::mutex> guard(lock_);
    board = BoardState();
    active = ActiveState();
    next = NextState();
    cleanup_phase = data_.cleanup_phase_;
    cleanup_row = data_.cleanup_row_ - hidden_rows_;
    score = data_.score_;
    lines = data_.lines_;
    level = Level();
    paused = data_.paused_;
    game_over = data_.game_over_;
  }
  std::ostringstream js;
  js << "renderGame('" << board << "','" << active << "','" << next << "',"
     << score << ',' << lines << ',' << level << ','
     << (paused ? "true" : "false") << ','
     << (game_over ? "true" : "false") << ',' << cleanup_phase << ','
     << cleanup_row << ')';
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
