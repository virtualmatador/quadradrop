#include "data.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <istream>
#include <ostream>

namespace {
constexpr std::int64_t LevelThreshold(int level) {
  if (level <= 1)
    return 0;
  const auto value = static_cast<std::int64_t>(level);
  return (3 * value * value - 5 * value + 4) / 2;
}

constexpr int QuadraticLevel(int lines) {
  constexpr int search_limit = 1 << 16;
  int first = 1;
  int last = search_limit;
  while (first + 1 < last) {
    const int middle = first + (last - first) / 2;
    if (LevelThreshold(middle) <= lines)
      first = middle;
    else
      last = middle;
  }
  return first;
}

} // namespace

const int main::Data::shapes_[7][4][4][2] = {
    {{{0, 1}, {1, 1}, {2, 1}, {3, 1}},
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

main::Data main::data_;

int main::Data::Level() const { return QuadraticLevel(lines_); }

void main::Data::Load(std::istream &input) {
  input >> std::dec >> std::noboolalpha >> std::skipws;
  int version = 0;
  if (!(input >> version)) {
    Reset();
    return;
  }

  if (version != save_version_) {
    if (Convert(version, input)) {
      incompatible_save_ = false;
      incompatible_save_version_ = save_version_;
      return;
    }

    Reset();
    incompatible_save_ = true;
    incompatible_save_version_ = version;
    return;
  }

  Data loaded;
  bool valid = static_cast<bool>(
      input >> loaded.score_ >> loaded.lines_ >> loaded.quadras_ >>
      loaded.action_sound_ >> loaded.step_sound_ >> loaded.show_controls_ >>
      loaded.theme_ >> loaded.piece_ >> loaded.next_piece_ >>
      loaded.next_rotation_ >> loaded.next_piece_x_ >> loaded.next_piece_y_ >>
      loaded.rotation_ >> loaded.piece_x_ >> loaded.piece_y_ >>
      loaded.paused_ >> loaded.game_over_ >> loaded.cleanup_phase_ >>
      loaded.cleanup_row_ >> loaded.cleanup_count_);
  for (std::size_t i = 0; valid && i < loaded.explosion_targets_.size(); ++i) {
    valid = static_cast<bool>(input >> loaded.explosion_targets_[i]);
  }
  for (auto &row : loaded.board_)
    for (int &cell : row)
      if (valid)
        valid = static_cast<bool>(input >> cell);
  if (valid) {
    input >> std::ws;
    valid = input.eof() && !input.bad();
  }

  valid = valid && loaded.score_ >= 0 && loaded.score_ <= score_max_ &&
          loaded.lines_ >= 0 && loaded.lines_ <= lines_max_ &&
          loaded.quadras_ >= 0 && loaded.quadras_ <= quadras_max_ &&
          loaded.theme_ >= THEME_SYSTEM && loaded.theme_ < THEME_COUNT &&
          loaded.piece_ >= 0 && loaded.piece_ < 7 && loaded.next_piece_ >= 0 &&
          loaded.next_piece_ < 7 && loaded.next_rotation_ >= 0 &&
          loaded.next_rotation_ < 4 && loaded.next_piece_x_ >= 1 &&
          loaded.next_piece_x_ < 6 && loaded.next_piece_y_ >= 1 &&
          loaded.next_piece_y_ < 4 && loaded.rotation_ >= 0 &&
          loaded.rotation_ < 4 && loaded.piece_x_ >= -2 &&
          loaded.piece_x_ < board_width_ && loaded.piece_y_ >= 0 &&
          loaded.piece_y_ < board_height_ &&
          loaded.cleanup_phase_ >= CLEANUP_PLAYING &&
          loaded.cleanup_phase_ < CLEANUP_PHASE_COUNT &&
          loaded.cleanup_row_ >= 0 && loaded.cleanup_row_ < board_height_ &&
          loaded.cleanup_count_ >= 0 && loaded.cleanup_count_ <= board_height_;
  for (std::size_t i = 0; valid && i < loaded.explosion_targets_.size(); ++i) {
    valid = loaded.explosion_targets_[i] >= -1 &&
            loaded.explosion_targets_[i] < board_width_ * board_height_;
  }
  for (const auto &row : loaded.board_)
    for (int cell : row)
      if (valid)
        valid = cell >= 0 && cell <= 7;
  if (!valid) {
    Reset();
    return;
  }

  score_ = loaded.score_;
  lines_ = loaded.lines_;
  quadras_ = loaded.quadras_;
  action_sound_ = loaded.action_sound_;
  step_sound_ = loaded.step_sound_;
  show_controls_ = loaded.show_controls_;
  theme_ = loaded.theme_;
  board_ = loaded.board_;
  piece_ = loaded.piece_;
  next_piece_ = loaded.next_piece_;
  next_rotation_ = loaded.next_rotation_;
  next_piece_x_ = loaded.next_piece_x_;
  next_piece_y_ = loaded.next_piece_y_;
  rotation_ = loaded.rotation_;
  piece_x_ = loaded.piece_x_;
  piece_y_ = loaded.piece_y_;
  paused_ = loaded.paused_;
  game_over_ = loaded.game_over_;
  cleanup_phase_ = loaded.cleanup_phase_;
  cleanup_row_ = loaded.cleanup_row_;
  cleanup_count_ = loaded.cleanup_count_;
  explosion_targets_ = loaded.explosion_targets_;
  incompatible_save_ = false;
  incompatible_save_version_ = save_version_;
}

bool main::Data::Convert(int version, std::istream &input) {
  switch (version) {
  case 1: {
    Data loaded;
    bool valid = static_cast<bool>(
        input >> loaded.score_ >> loaded.lines_ >> loaded.quadras_ >>
        loaded.action_sound_ >> loaded.step_sound_ >> loaded.show_controls_ >>
        loaded.piece_ >> loaded.next_piece_ >> loaded.next_rotation_ >>
        loaded.next_piece_x_ >> loaded.next_piece_y_ >> loaded.rotation_ >>
        loaded.piece_x_ >> loaded.piece_y_ >> loaded.paused_ >>
        loaded.game_over_ >> loaded.cleanup_phase_ >> loaded.cleanup_row_ >>
        loaded.cleanup_count_);
    for (std::size_t i = 0; valid && i < loaded.explosion_targets_.size();
         ++i) {
      valid = static_cast<bool>(input >> loaded.explosion_targets_[i]);
    }
    for (auto &row : loaded.board_)
      for (int &cell : row)
        if (valid)
          valid = static_cast<bool>(input >> cell);
    if (valid) {
      input >> std::ws;
      valid = input.eof() && !input.bad();
    }

    valid = valid && loaded.score_ >= 0 && loaded.score_ <= score_max_ &&
            loaded.lines_ >= 0 && loaded.lines_ <= lines_max_ &&
            loaded.quadras_ >= 0 && loaded.quadras_ <= quadras_max_ &&
            loaded.piece_ >= 0 && loaded.piece_ < 7 &&
            loaded.next_piece_ >= 0 && loaded.next_piece_ < 7 &&
            loaded.next_rotation_ >= 0 && loaded.next_rotation_ < 4 &&
            loaded.next_piece_x_ >= 1 && loaded.next_piece_x_ < 6 &&
            loaded.next_piece_y_ >= 1 && loaded.next_piece_y_ < 4 &&
            loaded.rotation_ >= 0 && loaded.rotation_ < 4 &&
            loaded.piece_x_ >= -2 && loaded.piece_x_ < board_width_ &&
            loaded.piece_y_ >= 0 && loaded.piece_y_ < board_height_ &&
            loaded.cleanup_phase_ >= CLEANUP_PLAYING &&
            loaded.cleanup_phase_ < CLEANUP_PHASE_COUNT &&
            loaded.cleanup_row_ >= 0 && loaded.cleanup_row_ < board_height_ &&
            loaded.cleanup_count_ >= 0 &&
            loaded.cleanup_count_ <= board_height_;
    for (std::size_t i = 0; valid && i < loaded.explosion_targets_.size();
         ++i) {
      valid = loaded.explosion_targets_[i] >= -1 &&
              loaded.explosion_targets_[i] < board_width_ * board_height_;
    }
    for (const auto &row : loaded.board_)
      for (int cell : row)
        if (valid)
          valid = cell >= 0 && cell <= 7;
    if (!valid)
      return false;

    score_ = loaded.score_;
    lines_ = loaded.lines_;
    quadras_ = loaded.quadras_;
    action_sound_ = loaded.action_sound_;
    step_sound_ = loaded.step_sound_;
    show_controls_ = loaded.show_controls_;
    theme_ = loaded.theme_;
    board_ = loaded.board_;
    piece_ = loaded.piece_;
    next_piece_ = loaded.next_piece_;
    next_rotation_ = loaded.next_rotation_;
    next_piece_x_ = loaded.next_piece_x_;
    next_piece_y_ = loaded.next_piece_y_;
    rotation_ = loaded.rotation_;
    piece_x_ = loaded.piece_x_;
    piece_y_ = loaded.piece_y_;
    paused_ = loaded.paused_;
    game_over_ = loaded.game_over_;
    cleanup_phase_ = loaded.cleanup_phase_;
    cleanup_row_ = loaded.cleanup_row_;
    cleanup_count_ = loaded.cleanup_count_;
    explosion_targets_ = loaded.explosion_targets_;
    return true;
  }
  default:
    return false;
  }
}

void main::Data::Save(std::ostream &output) const {
  if (incompatible_save_) {
    output.setstate(std::ios::failbit);
    return;
  }
  output << std::dec << std::noboolalpha << std::noshowbase << std::noshowpos;
  output.width(0);
  output << save_version_ << '\n'
         << score_ << '\n'
         << lines_ << '\n'
         << quadras_ << '\n'
         << action_sound_ << '\n'
         << step_sound_ << '\n'
         << show_controls_ << '\n'
         << theme_ << '\n'
         << piece_ << '\n'
         << next_piece_ << '\n'
         << next_rotation_ << '\n'
         << next_piece_x_ << '\n'
         << next_piece_y_ << '\n'
         << rotation_ << '\n'
         << piece_x_ << '\n'
         << piece_y_ << '\n'
         << paused_ << '\n'
         << game_over_ << '\n'
         << cleanup_phase_ << '\n'
         << cleanup_row_ << '\n'
         << cleanup_count_ << '\n';
  for (std::size_t i = 0; i < explosion_targets_.size(); ++i)
    output << explosion_targets_[i] << '\n';
  for (const auto &row : board_)
    for (int cell : row)
      output << cell << '\n';
}

void main::Data::Reset() {
  incompatible_save_ = false;
  incompatible_save_version_ = save_version_;
  action_sound_ = true;
  step_sound_ = false;
  show_controls_ = false;
  theme_ = THEME_SYSTEM;
  Restart();
}

void main::Data::Restart() {
  score_ = 0;
  lines_ = 0;
  quadras_ = 3;
  board_ = {};
  paused_ = true;
  game_over_ = false;
  cleanup_phase_ = CLEANUP_PLAYING;
  cleanup_row_ = 0;
  cleanup_count_ = 0;
  explosion_targets_.fill(-1);
  ChooseNextPiece();
  SpawnPiece(false);
}

void main::Data::SpawnPiece(bool has_previous_piece) {
  const int piece = next_piece_;
  const int rotation = next_rotation_;
  const int x = next_piece_x_;
  const int y = next_piece_y_;

  ChooseNextPiece(piece, has_previous_piece ? piece_ : -1);

  piece_ = piece;
  rotation_ = rotation;
  piece_x_ = x;
  piece_y_ = y;
  ++piece_generation_;
  if (!Fits(piece_, rotation_, piece_x_, piece_y_)) {
    score_ += quadras_ * 40;
    quadras_ = 0;
    game_over_ = true;
  }
}

void main::Data::ChooseNextPiece(int previous_piece, int earlier_piece) {
  int weights[7];
  if (previous_piece < 0) {
    std::fill(std::begin(weights), std::end(weights), 19);
  } else if (earlier_piece < 0 || previous_piece == earlier_piece) {
    std::fill(std::begin(weights), std::end(weights), 22);
    weights[previous_piece] = 1;
  } else {
    std::fill(std::begin(weights), std::end(weights), 26);
    weights[previous_piece] = 1;
    weights[earlier_piece] = 2;
  }

  int selection = std::uniform_int_distribution<int>(1, 133)(random_);
  for (next_piece_ = 0; next_piece_ < 6; ++next_piece_) {
    selection -= weights[next_piece_];
    if (selection <= 0)
      break;
  }
  next_rotation_ = std::uniform_int_distribution<int>(0, 3)(random_);

  int min_x = 3;
  int max_x = 0;
  int max_y = 0;
  for (const auto &block : shapes_[next_piece_][next_rotation_]) {
    min_x = std::min(min_x, block[0]);
    max_x = std::max(max_x, block[0]);
    max_y = std::max(max_y, block[1]);
  }

  next_piece_x_ =
      std::uniform_int_distribution<int>(3 - min_x, 6 - max_x)(random_);
  next_piece_y_ = hidden_rows_ - max_y;
}

bool main::Data::Fits(int type, int rotation, int x, int y) const {
  for (const auto &block : shapes_[type][rotation]) {
    const int bx = x + block[0];
    const int by = y + block[1];
    if (bx < 0 || bx >= board_width_ || by < 0 || by >= board_height_ ||
        board_[by][bx])
      return false;
  }
  return true;
}
