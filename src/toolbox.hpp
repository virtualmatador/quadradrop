#ifndef SRC_TOOLBOX_H
#define SRC_TOOLBOX_H

#include <sstream>
#include <type_traits>

#include "bridge.h"

namespace toolbox
{
    template<typename T>
    void Load(const char* key, T & value, const T & begin, const T & end)
    {
        std::istringstream parser(bridge::GetPreference(key));
        parser >> value;
        if (parser.fail())
        {
            throw "";
        }
        if constexpr (!std::is_same<T, bool>::value)
        {
            if (value < begin || !(value < end))
                throw "";
        }
    };

    template<typename T>
    void Save(const char* key, const T & value)
    {
        std::ostringstream composer;
        composer << value;
        bridge::SetPreference(key, composer.str().c_str());
    }
}

#endif //SRC_TOOLBOX_H
