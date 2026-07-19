#pragma once

#include <cstdint>

namespace tsm { namespace features { namespace camera {

bool ReadCameraAngleX(float& outValue);
bool WriteCameraAngleX(float value);

bool ReadCameraAngleY(float& outValue);
bool WriteCameraAngleY(float value);

bool ReadCameraRotation(float& outValue);
bool WriteCameraRotation(float value);

bool ReadCameraFOV(float& outValue);
bool WriteCameraFOV(float value);

bool ReadCameraZoom(float& outValue);
bool WriteCameraZoom(float value);

}}}
