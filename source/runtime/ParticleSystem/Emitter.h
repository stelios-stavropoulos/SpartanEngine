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
#include "ParticleData.h"
//============================================

namespace spartan
{
    class Renderable;

    class Emitter
    {
    public:
        Emitter(Renderable* renderable);
        ~Emitter() = default;

        void Update(const double& delta_time);
        void ChangeSpawnRate(float new_spawn_rate);

    private:

        void SpawnParticles(uint32_t count, uint64_t frame_id);
        void Kill(uint32_t index);
        void SelectHotPaths();
        //inline math::Vector3 RandomPointInSphere(float radius, uint32_t particle_id, uint64_t frame_id);

    public:

        Renderable*       renderable  = nullptr;
        ParticleInstance* buffer_data = nullptr;
        ParticleData      particle_data;

        // Hot-path context structs
        struct EmitterUpdateContext
        {
            ParticleData* data;
            ParticleInstance* buffer_data;
            float               dt;
            float               grav_x, grav_y, grav_z;
            float               damping_dt;
            float               end_col_r, end_col_g, end_col_b, end_col_a;
            float               end_scale_x, end_scale_y;
        };

        struct EmitterSpawnContext
        {
            ParticleData* data;
            uint64_t          frame_id;
            float             sphere_radius;
            float             lifetime_constant, lifetime_min, lifetime_max;
            math::Vector3     initial_velocity;
            math::Vector3     velocity_min, velocity_max;
            float             velocity_scale, velocity_scale_min, velocity_scale_max;
            math::Vector2     scale_min, scale_max, scale_start;
            Color             color_min, color_max, color_start;
        };

        // Function pointer types
        using UpdateFn = void(*)(EmitterUpdateContext&, int32_t, int32_t);
        using SpawnFn  = void(*)(EmitterSpawnContext&, uint32_t, uint32_t);

        UpdateFn fn_update = nullptr;
        SpawnFn  fn_spawn  = nullptr;

        // Spawn
        float spawn_rate        = 30000.0f;
        float spawn_accumulator = 0.0f;

        enum class LifetimeMode
        {
            Constant,             // all particles get lifetime_constant
            RandomRange,          // per-particle random in [lifetime_min, lifetime_max]
        };

        // Lifetime
        LifetimeMode  lifetime_mode = LifetimeMode::RandomRange;
        float lifetime_constant    = 2.0f;
        float lifetime_min         = 1.0f;
        float lifetime_max         = 3.0f;

        // Sphere
        float sphere_radius = 1.0f;

        enum class VelocityMode
        {
            None,                  // particles don't move
            Constant,              // all particles get initial_velocity exactly
            RandomRange,           // per-particle random in [velocity_min, velocity_max]
            OutwardConstant,       // direction = normalize(spawn_pos), speed = velocity_scale
            OutwardRandomRange,    // direction = normalize(spawn_pos), speed in [velocity_scale_min, velocity_scale_max]
        };

        // Velocity
        VelocityMode          velocity_mode = VelocityMode::OutwardRandomRange;
        math::Vector3 initial_velocity = math::Vector3(0, 5, 0);
        math::Vector3 velocity_min = math::Vector3(-5, -5, -5);
        math::Vector3 velocity_max = math::Vector3(5, 5, 5);
        float         velocity_scale = 5.0f;     // used by OutwardConstant
        float         velocity_scale_min = 2.0f;     // used by OutwardRandomRange
        float         velocity_scale_max = 8.0f;     // used by OutwardRandomRange

        enum class ScaleMode
        {
            Constant,             // all particles get scale_constant
            RandomRange,          // per-particle random in [scale_min, scale_max]
            LerpConstant,         // lerp from scale_start to scale_end over lifetime
            LerpRandomRange,      // lerp from random [scale_min, scale_max] to scale_end
        };

        // Scale
        ScaleMode     scale_mode = ScaleMode::Constant;
        math::Vector2 scale_constant = math::Vector2(1.0f, 1.0f);
        math::Vector2 scale_start = math::Vector2(1.0f, 1.0f); // LerpConstant start
        math::Vector2 scale_min = math::Vector2(0.2f, 0.2f);
        math::Vector2 scale_max = math::Vector2(1.0f, 1.0f);
        math::Vector2 scale_end = math::Vector2(0.0f, 0.0f); // lerp target at death

        enum class ColorMode
        {
            Constant,             // all particles get color_constant
            RandomRange,          // per-particle random in [color_min, color_max]
            LerpConstant,         // lerp from color_start to color_end over lifetime
            LerpRandomRange,      // lerp from random [color_min, color_max] to color_end
        };

        // Color
        ColorMode color_mode = ColorMode::Constant;
        Color     color_constant = Color::standard_white;
        Color     color_min = Color(0.0f, 1.0f, 0.0f, 1.0f);
        Color     color_max = Color(0.0f, 60/255.0f, 1.0f, 1.0f);
        Color     color_start = Color(1.0f, 146/255.0f, 0.0f, 1.0f); // LerpConstant start
        Color     color_end = Color::standard_red;   // lerp target at death

        // Physics
        math::Vector3 gravity = math::Vector3(0, -9.81f, 0);
        float         damping = 0.98f;

        // Threading
        static constexpr uint32_t k_thread_threshold = 1024;
};
}
