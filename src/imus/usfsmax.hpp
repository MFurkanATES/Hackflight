/*
   Support for USFSMAX IMU

   Copyright (c) 2020 Simon D. Levy

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

#include <Wire.h>
#include <USFSMAX_Basic.h>
#include "imu.hpp"

namespace hf {


    class USFSMAX_IMU : public IMU {

        private:

            // Magnetic constants for Kelseyville, CA
            // For your location, use https://www.ngdc.noaa.gov/geomag/calculators/magcalc.shtml#igrfwmm
            static constexpr float MAG_V           = 42.9631f; // angle, degrees
            static constexpr float MAG_H           = 22.7568f; // angle, degrees
            static constexpr float MAG_DECLINATION = 13.7433f; // uT

            // I2C Clock Speed
            static const uint32_t I2C_CLOCK = 1000000;    // 1MHz

            // Output Data Rates (ODRs)
            static const USFSMAX::AccelGyroODR_t ACCEL_ODR = USFSMAX::ACCEL_GYRO_ODR_834;
            static const USFSMAX::AccelGyroODR_t GYRO_ODR  = USFSMAX::ACCEL_GYRO_ODR_834;
            static const USFSMAX::MagODR_t       MAG_ODR   = USFSMAX::MAG_ODR_100;
            static const USFSMAX::BaroODR_t      BARO_ODR  = USFSMAX::BARO_ODR_50;
            static const USFSMAX::QuatDiv_t      QUAT_DIV  = USFSMAX::QUAT_DIV_8;

            // LSM6DSM filter settings
            static const USFSMAX::LSM6DSMGyroLPF_t   LSM6DSM_GYRO_LPF    = USFSMAX::LSM6DSM_GYRO_LPF_167;
            static const USFSMAX::LSM6DSMAccLpfODR_t LSM6DSM_ACC_LPF_ODR = USFSMAX::LSM6DSM_ACC_LPF_ODR_DIV400;

            // LIS2MDL filter setting
            static const USFSMAX::LIS2MDLMagLpfODR_t LIS2MDL_MAG_LPF_ODR = USFSMAX::LIS2MDL_MAG_LPF_ODR_4;

            // LPS22HB baro filter setting
            static const USFSMAX::LPS22HBBaroLpfODR_t LPS22HB_BARO_LPF = USFSMAX::LPS22HB_BARO_LPF_ODR_20;

            // IMU scaling
            USFSMAX::AccScale_t  ACC_SCALE  = USFSMAX::ACC_SCALE_16;
            USFSMAX::GyroScale_t GYRO_SCALE = USFSMAX::GYRO_SCALE_2000;

            USFSMAX_Basic _usfsmax = 
                USFSMAX_Basic( ACCEL_ODR, GYRO_ODR, MAG_ODR, BARO_ODR, QUAT_DIV,
                        LSM6DSM_GYRO_LPF, LSM6DSM_ACC_LPF_ODR,
                        ACC_SCALE, GYRO_SCALE,
                        LIS2MDL_MAG_LPF_ODR, LPS22HB_BARO_LPF,
                        MAG_V, MAG_H, MAG_DECLINATION);


        protected:

            virtual bool getGyrometer(float & gx, float & gy, float & gz) override
            {
                switch (_usfsmax.dataReady()) {
                    case USFSMAX::DATA_READY_GYRO_ACC:
                    case USFSMAX::DATA_READY_GYRO_ACC_MAG_BARO:
                        {
                            float gyro[3] = {};
                            float acc[3] = {};
                            _usfsmax.readGyroAcc(gyro, acc);
                            gx = gyro[0];
                            gy = gyro[1];
                            gz = gyro[2];
                        }
                        return true;
                    default:
                        break;
                }
                return false;
            }

            virtual bool getQuaternion(float & q0, float & q1, float & q2, float & q3, float time) override
            {
                (void)time;

                if (_usfsmax.quaternionReady()) {
                    float quat[4] = {};
                    _usfsmax.readQuat(quat);
                    q0 = quat[0];
                    q1 = quat[1];
                    q2 = quat[2];
                    q3 = quat[3];

                    return true;
                }

                return false;
            }

            virtual void begin(void) override
            {
                Wire.setClock(100000); 
                delay(100);
                _usfsmax.begin();
                Wire.setClock(I2C_CLOCK);// Set the I2C clock to high speed for run-mode data collection
                delay(100);

            }

            void computeEulerAngles(float q0, float q1, float q2, float q3, float euler[3])
            {
                //roll    = atan2f(2.0f*(q0*q2 - q1*q3), q0*q0 - q1*q1 - q2*q2 + q3*q3);
                //pitch   = asinf(2.0f*(q2*q3 + q0*q1));
                //heading = atan2f(2.0f*(q1*q2 - q0*q3), q0*q0 - q1*q1 + q2*q2 - q3*q3);

                // MAXUSFS Quaternion is ENU
                euler[0] = atan2f(2.0f*(q0*q2 - q1*q3), q0*q0 - q1*q1 - q2*q2 + q3*q3);  // roll
                euler[1] = asinf(2.0f*(q2*q3 + q0*q1));                                  // pitch
                euler[2] = atan2f(2.0f*(q1*q2 - q0*q3), q0*q0 - q1*q1 + q2*q2 - q3*q3);  // yaw (heading)
            }

    }; // class USFSMAX

} // namespace hf
