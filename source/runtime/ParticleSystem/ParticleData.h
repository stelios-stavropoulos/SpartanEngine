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
    template <typename T, std::size_t Alignment>
    struct AlignedAllocator
    {
        using value_type = T;

        AlignedAllocator() = default;
        template <typename U>
        AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}

        T* allocate(std::size_t n)
        {
            std::size_t size = n * sizeof(T);
            // round size up to a multiple of alignment (required by aligned_alloc)
            size = (size + Alignment - 1) & ~(Alignment - 1);
            void* ptr = ::operator new(size, std::align_val_t{ Alignment });
            if (!ptr) throw std::bad_alloc{};
            return static_cast<T*>(ptr);
        }

        void deallocate(T* ptr, std::size_t) noexcept
        {
            ::operator delete(ptr, std::align_val_t{ Alignment });
        }

        template <typename U>
        struct rebind { using other = AlignedAllocator<U, Alignment>; };

        bool operator==(const AlignedAllocator&) const noexcept { return true; }
        bool operator!=(const AlignedAllocator&) const noexcept { return false; }
    };

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

        // Position
        float* pos_x = nullptr;
        float* pos_y = nullptr;
        float* pos_z = nullptr;

        // Velocity
        float* vel_x = nullptr;
        float* vel_y = nullptr;
        float* vel_z = nullptr;

        // Color (RGBA)
        float* col_r = nullptr;
        float* col_g = nullptr;
        float* col_b = nullptr;
        float* col_a = nullptr;

        // Scale
        float* scale_x = nullptr;
        float* scale_y = nullptr;

        // Life
        float* inv_lifetimes = nullptr;
        float* normalized_lifetimes = nullptr;

    private:
        std::vector<float, AlignedAllocator<float, 32>> buffer;

    public:
        ParticleData() = default;
        ~ParticleData() { buffer.clear(); }

        explicit ParticleData(uint32_t count) { Resize(count); }

        void Resize(uint32_t new_max_count)
        {

            max_particle_count = new_max_count;
            alive_particle_count = 0;

            // 14 total float streams (3 pos, 3 vel, 4 col, 2 scale, 2 life)
            // We align each stream to 32 bytes (8 floats) for AVX compatibility
            size_t stride = (new_max_count + 7) & ~7;
            size_t total_size = stride * 14; // 14 buffers total

            buffer.assign(total_size, 0.0f);
            float* data = buffer.data();

            pos_x = data + (stride * 0);
            pos_y = data + (stride * 1);
            pos_z = data + (stride * 2);

            vel_x = data + (stride * 3);
            vel_y = data + (stride * 4);
            vel_z = data + (stride * 5);

            col_r = data + (stride * 6);
            col_g = data + (stride * 7);
            col_b = data + (stride * 8);
            col_a = data + (stride * 9);

            scale_x = data + (stride * 10);
            scale_y = data + (stride * 11);

            inv_lifetimes = data + (stride * 12);
            normalized_lifetimes = data + (stride * 13);
        }

        int32_t GetLastAliveIndex() const { return static_cast<int32_t>(alive_particle_count) - 1; }
    };
}
