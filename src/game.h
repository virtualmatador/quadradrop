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
  bool Rotate(int direction = 1);
  void HardDrop();
  void LockPiece();
  bool BeginCleanup();
  void AdvanceCleanup();
  int FindFullRow() const;
  void SpawnPiece();
  bool PieceEnteredView() const;
  void UpdatePreview();
  int RandomPiece();
  int Level() const;
  int GravityFrames() const;
  bool ValidateData() const;
  std::string BoardState() const;
  std::string NextState() const;
  void Render();
  void PlayAudio(const char *audio);

  int frame_ = 0;
  bool preview_pending_ = false;
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
