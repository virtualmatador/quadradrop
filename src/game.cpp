#include "game.h"

#include <algorithm>
#include <cstring>
#include <sstream>

#include "data.h"
#include "progress.h"

namespace {
constexpr int explosion_columns[10] = {4, 5, 3, 6, 2, 7, 1, 8, 0, 9};

enum class GameplayAction {
  UNKNOWN,
  LEFT,
  RIGHT,
  DOWN,
  ROTATE_LEFT,
  ROTATE_RIGHT,
  DROP,
  EXPLODE,
};

GameplayAction ParseGameplayAction(const char *action) {
  if (std::strcmp(action, "left") == 0)
    return GameplayAction::LEFT;
  if (std::strcmp(action, "right") == 0)
    return GameplayAction::RIGHT;
  if (std::strcmp(action, "down") == 0)
    return GameplayAction::DOWN;
  if (std::strcmp(action, "rotate-left") == 0)
    return GameplayAction::ROTATE_LEFT;
  if (std::strcmp(action, "rotate") == 0 ||
      std::strcmp(action, "rotate-right") == 0)
    return GameplayAction::ROTATE_RIGHT;
  if (std::strcmp(action, "drop") == 0)
    return GameplayAction::DROP;
  if (std::strcmp(action, "explode") == 0)
    return GameplayAction::EXPLODE;
  return GameplayAction::UNKNOWN;
}
} // namespace

