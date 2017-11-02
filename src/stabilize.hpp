/*
   stabilize.hpp : PID-based stablization code

   This file is part of Hackflight.

   Hackflight is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Hackflight is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with Hackflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <limits>

#include "receiver.hpp"
#include "config.hpp"
#include "filter.hpp"
#include "model.hpp"

namespace hf {

enum {
    AXIS_ROLL = 0,
    AXIS_PITCH,
    AXIS_YAW
};

class Stabilize {
public:
    int16_t axisPids[3];

    void init(const StabilizeConfig& _config, const ImuConfig& _imuConfig, Model * _model);

    void update(float rcCommandRoll, float rcCommandPitch, float rcCommandYaw, 
            int16_t gyroRaw[3], float eulerAnglesDegrees[3]);

    void resetIntegral(void);

private:

    int16_t lastGyro[2];
    int32_t delta1[2]; 
    int32_t delta2[2];
    int32_t errorGyroI[3];
    int32_t errorAngleI[2];

    int16_t softwareTrim[3];

    Board * board;
    Model * model;

    ImuConfig imuConfig;
    StabilizeConfig config;

    int32_t computeITermGyro(float rateP, float rateI, int16_t rcCommand, int16_t gyroRaw[3], uint8_t axis);
    int16_t computePid(float rateP, int32_t PTerm, int32_t ITerm, int32_t DTerm, int16_t gyroRaw[3], uint8_t axis);
    int16_t computeLevelPid(int16_t rcCommand, float prop, int16_t gyroRaw[3], float eulerAnglesDegrees[3], uint8_t imuAxis);
}; 


/********************************************* CPP ********************************************************/

void Stabilize::init(const StabilizeConfig& _config, const ImuConfig& _imuConfig, Model * _model)
{
    // We'll use PID, IMU config values in update() below
    memcpy(&config, &_config, sizeof(StabilizeConfig));
    memcpy(&imuConfig, &_imuConfig, sizeof(ImuConfig));
    model = _model;

    // Zero-out previous values for D term
    for (uint8_t axis=0; axis<2; ++axis) {
        lastGyro[axis] = 0;
        delta1[axis] = 0;
        delta2[axis] = 0;
    }

    // Store software trims in a convenient form 
    softwareTrim[AXIS_ROLL]  = _model->softwareTrimRoll;
    softwareTrim[AXIS_PITCH] = _model->softwareTrimPitch;
    softwareTrim[AXIS_YAW]   = _model->softwareTrimYaw;

    resetIntegral();
}

int32_t Stabilize::computeITermGyro(float rateP, float rateI, int16_t rcCommand, int16_t gyroRaw[3], uint8_t axis)
{
    int32_t error = (int32_t)(rcCommand * rateP) - gyroRaw[axis];

    // Avoid integral windup
    errorGyroI[axis] = Filter::constrainAbs(errorGyroI[axis] + error, config.gyroWindupMax);

    // Reset integral on quick gyro change or large yaw command
    if ((std::abs(gyroRaw[axis]) > config.bigGyro) || ((axis == AXIS_YAW) && (std::abs(rcCommand) > config.bigYawDemand)))
        errorGyroI[axis] = 0;

    return (int32_t)(errorGyroI[axis] * rateI);
}

int16_t Stabilize::computePid(float rateP, int32_t PTerm, int32_t ITerm, int32_t DTerm, int16_t gyroRaw[3], uint8_t axis)
{
    PTerm -= (int32_t)(gyroRaw[axis] * rateP);
    return PTerm + ITerm - DTerm + softwareTrim[axis];
}

// Computes PID for pitch or roll
int16_t Stabilize::computeLevelPid(int16_t rcCommand, float prop, int16_t gyroRaw[3], float eulerAnglesDegrees[3], uint8_t imuAxis)
{
    int32_t ITermGyro = computeITermGyro(model->ratePitchrollP, model->ratePitchrollI, rcCommand, gyroRaw, imuAxis);

    // RC command is in [-500,+500].  We compute error by scaling it up to [-1000,+1000], then treating this value as tenths
    // of a degree and subtracting off corresponding pitch or roll angle obtained from IMU.
    int32_t errorAngle = Filter::constrainAbs(2*rcCommand, (int32_t)(10*imuConfig.maxAngleInclination)) - 
        (int32_t)(10*eulerAnglesDegrees[imuAxis]);

    int32_t PTermAccel = (int32_t)(errorAngle * model->levelP); 

    // Avoid integral windup
    errorAngleI[imuAxis] = Filter::constrainAbs(errorAngleI[imuAxis] + errorAngle, config.angleWindupMax);

    int32_t PTerm = (int32_t)Filter::complementary((float)rcCommand, (float)PTermAccel, prop);

    int32_t ITerm = (int32_t)(ITermGyro * prop);

    int32_t delta = gyroRaw[imuAxis] - lastGyro[imuAxis];
    lastGyro[imuAxis] = gyroRaw[imuAxis];
    int32_t deltaSum = delta1[imuAxis] + delta2[imuAxis] + delta;
    delta2[imuAxis] = delta1[imuAxis];
    delta1[imuAxis] = delta;
    int32_t DTerm = (int32_t)(deltaSum * model->ratePitchrollD);

    return computePid(model->ratePitchrollP, PTerm, ITerm, DTerm, gyroRaw, imuAxis);
}

void Stabilize::update(float rcCommandRoll, float rcCommandPitch, float rcCommandYaw, 
            int16_t gyroRaw[3], float eulerAnglesDegrees[3])
{
    // XXX Convert receiver demands to integers for stabilizer
    int16_t demandRoll  = 1000 * rcCommandRoll;
    int16_t demandPitch = 1000 * rcCommandPitch;
    int16_t demandYaw   = 1000 * rcCommandYaw;

     // Compute proportion of cyclic demand compared to its maximum
    float prop = (std::max)(std::abs(demandRoll), std::abs(demandPitch)) / 500.f;

    // Pitch, roll use leveling based on Euler angles
    axisPids[AXIS_ROLL]  = computeLevelPid(demandRoll,  prop, gyroRaw, eulerAnglesDegrees, AXIS_ROLL);
    axisPids[AXIS_PITCH] = computeLevelPid(demandPitch, prop, gyroRaw, eulerAnglesDegrees, AXIS_PITCH);

    // For yaw, P term comes directly from RC command, and D term is zero
    int32_t ITermGyroYaw = computeITermGyro(model->yawP, model->yawI, demandYaw, gyroRaw, AXIS_YAW);
    axisPids[AXIS_YAW] = computePid(model->yawP, demandYaw, ITermGyroYaw, 0, gyroRaw, AXIS_YAW);

    // Prevent "yaw jump" during yaw correction
    axisPids[AXIS_YAW] = Filter::constrainAbs(axisPids[AXIS_YAW], 100 + std::abs(demandYaw));
}

void Stabilize::resetIntegral(void)
{
    errorGyroI[AXIS_ROLL] = 0;
    errorGyroI[AXIS_PITCH] = 0;
    errorGyroI[AXIS_YAW] = 0;
    errorAngleI[AXIS_ROLL] = 0;
    errorAngleI[AXIS_PITCH] = 0;
}

} // namespace
