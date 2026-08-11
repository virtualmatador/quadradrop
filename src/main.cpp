//
//  main.cpp
//  QuadraDrop
//
//  Created by Ali Asadpoor on 7/13/19.
//  Copyright © 2020 Shaidin. All rights reserved.
//

#include <locale>

#include "main.h"

#include "data.h"
#include "game.h"
#include "menu.h"
#include "progress.h"

main::PROGRESS main::progress_ = main::PROGRESS::MENU;

void application::Restore(Completion completion) {
  std::locale::global(std::locale::classic());
  main::data_.Load();
  completion();
}

void application::Checkpoint() {
  main::data_.Save();
}

std::unique_ptr<core::Stage> application::CreateStage() {
  if (main::progress_ == main::PROGRESS::GAME) {
    return std::make_unique<main::Game>();
  }
  return std::make_unique<main::Menu>();
}
