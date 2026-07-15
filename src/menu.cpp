//
//  error.cpp
//  QuadraDrop
//
//  Created by Ali Asadpoor on 4/12/19.
//  Copyright © 2020 Shaidin. All rights reserved.
//

#include <cstring>
#include <sstream>

#include "helper.h"

#include "data.h"
#include "menu.h"
#include "progress.h"

main::Menu::Menu() {
  handlers_["body"] = [&](const char *command, const char *info) {
    if (std::strlen(command) == 0)
      return;
    else if (std::strcmp(command, "ready") == 0) {
      std::ostringstream js;
      js.str("");
      js.clear();
      js << "setLevel(" << data_.lines_ / 10 + 1 << ")";
      bridge::CallFunction(js.str().c_str());
      js.str("");
      js.clear();
      js << "setLines(" << data_.lines_ << ")";
      bridge::CallFunction(js.str().c_str());
      js.str("");
      js.clear();
      js << "setScore(" << data_.score_ << ")";
      bridge::CallFunction(js.str().c_str());
      js.str("");
      js.clear();
      js << "setSound(" << data_.sound_ << ")";
      bridge::CallFunction(js.str().c_str());
      js.str("");
      js.clear();
      js << "setShowControls(" << data_.show_controls_ << ")";
      bridge::CallFunction(js.str().c_str());
    }
  };
  handlers_["play"] = [&](const char *command, const char *info) {
    if (std::strlen(command) == 0)
      return;
    else if (std::strcmp(command, "click") == 0)
      Play();
  };
  handlers_["reset"] = [&](const char *command, const char *info) {
    if (std::strlen(command) == 0)
      return;
    else if (std::strcmp(command, "click") == 0)
      Reset();
  };
  handlers_["sound"] = [&](const char *command, const char *info) {
    if (std::strlen(command) == 0)
      return;
    else if (std::strcmp(command, "click") == 0) {
      if (std::strlen(info) == 0)
        return;
      else if (std::strcmp(info, "true") == 0)
        data_.sound_ = true;
      else if (std::strcmp(info, "false") == 0)
        data_.sound_ = false;
    }
  };
  handlers_["show-controls"] = [&](const char *command, const char *info) {
    if (std::strlen(command) == 0)
      return;
    else if (std::strcmp(command, "click") == 0) {
      if (std::strlen(info) == 0)
        return;
      else if (std::strcmp(info, "true") == 0)
        data_.show_controls_ = true;
      else if (std::strcmp(info, "false") == 0)
        data_.show_controls_ = false;
    }
  };
  bridge::LoadView(index_, (std::int32_t)core::VIEW_INFO::AudioNoSolo, "menu");
}

main::Menu::~Menu() {}

void main::Menu::FeedUri(
    const char *uri,
    std::function<void(const std::vector<unsigned char> &)> &&consume) {}

void main::Menu::Escape() { bridge::Exit(); }

void main::Menu::Play() {
  progress_ = PROGRESS::GAME;
  bridge::NeedRestart();
}

void main::Menu::Reset() {
  data_.Reset();
  bridge::NeedRestart();
}
