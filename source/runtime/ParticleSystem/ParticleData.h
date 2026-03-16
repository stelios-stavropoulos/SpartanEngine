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

#pragma once

//= INCLUDES =================================
#include "../Rendering/Color.h"
//============================================

namespace spartan
{
    struct ParticleInstance
    {
        math::Vector3 position;
        math::Vector2 scale;
        Color color;
    };

    struct ParticleData
    {
        uint32_t max_particle_count = 0;
        uint32_t alive_particle_count = 0;

        float* inv_lifetimes = nullptr;
        float* normalized_lifetimes = nullptr;
        math::Vector3* positions = nullptr;
        math::Vector3* velocities = nullptr;
        Color* colors = nullptr;
        math::Vector2* scales = nullptr;
        
    private:
        std::vector<uint8_t> buffer;

    public:
        ParticleData() = default;

        explicit ParticleData(uint32_t initial_max_particle_count)
        {
            Resize(initial_max_particle_count);
        }

        ~ParticleData()
        {
            buffer.clear();
        }

        void Resize(uint32_t new_max_count)
        {
            max_particle_count = new_max_count;
            alive_particle_count = 0;

            // Calculate sizes for each attribute section
            size_t size_pos = new_max_count * sizeof(math::Vector3);
            size_t size_vel = new_max_count * sizeof(math::Vector3);
            size_t size_color = new_max_count * sizeof(Color);
            size_t size_scale = new_max_count * sizeof(math::Vector2);
            size_t size_life = new_max_count * sizeof(float);
            size_t size_norm_life = new_max_count * sizeof(float);

            size_t total_size = size_pos + size_vel + size_color + size_scale + size_life + size_norm_life;

            // Reallocate the entire buffer. 
            buffer.clear();
            buffer.resize(total_size);

            // Assign pointers to the correct offsets within the single buffer
            uint8_t* data_ptr = buffer.data();
            positions = reinterpret_cast<math::Vector3*>(data_ptr);
            velocities = reinterpret_cast<math::Vector3*>(data_ptr + size_pos);
            colors = reinterpret_cast<Color*>(data_ptr + size_pos + size_vel);
            scales = reinterpret_cast<math::Vector2*>(data_ptr + size_pos + size_vel + size_color);
            inv_lifetimes = reinterpret_cast<float*>(data_ptr + size_pos + size_vel + size_color + size_scale);
            normalized_lifetimes = reinterpret_cast<float*>(data_ptr + size_pos + size_vel + size_color + size_scale + size_life);

            // Optional: Zero out the memory if you want a truly clean slate
            memset(buffer.data(), 0, total_size);
        }

        int32_t GetLastAliveIndex() const
        {
            return alive_particle_count - 1;
        }
    };
}
