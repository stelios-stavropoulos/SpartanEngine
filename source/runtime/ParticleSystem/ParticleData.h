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
        uint32_t max_particle_count    = 0;
        uint32_t alive_particle_count  = 0;

        float* pos_x              = nullptr;
        float* pos_y              = nullptr;
        float* pos_z              = nullptr;
        float* vel_x              = nullptr;
        float* vel_y              = nullptr;
        float* vel_z              = nullptr;
        float* col_r              = nullptr;
        float* col_g              = nullptr;
        float* col_b              = nullptr;
        float* col_a              = nullptr;
        float* scale_x            = nullptr;
        float* scale_y            = nullptr;
        float* inv_lifetimes      = nullptr;
        float* normalized_lifetimes = nullptr;

        // Per-particle spawn values for lerping — only allocated when needed
        float* spawn_col_r        = nullptr; // color at birth
        float* spawn_col_g        = nullptr;
        float* spawn_col_b        = nullptr;
        float* spawn_col_a        = nullptr;
        float* spawn_scale_x      = nullptr; // scale at birth
        float* spawn_scale_y      = nullptr;

    private:
        std::vector<float, AlignedAllocator<float, 32>> buffer;
        bool has_color_lerp = false;
        bool has_scale_lerp = false;

    public:
        void Resize(uint32_t count, bool color_lerp, bool scale_lerp)
        {
            has_color_lerp = color_lerp;
            has_scale_lerp = scale_lerp;

            max_particle_count    = count;
            alive_particle_count  = 0;

            size_t stride     = (count + 7) & ~7u;
            size_t num_streams = 14;
            if (color_lerp) num_streams += 4; // spawn_col rgba
            if (scale_lerp) num_streams += 2; // spawn_scale xy

            buffer.assign(stride * num_streams, 0.0f);
            float* d = buffer.data();
            size_t s = 0;

            pos_x               = d + stride * s++;
            pos_y               = d + stride * s++;
            pos_z               = d + stride * s++;
            vel_x               = d + stride * s++;
            vel_y               = d + stride * s++;
            vel_z               = d + stride * s++;
            col_r               = d + stride * s++;
            col_g               = d + stride * s++;
            col_b               = d + stride * s++;
            col_a               = d + stride * s++;
            scale_x             = d + stride * s++;
            scale_y             = d + stride * s++;
            inv_lifetimes       = d + stride * s++;
            normalized_lifetimes = d + stride * s++;

            spawn_col_r = color_lerp ? d + stride * s++ : nullptr;
            spawn_col_g = color_lerp ? d + stride * s++ : nullptr;
            spawn_col_b = color_lerp ? d + stride * s++ : nullptr;
            spawn_col_a = color_lerp ? d + stride * s++ : nullptr;

            spawn_scale_x = scale_lerp ? d + stride * s++ : nullptr;
            spawn_scale_y = scale_lerp ? d + stride * s   : nullptr;
        }

        int32_t GetLastAliveIndex() const { return (int32_t)alive_particle_count - 1; }
    };
}
