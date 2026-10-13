#pragma once

#include <QString>
#include <memory>
#include "ThreeDModel.hpp"
#include "ThreeDPose.hpp"

namespace Kaburagi {

class ThreeDVrmParser {
public:
    static bool loadVrm(const QString& filePath, ThreeDModel& model, Skeleton3D& skeleton);
};

} // namespace Kaburagi
