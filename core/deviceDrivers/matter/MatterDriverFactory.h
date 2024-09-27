//
// Created by tlea200 on 11/4/21.
//

#ifndef ZILKER_MATTERDRIVERFACTORY_H
#define ZILKER_MATTERDRIVERFACTORY_H

#include "matter/MatterDeviceDriver.h"
#include <map>

namespace zilker
{
    class MatterDriverFactory
    {
    public:
        static MatterDriverFactory &Instance()
        {
            static MatterDriverFactory instance;
            return instance;
        }

        bool RegisterDriver(MatterDeviceDriver *driver);
        MatterDeviceDriver *GetDriver(DiscoveredDeviceDetails *details);

    private:
        MatterDriverFactory() = default;
        ~MatterDriverFactory() = default;
        std::map<const char *, MatterDeviceDriver *> drivers;
    };

} // namespace zilker

#endif // ZILKER_MATTERDRIVERFACTORY_H
