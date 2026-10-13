#pragma once

#include <QString>
#include <memory>
#include "ThreeDModel.hpp"
#include "ThreeDPose.hpp"

namespace Kaburagi {

class ThreeDMmdParser {
public:
    static bool loadPmd(const QString& filePath, ThreeDModel& model, Skeleton3D& skeleton);
    static bool loadPmx(const QString& filePath, ThreeDModel& model, Skeleton3D& skeleton);
};

} // namespace Kaburagi
