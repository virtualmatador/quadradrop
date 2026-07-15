#include "data.h"

#include <sstream>

#include "bridge.h"
#include "toolbox.hpp"

main::Data main::data_;

void main::Data::Load() {
  try {
    toolbox::Load("GAME_SCORE", score_, 0, score_max_ + 1);
    toolbox::Load("GAME_LINES", lines_, 0, lines_max_ + 1);
    toolbox::Load("GAME_SOUND", sound_, false, false);
    // Older saved games predate this preference and should retain their data.
    if (!bridge::GetPreference("GAME_SHOW_CONTROLS").empty())
      toolbox::Load("GAME_SHOW_CONTROLS", show_controls_, false, false);
    toolbox::Load("GAME_INITIALIZED", game_initialized_, false, false);
    if (!game_initialized_)
      return;
    toolbox::Load("GAME_PIECE", piece_, 0, 7);
    toolbox::Load("GAME_NEXT_PIECE", next_piece_, 0, 7);
    toolbox::Load("GAME_ROTATION", rotation_, 0, 4);
    toolbox::Load("GAME_PIECE_X", piece_x_, -2, board_width_);
    toolbox::Load("GAME_PIECE_Y", piece_y_, 0, board_height_);
    toolbox::Load("GAME_PAUSED", paused_, false, false);
    toolbox::Load("GAME_OVER", game_over_, false, false);
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
  toolbox::Save("GAME_SOUND", sound_);
  toolbox::Save("GAME_SHOW_CONTROLS", show_controls_);
  toolbox::Save("GAME_INITIALIZED", game_initialized_);
  if (!game_initialized_)
    return;
  toolbox::Save("GAME_PIECE", piece_);
  toolbox::Save("GAME_NEXT_PIECE", next_piece_);
  toolbox::Save("GAME_ROTATION", rotation_);
  toolbox::Save("GAME_PIECE_X", piece_x_);
  toolbox::Save("GAME_PIECE_Y", piece_y_);
  toolbox::Save("GAME_PAUSED", paused_);
  toolbox::Save("GAME_OVER", game_over_);
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
  score_ = 0;
  lines_ = 0;
  sound_ = true;
  show_controls_ = true;
  game_initialized_ = false;
  board_ = {};
  paused_ = true;
  game_over_ = false;
}
