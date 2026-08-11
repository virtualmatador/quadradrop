#include "data.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <sstream>

#include "bridge.h"
#include "toolbox.hpp"

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

void main::Data::Load() {
  try {
    toolbox::Load("GAME_SCORE", score_, 0, score_max_ + 1);
    toolbox::Load("GAME_LINES", lines_, 0, lines_max_ + 1);
    toolbox::Load("GAME_QUADRAS", quadras_, 0, quadras_max_ + 1);
    toolbox::Load("GAME_ACTION_SOUND", action_sound_, false, false);
    toolbox::Load("GAME_STEP_SOUND", step_sound_, false, false);
    toolbox::Load("GAME_SHOW_CONTROLS", show_controls_, false, false);
    toolbox::Load("GAME_PIECE", piece_, 0, 7);
    toolbox::Load("GAME_NEXT_PIECE", next_piece_, 0, 7);
    toolbox::Load("GAME_NEXT_ROTATION", next_rotation_, 0, 4);
    toolbox::Load("GAME_NEXT_PIECE_X", next_piece_x_, 1, 5);
    toolbox::Load("GAME_NEXT_PIECE_Y", next_piece_y_, 1, 4);
    toolbox::Load("GAME_ROTATION", rotation_, 0, 4);
    toolbox::Load("GAME_PIECE_X", piece_x_, -2, board_width_);
    toolbox::Load("GAME_PIECE_Y", piece_y_, 0, board_height_);
    toolbox::Load("GAME_PAUSED", paused_, false, false);
    toolbox::Load("GAME_OVER", game_over_, false, false);
    toolbox::Load("GAME_CLEANUP_PHASE", cleanup_phase_, 0,
                  static_cast<int>(CLEANUP_PHASE_COUNT));
    toolbox::Load("GAME_CLEANUP_ROW", cleanup_row_, 0, board_height_);
    toolbox::Load("GAME_CLEANUP_COUNT", cleanup_count_, 0, board_height_ + 1);
    for (std::size_t i = 0; i < explosion_targets_.size(); ++i) {
      std::ostringstream key;
      key << "GAME_EXPLOSION_TARGET_" << i;
      toolbox::Load(key.str().c_str(), explosion_targets_[i], -1,
                    board_width_ * board_height_);
    }
    for (int y = 0; y < board_height_; ++y) {
      std::ostringstream key;
      key << "GAME_BOARD_" << y;
      const auto row = bridge::GetPreference(key.str().c_str());
      if (row.size() != board_width_)
        throw "invalid board row";
      for (int x = 0; x < board_width_; ++x) {
        if (row[x] < '0' || row[x] > '7')
          throw "invalid board cell";
        board_[y][x] = row[x] - '0';
      }
    }
  } catch (...) {
    Reset();
  }
}

void main::Data::Save() {
  toolbox::Save("GAME_SCORE", score_);
  toolbox::Save("GAME_LINES", lines_);
  toolbox::Save("GAME_QUADRAS", quadras_);
  toolbox::Save("GAME_ACTION_SOUND", action_sound_);
  toolbox::Save("GAME_STEP_SOUND", step_sound_);
  toolbox::Save("GAME_SHOW_CONTROLS", show_controls_);
  toolbox::Save("GAME_PIECE", piece_);
  toolbox::Save("GAME_NEXT_PIECE", next_piece_);
  toolbox::Save("GAME_NEXT_ROTATION", next_rotation_);
  toolbox::Save("GAME_NEXT_PIECE_X", next_piece_x_);
  toolbox::Save("GAME_NEXT_PIECE_Y", next_piece_y_);
  toolbox::Save("GAME_ROTATION", rotation_);
  toolbox::Save("GAME_PIECE_X", piece_x_);
  toolbox::Save("GAME_PIECE_Y", piece_y_);
  toolbox::Save("GAME_PAUSED", paused_);
  toolbox::Save("GAME_OVER", game_over_);
  toolbox::Save("GAME_CLEANUP_PHASE", cleanup_phase_);
  toolbox::Save("GAME_CLEANUP_ROW", cleanup_row_);
  toolbox::Save("GAME_CLEANUP_COUNT", cleanup_count_);
  for (std::size_t i = 0; i < explosion_targets_.size(); ++i) {
    std::ostringstream key;
    key << "GAME_EXPLOSION_TARGET_" << i;
    toolbox::Save(key.str().c_str(), explosion_targets_[i]);
  }
  for (int y = 0; y < board_height_; ++y) {
    std::ostringstream key;
    key << "GAME_BOARD_" << y;
    std::string row;
    row.reserve(board_width_);
    for (int cell : board_[y])
      row.push_back(static_cast<char>('0' + cell));
    bridge::SetPreference(key.str().c_str(), row.c_str());
  }
}

void main::Data::Reset() {
  action_sound_ = true;
  step_sound_ = false;
  show_controls_ = false;
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