main::Game::Game() {
  handlers_["body"] = [this](const char *command, const char *) {
    if (std::strcmp(command, "ready") == 0)
      Setup();
    else if (std::strcmp(command, "setup") == 0) {
      Render();
      Run();
    } else if (std::strcmp(command, "back") == 0)
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

  data_.paused_ = true;
  if (!ValidateData())
    data_.Restart();
}

main::Game::~Game() {
  {
    std::lock_guard<std::mutex> guard(lock_);
    run_ = false;
  }
  waiter_.notify_all();
  if (worker_.joinable())
    worker_.join();
  if (screen_on_)
    bridge::SetScreenOn(false);
}

void main::Game::Attach() {
  bridge::SetAudioNoSolo(true);
  bridge::SetLayout(true, false);
  bridge::LoadView(Index(), "game");
}

void main::Game::Suspend() {
  const bool pause_changed = SetPaused(true);
  bool update_screen_on = false;
  {
    std::lock_guard<std::mutex> guard(lock_);
    update_screen_on = screen_on_;
    screen_on_ = false;
  }
  if (update_screen_on)
    bridge::SetScreenOn(false);
  if (pause_changed)
    bridge::AsyncMessage(Index(), "game", "render", "");
}

void main::Game::Setup() {
  std::ostringstream js;
  js << "setup(" << (data_.show_controls_ ? "true" : "false") << ")";
  bridge::CallFunction(js.str().c_str());
}

void main::Game::Run() {
  if (worker_.joinable())
    return;
  worker_ = std::thread([this] {
    std::unique_lock<std::mutex> guard(lock_);
    while (run_) {
      waiter_.wait(guard, [this] {
        return !run_ || (!data_.paused_ && !data_.game_over_);
      });
      if (!run_)
        break;

      if (waiter_.wait_for(guard, std::chrono::milliseconds(50), [this] {
            return !run_ || data_.paused_ || data_.game_over_;
          }))
        continue;
      ++frame_;
      if (data_.cleanup_phase_) {
        if (frame_ >= 10) {
          frame_ = 0;
          Cleanup();
          bridge::AsyncMessage(Index(), "game", "render", "");
        }
      } else {
        if (frame_ >= GravityFrames()) {
          frame_ = 0;
          const char *sound = Step();
          const bool is_step_sound = std::strcmp(sound, "step") == 0;
          if ((is_step_sound && data_.step_sound_) ||
              (!is_step_sound && data_.action_sound_))
            bridge::AsyncMessage(Index(), "game", "audio", sound);
          bridge::AsyncMessage(Index(), "game", "render", "");
        }
      }
    }
  });
}

bool main::Game::SetPaused(bool paused) {
  bool changed = false;
  {
    std::lock_guard<std::mutex> guard(lock_);
    if (!data_.game_over_ && data_.paused_ != paused) {
      data_.paused_ = paused;
      frame_ = 0;
      changed = true;
    }
  }
  if (changed)
    waiter_.notify_all();
  return changed;
}

const char *main::Game::Step() {
  if (!Move(0, 1))
    return LockPiece();
  return "step";
}

void main::Game::HandleAction(const char *action) {
  if (std::strcmp(action, "back") == 0) {
    Escape();
    return;
  }
  if (std::strcmp(action, "restart") == 0) {
    bool restarted = false;
    {
      std::lock_guard<std::mutex> guard(lock_);
      if (data_.game_over_) {
        data_.Restart();
        restarted = true;
      }
    }
    if (restarted)
      Render();
    return;
  }
  if (std::strcmp(action, "pause") == 0) {
    bool paused;
    {
      std::lock_guard<std::mutex> guard(lock_);
      paused = data_.paused_;
    }
    if (SetPaused(!paused))
      Render();
    return;
  }

  bool changed = false;
  bool became_game_over = false;
  const char *sound = nullptr;
  const GameplayAction gameplay_action = ParseGameplayAction(action);
  {
    std::lock_guard<std::mutex> guard(lock_);
    const bool was_game_over = data_.game_over_;
    if (!data_.paused_ && !data_.game_over_ &&
        gameplay_action != GameplayAction::UNKNOWN) {
      if (data_.cleanup_phase_) {
        sound = "invalid";
      } else if (gameplay_action == GameplayAction::LEFT) {
        changed = Move(-1, 0);
        sound = changed ? "move" : "invalid";
      } else if (gameplay_action == GameplayAction::RIGHT) {
        changed = Move(1, 0);
        sound = changed ? "move" : "invalid";
      } else if (gameplay_action == GameplayAction::ROTATE_RIGHT) {
        changed = Rotate(1);
        sound = changed ? "turn" : "invalid";
      } else if (gameplay_action == GameplayAction::ROTATE_LEFT) {
        changed = Rotate(-1);
        sound = changed ? "turn" : "invalid";
      } else if (gameplay_action == GameplayAction::DOWN) {
        changed = Move(0, 1);
        if (changed) {
          ++data_.score_;
          sound = "move";
        } else {
          sound = LockPiece();
          changed = true;
        }
        frame_ = 0;
      } else if (gameplay_action == GameplayAction::DROP) {
        sound = HardDrop();
        changed = true;
        frame_ = 0;
      } else if (gameplay_action == GameplayAction::EXPLODE) {
        if (data_.quadras_ > 0) {
          sound = Explode();
          changed = true;
          frame_ = 0;
        } else {
          sound = "invalid";
        }
      }
    }
    became_game_over = !was_game_over && data_.game_over_;
  }
  if (became_game_over)
    waiter_.notify_all();
  if (changed)
    Render();
  if (sound && data_.action_sound_)
    PlayAudio(sound);
}

bool main::Game::Move(int dx, int dy) {
  if (!data_.Fits(data_.piece_, data_.rotation_, data_.piece_x_ + dx,
                  data_.piece_y_ + dy))
    return false;
  data_.piece_x_ += dx;
  data_.piece_y_ += dy;
  return true;
}

bool main::Game::Rotate(int direction) {
  const int rotation = (data_.rotation_ + direction + 4) % 4;
  if (data_.Fits(data_.piece_, rotation, data_.piece_x_, data_.piece_y_)) {
    data_.rotation_ = rotation;
    return true;
  }

  constexpr int kick_directions[] = {-1, 1};
  bool kick_path_clear[] = {true, true};
  for (int count = 1; count <= 2; ++count) {
    for (int index = 0; index < 2; ++index) {
      if (!kick_path_clear[index])
        continue;
      const int x = data_.piece_x_ + count * kick_directions[index] * direction;
      if (!data_.Fits(data_.piece_, data_.rotation_, x, data_.piece_y_)) {
        kick_path_clear[index] = false;
        continue;
      }
      if (data_.Fits(data_.piece_, rotation, x, data_.piece_y_)) {
        data_.piece_x_ = x;
        data_.rotation_ = rotation;
        return true;
      }
    }
  }
  return false;
}

const char *main::Game::HardDrop() {
  int distance = 0;
  while (Move(0, 1))
    ++distance;
  data_.score_ += distance * 2;
  return LockPiece();
}

const char *main::Game::Explode() {
  for (const auto &block : Data::shapes_[data_.piece_][data_.rotation_]) {
    const int y = data_.piece_y_ + block[1];
    data_.board_[y][data_.piece_x_ + block[0]] = data_.piece_ + 1;
  }
  --data_.quadras_;
  data_.cleanup_phase_ = Data::CLEANUP_EXPLODE;
  data_.cleanup_row_ = 0;
  data_.cleanup_count_ = 0;
  data_.explosion_targets_.fill(-1);
  return "lock";
}

const char *main::Game::LockPiece() {
  for (const auto &block : Data::shapes_[data_.piece_][data_.rotation_]) {
    const int y = data_.piece_y_ + block[1];
    data_.board_[y][data_.piece_x_ + block[0]] = data_.piece_ + 1;
  }
  data_.cleanup_count_ = 0;
  const int row = FindFullRow();
  if (row >= 0) {
    data_.cleanup_row_ = row;
    data_.cleanup_phase_ = Data::CLEANUP_START;
  } else {
    data_.SpawnPiece();
  }
  return data_.game_over_ ? "die" : "lock";
}

std::array<int, 4> main::Game::ExplosionSources() const {
  std::array<std::array<bool, width_>, height_> is_source{};
  for (const auto &block : Data::shapes_[data_.piece_][data_.rotation_]) {
    const int x = data_.piece_x_ + block[0];
    const int y = data_.piece_y_ + block[1];
    is_source[y][x] = true;
  }

  std::array<int, 4> sources{};
  int source_count = 0;
  for (int y = height_ - 1; y >= 0; --y) {
    for (int x : explosion_columns) {
      if (is_source[y][x])
        sources[source_count++] = y * width_ + x;
    }
  }
  return sources;
}

std::string main::Game::ExplosionMoves() const {
  const auto sources = ExplosionSources();
  std::ostringstream moves;
  for (std::size_t i = 0; i < sources.size(); ++i) {
    if (i)
      moves << ';';
    moves << sources[i] << ',' << data_.explosion_targets_[i];
  }
  return moves.str();
}

void main::Game::ApplyExplosion() {
  const auto sources = ExplosionSources();
  std::array<std::array<bool, width_>, height_> is_source{};
  for (int source : sources)
    is_source[source / width_][source % width_] = true;

  int target_count = 0;
  for (int y = height_ - 1; y >= 0 && target_count < 4; --y) {
    for (int x : explosion_columns) {
      if (target_count == 4)
        break;
      if (!data_.board_[y][x] || is_source[y][x])
        data_.explosion_targets_[target_count++] = y * width_ + x;
    }
  }

  for (int source : sources)
    data_.board_[source / width_][source % width_] = 0;

  for (std::size_t i = 0; i < sources.size(); ++i) {
    const int target = data_.explosion_targets_[i];
    data_.board_[target / width_][target % width_] = data_.piece_ + 1;
  }
}

int main::Game::FindFullRow() const {
  for (int y = height_ - 1; y >= hidden_rows_; --y) {
    if (std::all_of(data_.board_[y].begin(), data_.board_[y].end(),
                    [](int cell) { return cell != 0; }))
      return y;
  }
  return -1;
}

void main::Game::Cleanup() {
  switch (data_.cleanup_phase_) {
  case Data::CLEANUP_EXPLODE:
    ApplyExplosion();
    data_.cleanup_phase_ = Data::CLEANUP_EXPLODING;
    if (data_.action_sound_)
      bridge::AsyncMessage(Index(), "game", "audio", "explode");
    break;

  case Data::CLEANUP_EXPLODING:
    data_.explosion_targets_.fill(-1);
    if (const char *sound = ResolveBoard(); sound && data_.action_sound_)
      bridge::AsyncMessage(Index(), "game", "audio", sound);
    break;

  case Data::CLEANUP_START:
    data_.cleanup_phase_ = Data::CLEANUP_CLEARING;
    if (data_.action_sound_)
      bridge::AsyncMessage(Index(), "game", "audio", "food");
    break;

  case Data::CLEANUP_CLEARING:
    for (int row = data_.cleanup_row_; row > 0; --row)
      data_.board_[row] = data_.board_[row - 1];
    data_.board_[0].fill(0);
    data_.score_ += (data_.cleanup_count_ + 1) * 100 * data_.Level();
    ++data_.cleanup_count_;
    if (const int previous_level = data_.Level();
        ++data_.lines_, data_.Level() > previous_level) {
      data_.cleanup_phase_ = Data::CLEANUP_LEVEL_CHANGE;
      if (data_.action_sound_)
        bridge::AsyncMessage(Index(), "game", "audio", "level");
      break;
    }
    if (const char *sound = ResolveBoard(); sound && data_.action_sound_)
      bridge::AsyncMessage(Index(), "game", "audio", sound);
    break;

  case Data::CLEANUP_LEVEL_CHANGE:
    if (const char *sound = ResolveBoard(); sound && data_.action_sound_)
      bridge::AsyncMessage(Index(), "game", "audio", sound);
    break;

  case Data::CLEANUP_WIN:
    data_.cleanup_phase_ = Data::CLEANUP_PLAYING;
    data_.cleanup_count_ = 0;
    data_.SpawnPiece();
    if (data_.game_over_ && data_.action_sound_)
      bridge::AsyncMessage(Index(), "game", "audio", "die");
    break;

  case Data::CLEANUP_PLAYING:
    break;

  default:
    break;
  }
}

const char *main::Game::ResolveBoard() {
  const int row = FindFullRow();
  if (row >= 0) {
    data_.cleanup_row_ = row;
    data_.cleanup_phase_ = Data::CLEANUP_CLEARING;
    return "food";
  }
  if (data_.cleanup_count_ > 3) {
    ++data_.quadras_;
    data_.cleanup_phase_ = Data::CLEANUP_WIN;
    return "win";
  }
  data_.cleanup_phase_ = Data::CLEANUP_PLAYING;
  data_.cleanup_count_ = 0;
  data_.SpawnPiece();
  if (data_.game_over_)
    return "die";
  return nullptr;
}

bool main::Game::ValidNextPiece() const {
  for (const auto &block :
       Data::shapes_[data_.next_piece_][data_.next_rotation_]) {
    const int x = data_.next_piece_x_ + block[0];
    const int y = data_.next_piece_y_ + block[1];
    if (x < 3 || x > 6 || y < 0 || y > hidden_rows_)
      return false;
  }
  return std::any_of(Data::shapes_[data_.next_piece_][data_.next_rotation_],
                     Data::shapes_[data_.next_piece_][data_.next_rotation_] + 4,
                     [this](const int (&block)[2]) {
                       return data_.next_piece_y_ + block[1] == hidden_rows_;
                     });
}

int main::Game::GravityFrames() const {
  return std::max(2, 20 - data_.Level());
}

bool main::Game::ValidateData() const {
  if (!ValidNextPiece())
    return false;
  const bool has_explosion_targets = std::any_of(
      data_.explosion_targets_.begin(), data_.explosion_targets_.end(),
      [](int target) { return target != -1; });
  if (data_.cleanup_phase_) {
    if (data_.cleanup_phase_ < Data::CLEANUP_PLAYING ||
        data_.cleanup_phase_ >= Data::CLEANUP_PHASE_COUNT || data_.game_over_)
      return false;

    if (data_.cleanup_phase_ == Data::CLEANUP_EXPLODE) {
      if (data_.cleanup_row_ != 0 || data_.cleanup_count_ != 0 ||
          has_explosion_targets)
        return false;
      for (const auto &block : Data::shapes_[data_.piece_][data_.rotation_]) {
        const int x = data_.piece_x_ + block[0];
        const int y = data_.piece_y_ + block[1];
        if (x < 0 || x >= width_ || y < 0 || y >= height_ ||
            data_.board_[y][x] != data_.piece_ + 1)
          return false;
      }
      return true;
    }

    if (data_.cleanup_phase_ == Data::CLEANUP_EXPLODING) {
      if (data_.cleanup_row_ != 0 || data_.cleanup_count_ != 0)
        return false;

      auto settled = data_.board_;
      std::array<bool, width_ * height_> used_targets{};
      for (int target : data_.explosion_targets_) {
        if (target < 0 || target >= width_ * height_ || used_targets[target] ||
            settled[target / width_][target % width_] != data_.piece_ + 1)
          return false;
        used_targets[target] = true;
        settled[target / width_][target % width_] = 0;
      }

      for (const auto &block : Data::shapes_[data_.piece_][data_.rotation_]) {
        const int x = data_.piece_x_ + block[0];
        const int y = data_.piece_y_ + block[1];
        if (x < 0 || x >= width_ || y < 0 || y >= height_ || settled[y][x])
          return false;
      }

      int expected = 0;
      for (int y = height_ - 1; y >= 0 && expected < 4; --y) {
        for (int x : explosion_columns) {
          if (!settled[y][x]) {
            if (data_.explosion_targets_[expected] != y * width_ + x)
              return false;
            if (++expected == 4)
              break;
          }
        }
      }
      return expected == 4;
    }

    return !has_explosion_targets && data_.cleanup_row_ >= hidden_rows_ &&
           data_.cleanup_row_ < height_;
  }
  if (has_explosion_targets)
    return false;
  return data_.game_over_ || data_.Fits(data_.piece_, data_.rotation_,
                                        data_.piece_x_, data_.piece_y_);
}

std::string main::Game::BoardState() const {
  auto visible = data_.board_;
  if (!data_.game_over_ && !data_.cleanup_phase_) {
    for (const auto &block : Data::shapes_[data_.piece_][data_.rotation_])
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
  for (const auto &block : Data::shapes_[data_.piece_][data_.rotation_]) {
    const int x = data_.piece_x_ + block[0];
    const int y = data_.piece_y_ + block[1] - hidden_rows_;
    if (y >= 0 && y < visible_height_)
      state[y * width_ + x] = '1';
  }
  return state;
}

std::string main::Game::NextState() const {
  return PreviewState(data_.next_piece_, data_.next_rotation_,
                      data_.next_piece_x_, data_.next_piece_y_);
}

std::string main::Game::PreviewState(int type, int rotation, int x,
                                     int y) const {
  std::string state(16, '0');
  for (const auto &block : Data::shapes_[type][rotation]) {
    const int preview_x = x + block[0] - 3;
    const int preview_y = y + block[1] - (hidden_rows_ - 3);
    if (preview_x >= 0 && preview_x < 4 && preview_y >= 0 && preview_y < 4)
      state[preview_y * 4 + preview_x] = static_cast<char>('1' + type);
  }
  return state;
}

void main::Game::Render() {
  std::string board;
  std::string active;
  std::string next;
  int cleanup_phase;
  int cleanup_row;
  int cleanup_count;
  std::string explosion_moves;
  int score;
  int lines;
  int level;
  int quadras;
  unsigned int piece_generation;
  bool paused;
  bool game_over;
  bool screen_on;
  bool update_screen_on;
  {
    std::lock_guard<std::mutex> guard(lock_);
    board = BoardState();
    active = ActiveState();
    next = NextState();
    cleanup_phase = data_.cleanup_phase_;
    cleanup_row = data_.cleanup_row_ - hidden_rows_;
    cleanup_count = data_.cleanup_count_;
    if (cleanup_phase == Data::CLEANUP_EXPLODING)
      explosion_moves = ExplosionMoves();
    score = data_.score_;
    lines = data_.lines_;
    level = data_.Level();
    quadras = data_.quadras_;
    piece_generation = data_.piece_generation_;
    paused = data_.paused_;
    game_over = data_.game_over_;
    screen_on = !paused && !game_over;
    update_screen_on = screen_on_ != screen_on;
    screen_on_ = screen_on;
  }
  if (update_screen_on)
    bridge::SetScreenOn(screen_on);
  std::ostringstream js;
  js << "renderGame('" << board << "','" << active << "','" << next << "',"
     << score << ',' << lines << ',' << level << ',' << quadras << ','
     << piece_generation << ',' << (paused ? "true" : "false") << ','
     << (game_over ? "true" : "false") << ',' << cleanup_phase << ','
     << cleanup_row << ',' << cleanup_count << ",'" << explosion_moves << "')";
  bridge::CallFunction(js.str().c_str());
}

void main::Game::PlayAudio(const char *audio) {
  std::ostringstream js;
  js << "playAudio('" << audio << "');";
  bridge::CallFunction(js.str().c_str());
}

void main::Game::Escape() {
  progress_ = PROGRESS::MENU;
  RequestStage();
}

void main::Game::FeedUri(
    const char *, std::function<void(const std::vector<unsigned char> &)> &&) {}
