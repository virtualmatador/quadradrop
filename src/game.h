#ifndef SRC_GAME_H
#define SRC_GAME_H

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <random>
#include <string>
#include <thread>

#include "stage.h"

namespace main {
class Game : public core::Stage {
  static constexpr int width_ = 10;
  static constexpr int visible_height_ = 20;
  static constexpr int hidden_rows_ = 4;
  static constexpr int height_ = visible_height_ + hidden_rows_;
  using Board = std::array<std::array<int, width_>, height_>;

public:
  Game();
  ~Game();

private:
  void Escape() override;
  void
  FeedUri(const char *,
          std::function<void(const std::vector<unsigned char> &)> &&) override;
  void Setup();
  void Run();
  void Step();
  void HandleAction(const char *action);
  bool Fits(int type, int rotation, int x, int y) const;
  bool Move(int dx, int dy);
  bool Rotate();
  void HardDrop();
  void LockPiece();
  void ClearLines();
  void SpawnPiece();
  int RandomPiece();
  int Level() const;
  int GravityFrames() const;
  bool RestoreData();
  void SyncData();
  std::string BoardState() const;
  std::string NextState() const;
  void Render();
  void PlayAudio(const char *audio);

  Board board_{};
  int piece_ = 0;
  int next_piece_ = 0;
  int rotation_ = 0;
  int piece_x_ = 3;
  int piece_y_ = 0;
  int frame_ = 0;
  bool paused_ = true;
  bool game_over_ = false;
  bool run_ = true;
  std::random_device seeder_;
  std::default_random_engine random_{seeder_()};
  std::array<int, 7> bag_{0, 1, 2, 3, 4, 5, 6};
  std::size_t bag_index_ = bag_.size();
  std::mutex lock_;
  std::condition_variable waiter_;
  std::thread worker_;
};
} // namespace main

#endif
