//
//  progress.h
//  QuadraDrop
//
//  Created by Ali Asadpoor on 7/13/19.
//  Copyright © 2020 Shaidin. All rights reserved.
//

#ifndef PROGRESS_H
#define PROGRESS_H

#include <stdint.h>

namespace main
{
    enum class PROGRESS : std::int32_t
    {
        MENU,
        GAME,
    };

    extern PROGRESS progress_;
}

#endif //PROGRESS_H
