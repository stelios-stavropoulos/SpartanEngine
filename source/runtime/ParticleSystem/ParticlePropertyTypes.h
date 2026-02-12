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
#include "../Rendering/Color.h"
//============================================

namespace spartan
{
    enum particle_property_type
    {
        CONSTANT,
        RANGE,
        CURVE
    };

    struct ParticleFloat
    {
        particle_property_type type = particle_property_type::CONSTANT;

        float const_value = 0.0f;
        std::pair<float, float> range_value;

        float GetConstValue()
        {
            return const_value;
        }

        float GetRangedValue()
        {
            return std::lerp(range_value.first, range_value.second, math::random(0.0f, 1.0f));
        }
    };

    struct ParticleVector3
    {
        particle_property_type type = particle_property_type::CONSTANT;

        math::Vector3 const_value{ 0.0f, 0.0f, 0.0f };
        std::pair<math::Vector3, math::Vector3> range_value;

        math::Vector3 GetConstValue()
        {
            return const_value;
        }

        math::Vector3 GetRangedValue()
        {
            float x = std::lerp(range_value.first.x, range_value.second.x, math::random(0.0f, 1.0f));
            float y = std::lerp(range_value.first.y, range_value.second.y, math::random(0.0f, 1.0f));
            float z = std::lerp(range_value.first.z, range_value.second.z, math::random(0.0f, 1.0f));

            return math::Vector3(x,y,z);
        }
    };

    struct ParticleColor
    {
        particle_property_type type = particle_property_type::CONSTANT;
        Color const_value{ 1.0f, 1.0f, 1.0f, 1.0f };
        std::pair<Color, Color> range_value;
        Color GetConstValue()
        {
            return const_value;
        }
        Color GetRangedValue()
        {
            float r = std::lerp(range_value.first.r, range_value.second.r, math::random(0.0f, 1.0f));
            float g = std::lerp(range_value.first.g, range_value.second.g, math::random(0.0f, 1.0f));
            float b = std::lerp(range_value.first.b, range_value.second.b, math::random(0.0f, 1.0f));
            float a = std::lerp(range_value.first.a, range_value.second.a, math::random(0.0f, 1.0f));

            return Color(r, g, b, a);
        }
    };

}
