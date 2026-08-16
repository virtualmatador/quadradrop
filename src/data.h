#ifndef SRC_DATA_H
#define SRC_DATA_H

#include <array>
#include <iosfwd>
#include <random>

namespace main {
class Data {
  friend class Menu;
  friend class Game;

public:
  Data() = default;
  void Load(std::istream &input);
  void Save(std::ostream &output) const;
  void Reset();

private:
  enum CleanupPhase {
    CLEANUP_PLAYING,
    CLEANUP_START,
    CLEANUP_CLEARING,
    CLEANUP_LEVEL_CHANGE,
    CLEANUP_WIN,
    CLEANUP_EXPLODE,
    CLEANUP_EXPLODING,
    CLEANUP_PHASE_COUNT,
  };

  int Level() const;
  bool Convert(int version, std::istream &input);
  void Restart();
  void SpawnPiece(bool has_previous_piece = true);
  void ChooseNextPiece(int previous_piece = -1, int earlier_piece = -1);
  bool Fits(int type, int rotation, int x, int y) const;

  static constexpr int board_width_ = 10;
  static constexpr int board_height_ = 24;
  static constexpr int hidden_rows_ = 4;
  static const int shapes_[7][4][4][2];
  static constexpr int score_max_ = 1000000000;
  static constexpr int lines_max_ = 1000000;
  static constexpr int quadras_max_ = 100000;
  static constexpr int save_version_ = 1;

  using Board = std::array<std::array<int, board_width_>, board_height_>;
  int score_ = 0;
  int lines_ = 0;
  int quadras_ = 0;
  bool action_sound_ = true;
  bool step_sound_ = true;
  bool show_controls_ = true;
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
  int cleanup_phase_ = CLEANUP_PLAYING;
  int cleanup_row_ = 0;
  int cleanup_count_ = 0;
  std::array<int, 4> explosion_targets_{{-1, -1, -1, -1}};
  unsigned int piece_generation_ = 0;
  bool incompatible_save_ = false;
  int incompatible_save_version_ = save_version_;
  std::random_device seeder_;
  std::default_random_engine random_{seeder_()};
};

extern Data data_;
} // namespace main

#endif
