//
//  menu.h
//  QuadraDrop
//
//  Created by Ali Asadpoor on 4/12/19.
//  Copyright © 2020 Shaidin. All rights reserved.
//

#ifndef SRC_MENU_H
#define SRC_MENU_H

#include "stage.h"

namespace main
{
    class Menu: public core::Stage
    {
    public:
        Menu();
        ~Menu();
        void Escape() override;
        void FeedUri(const char* uri, std::function<void(
            const std::vector<unsigned char>&)>&& consume) override;

    private:
        void Play();
        void Reset();
    };
}

#endif //SRC_MENU_H
