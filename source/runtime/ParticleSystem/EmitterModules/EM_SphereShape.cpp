/*
Copyright(c) 2015-2025 Panos Karabelas

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

//= INCLUDES ======================
#include "pch.h"
#include "EM_SphereShape.h"
#include "../ParticleData.h"
//=================================

namespace spartan
{
    inline math::Vector3 EM_SphereShape::RandomPointInSphere(float radius)
    {
        float x = math::random(-1.0f, 1.0f);
        float y = math::random(-1.0f, 1.0f);
        float z = math::random(-1.0f, 1.0f);

        // Quick normalization (not perfect but fast)
        float inv_mag = 1.0f / std::sqrt(x * x + y * y + z * z + 1e-8f);

        // Random radius with quadratic distribution (fast approximation of cube root)
        float u = math::random(0.0f, 1.0f);
        // Quadratic gives decent approximation of uniform volume distribution
        float scale = radius * u * u;

        return math::Vector3(x * inv_mag * scale, y * inv_mag * scale, z * inv_mag * scale);
    }

    void EM_SphereShape::OnInitialize(ParticleData& particle_data, uint32_t start_index, uint32_t end_index)
    {
        if (radius.type == particle_property_type::CONSTANT)
        {
            float radius_value = radius.GetConstValue();
            for (uint32_t i = start_index; i < end_index; ++i)
            {
                particle_data.particles[i].position = RandomPointInSphere(radius_value);
            }
        }
        else if (radius.type == particle_property_type::RANGE)
        {
            for (uint32_t i = start_index; i < end_index; ++i)
            {
                float radius_value = radius.GetRangedValue();

                particle_data.particles[i].position = RandomPointInSphere(radius_value);
            }
        }
    }
}
