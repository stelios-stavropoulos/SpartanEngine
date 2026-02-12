/*
Copyright(c) 2015-2026 Panos Karabelas

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

//= INCLUDES =================================
#include "../EmitterModule.h"
//============================================

namespace spartan
{
    class EM_Gravity : public EmitterModule
    {
    public:
        ParticleVector3 gravity{
            .type = particle_property_type::CONSTANT,
            .const_value = {0.0f, -9.81f, 0.0f},
            .range_value = std::make_pair(
                spartan::math::Vector3{0.0f, -8.0f, 0.0f},
                spartan::math::Vector3{0.0f, -10.0f, 0.0f}
            )
        };

    public:
        EM_Gravity() = default;
        ~EM_Gravity() = default;

        void OnUpdate(ParticleData& particle_data, const double& delta_time) override;

        const EmitterModuleType GetType() const override { return EmitterModuleType::Gravity; }
    };
}
