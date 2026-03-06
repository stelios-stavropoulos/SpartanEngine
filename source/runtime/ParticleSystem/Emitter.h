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
#include "../Rendering/Instance.h"
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

        void SpawnParticles(uint32_t count);
        void Kill(uint32_t index);
        math::Vector3 RandomPointInSphere(float radius);

    private:

        Renderable* renderable = nullptr;
        Instance* buffer_data = nullptr;

        ParticleData particle_data;

        // Spawn
        float spawn_rate = 30000.0f;
        float spawn_accumulator = 0.0f;

        // Lifetime
        bool lifetime_is_constant = false;
        float lifetime_constant = 2.0f;
        float lifetime_min = 1.0f;
        float lifetime_max = 3.0f;

        // Sphere
        float sphere_radius = 1.0f;

        // Velocity
        math::Vector3 initial_velocity = math::Vector3(0, 5, 0);

        // Scale
        bool scale_is_constant = false;
        math::Vector2 scale_constant = math::Vector2(1, 1);
        math::Vector2 scale_min = math::Vector2(0.2f, 0.2f);
        math::Vector2 scale_max = math::Vector2(1.0f, 1.0f);

        // Color
        bool color_is_constant = true;
        Color color_constant = Color::standard_white;
        Color color_min = Color::standard_white;
        Color color_max = Color::standard_red;

        // Physics
        math::Vector3 gravity = math::Vector3(0, -9.81f, 0);
        float damping = 0.98f;
    };
}
