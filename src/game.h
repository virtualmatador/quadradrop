#ifndef SRC_GAME_H
#define SRC_GAME_H

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
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
  void Attach() override;
  void Suspend() override;
  void Escape() override;
  void
  FeedUri(const char *,
          std::function<void(const std::vector<unsigned char> &)> &&) override;
  void Setup();
  void Run();
  bool SetPaused(bool paused);
  const char *Step();
  void HandleAction(const char *action);
  bool Move(int dx, int dy);
  bool Rotate(int direction = 1);
  const char *HardDrop();
  const char *Explode();
  const char *LockPiece();
  std::array<int, 4> ExplosionSources() const;
  std::string ExplosionMoves() const;
  void ApplyExplosion();
  void Cleanup();
  const char *ResolveBoard();
  int FindFullRow() const;
  bool ValidNextPiece() const;
  int GravityFrames() const;
  bool ValidateData() const;
  std::string PreviewState(int type, int rotation, int x, int y) const;
  std::string BoardState() const;
  std::string ActiveState() const;
  std::string NextState() const;
  void Render();
  void PlayAudio(const char *audio);

  int frame_ = 0;
  bool screen_on_ = false;
  bool run_ = true;
  std::mutex lock_;
  std::condition_variable waiter_;
  std::thread worker_;
};
} // namespace main

#endif
