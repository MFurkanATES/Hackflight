/*
   Hackflight sketch for TinyPICO with EM7180 USFS IMU, and mock motors/receiver

   Additional libraries needed:

       https://github.com/simondlevy/USFS
       https://github.com/simondlevy/CrossPlatformDataBus


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

#include "hackflight.hpp"
#include "boards/realboards/tinypico.hpp"
#include "receivers/mock.hpp"
#include "actuators/mixers/quadxcf.hpp"
#include "motors/mock.hpp"
#include "imus/usfs.hpp"

hf::Hackflight h;

hf::MockReceiver rc;

hf::MixerQuadXCF mixer;

hf::USFS imu;

hf::MockMotor motors;

void setup(void)
{
    hf::ArduinoBoard::powerPins(18, 19);

    h.init(new hf::TinyPico(), &imu, &rc, &mixer, &motors);
}

void loop(void)
{
    h.update();
}
