#ifndef SRC_DATA_H
#define SRC_DATA_H

#include <array>

namespace main {
class Data {
  friend class Menu;
  friend class Game;

public:
  Data() = default;
  void Load();
  void Save();
  void Reset();

private:
  static constexpr int board_width_ = 10;
  static constexpr int board_height_ = 24;
  using Board = std::array<std::array<int, board_width_>, board_height_>;
  static constexpr int score_max_ = 1000000000;
  static constexpr int lines_max_ = 1000000;
  int score_ = 0;
  int lines_ = 0;
  bool sound_ = true;
  bool show_controls_ = true;
  bool game_initialized_ = false;
  Board board_{};
  int piece_ = 0;
  int next_piece_ = 0;
  int next_rotation_ = 0;
  int next_piece_x_ = 3;
  int next_piece_y_ = 2;
  int rotation_ = 0;
  int piece_x_ = 3;
  int piece_y_ = 2;
  bool paused_ = true;
  bool game_over_ = false;
  // 0 = playing, 1 = animating a full row, 2 = moving rows above it down.
  int cleanup_phase_ = 0;
  int cleanup_row_ = 0;
  int cleanup_count_ = 0;
};

extern Data data_;
} // namespace main

#endif
