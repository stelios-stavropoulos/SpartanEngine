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

//= INCLUDES ================================
#include "pch.h"
#include "Emitter.h"
#include "../Rendering/Renderer.h"
#include "../Rendering/Material.h"
#include "../Resource/ResourceCache.h"
#include "../World/Components/Renderable.h"
#include "../Profiling/Profiler.h"
#include "../RHI/RHI_Buffer.h"
#include "../Core/ThreadPool.h"
#if defined(__AVX2__)
#include <immintrin.h>
#endif
//===========================================

namespace spartan
{
    Emitter::Emitter(Renderable* renderable)
    {
        this->renderable = renderable;

        renderable->SetMesh(MeshType::Quad);

        std::shared_ptr<Material> material = std::make_shared<Material>();
        material->LoadFromFile(
            std::string(ResourceCache::GetProjectDirectory()) +
            "materials/ParticleDefault" +
            std::string(EXTENSION_MATERIAL)
        );

        renderable->SetMaterial(material);

        ChangeSpawnRate(spawn_rate);
    }

    void Emitter::ChangeSpawnRate(float new_spawn_rate)
    {
        spawn_rate = new_spawn_rate;

        // Capacity = 2 seconds worth
        particle_data.Resize(static_cast<uint32_t>(spawn_rate * lifetime_constant));

        renderable->SetParticleInstances(particle_data.max_particle_count);

        RHI_Buffer* instances_buffer = renderable->GetInstanceBuffer();
        if (!instances_buffer)
            return;
        buffer_data = static_cast<ParticleInstance*>(instances_buffer->GetMappedData());
    }

    inline uint32_t hash(uint32_t x)
    {
        x = (x ^ 61) ^ (x >> 16);
        x *= 9;
        x = x ^ (x >> 4);
        x *= 0x27d4eb2d;
        x = x ^ (x >> 15);
        return x;
    }

    inline float random_float(uint32_t seed)
    {
        return (hash(seed) & 0x00FFFFFF) / float(0x01000000);
    }

    void Emitter::Update(const double& delta_time)
    {
        //SP_LOG_INFO("%d particles", particle_data.alive_particle_count);

        const float dt = static_cast<float>(delta_time);

        uint64_t frame_id = Renderer::GetFrameNumber();

        SP_PROFILE_CPU_START("CPU Particles Update")

        const float grav_x = gravity.x * dt;
        const float grav_y = gravity.y * dt;
        const float grav_z = gravity.z * dt;
        const float damping_dt = 1.0f - (1.0f - damping) * dt;

        auto my_func = [this, dt, grav_x, grav_y, grav_z, damping_dt](int32_t start, int32_t end)
        {
#if defined(__AVX2__)
                const __m256 dt_ps = _mm256_set1_ps(dt);
                const __m256 damp_ps = _mm256_set1_ps(damping_dt);

                const __m256 gx = _mm256_set1_ps(grav_x);
                const __m256 gy = _mm256_set1_ps(grav_y);
                const __m256 gz = _mm256_set1_ps(grav_z);

                int32_t i = start;

                for (; i <= end - 8; i += 8)
                {
                    // -------- Lifetime --------
                    __m256 life = _mm256_load_ps(&particle_data.normalized_lifetimes[i]);
                    __m256 inv_life = _mm256_load_ps(&particle_data.inv_lifetimes[i]);
                    life = _mm256_add_ps(life, _mm256_mul_ps(dt_ps, inv_life));
                    _mm256_store_ps(&particle_data.normalized_lifetimes[i], life);

                    // -------- Velocity --------
                    __m256 vx = _mm256_load_ps(&particle_data.vel_x[i]);
                    __m256 vy = _mm256_load_ps(&particle_data.vel_y[i]);
                    __m256 vz = _mm256_load_ps(&particle_data.vel_z[i]);

                    vx = _mm256_mul_ps(_mm256_add_ps(vx, gx), damp_ps);
                    vy = _mm256_mul_ps(_mm256_add_ps(vy, gy), damp_ps);
                    vz = _mm256_mul_ps(_mm256_add_ps(vz, gz), damp_ps);

                    _mm256_store_ps(&particle_data.vel_x[i], vx);
                    _mm256_store_ps(&particle_data.vel_y[i], vy);
                    _mm256_store_ps(&particle_data.vel_z[i], vz);

                    // -------- Position --------
                    __m256 px = _mm256_load_ps(&particle_data.pos_x[i]);
                    __m256 py = _mm256_load_ps(&particle_data.pos_y[i]);
                    __m256 pz = _mm256_load_ps(&particle_data.pos_z[i]);

                    px = _mm256_add_ps(px, _mm256_mul_ps(vx, dt_ps));
                    py = _mm256_add_ps(py, _mm256_mul_ps(vy, dt_ps));
                    pz = _mm256_add_ps(pz, _mm256_mul_ps(vz, dt_ps));

                    _mm256_store_ps(&particle_data.pos_x[i], px);
                    _mm256_store_ps(&particle_data.pos_y[i], py);
                    _mm256_store_ps(&particle_data.pos_z[i], pz);

                    // -------- GPU write (scalar, unavoidable AoS) --------
                    for (int j = 0; j < 8; j++)
                    {
                        int idx = i + j;

                        buffer_data[idx].position.x = particle_data.pos_x[idx];
                        buffer_data[idx].position.y = particle_data.pos_y[idx];
                        buffer_data[idx].position.z = particle_data.pos_z[idx];

                        buffer_data[idx].scale.x = particle_data.scale_x[idx];
                        buffer_data[idx].scale.y = particle_data.scale_y[idx];

                        buffer_data[idx].color.r = particle_data.col_r[idx];
                        buffer_data[idx].color.g = particle_data.col_g[idx];
                        buffer_data[idx].color.b = particle_data.col_b[idx];
                        buffer_data[idx].color.a = particle_data.col_a[idx];
                    }
                }
#else
            for (int32_t i = start; i < end; ++i)
            {
                // Lifetime
                particle_data.normalized_lifetimes[i] += dt * particle_data.inv_lifetimes[i];

                // Physics: Velocity
                particle_data.vel_x[i] = (particle_data.vel_x[i] + grav_x) * damping_dt;
                particle_data.vel_y[i] = (particle_data.vel_y[i] + grav_y) * damping_dt;
                particle_data.vel_z[i] = (particle_data.vel_z[i] + grav_z) * damping_dt;

                // Physics: Position
                particle_data.pos_x[i] += particle_data.vel_x[i] * dt;
                particle_data.pos_y[i] += particle_data.vel_y[i] * dt;
                particle_data.pos_z[i] += particle_data.vel_z[i] * dt;

                // Update GPU buffer (AoS format for the shader)
                buffer_data[i].position.x = particle_data.pos_x[i];
                buffer_data[i].position.y = particle_data.pos_y[i];
                buffer_data[i].position.z = particle_data.pos_z[i];

                buffer_data[i].scale.x = particle_data.scale_x[i];
                buffer_data[i].scale.y = particle_data.scale_y[i];

                buffer_data[i].color.r = particle_data.col_r[i];
                buffer_data[i].color.g = particle_data.col_g[i];
                buffer_data[i].color.b = particle_data.col_b[i];
                buffer_data[i].color.a = particle_data.col_a[i];
            }
#endif
        };

        if (particle_data.alive_particle_count > 0)
            ThreadPool::ParallelLoop(my_func, static_cast<uint32_t>(particle_data.alive_particle_count));

        SP_PROFILE_CPU_END()

        SP_PROFILE_CPU_START("CPU Particles Kill")

        uint32_t i = 0;

        while (i < particle_data.alive_particle_count)
        {
            if (particle_data.normalized_lifetimes[i] >= 1.0f)
                Kill(i);
            else
                ++i;
        }

        SP_PROFILE_CPU_END()

        SP_PROFILE_CPU_START("CPU Particles Spawn")

        spawn_accumulator += dt * spawn_rate;

        uint32_t spawn_count = static_cast<uint32_t>(spawn_accumulator);
        spawn_accumulator -= static_cast<float>(spawn_count);

        uint32_t available =
            particle_data.max_particle_count -
            particle_data.alive_particle_count;

        spawn_count = std::min(spawn_count, available);

        if (spawn_count > 0)
        {
            SpawnParticles(spawn_count, frame_id);
        }

        SP_PROFILE_CPU_END()
    }

    void Emitter::SpawnParticles(uint32_t count, uint64_t frame_id)
    {
        uint32_t start = particle_data.alive_particle_count;
        uint32_t end = start + count;

        for (uint32_t i = start; i < end; ++i)
        {
            particle_data.inv_lifetimes[i] = 1.0f / lifetime_constant;
            particle_data.normalized_lifetimes[i] = 0.0f;

            math::Vector3 pos = RandomPointInSphere(sphere_radius, i, frame_id);
            particle_data.pos_x[i] = pos.x;
            particle_data.pos_y[i] = pos.y;
            particle_data.pos_z[i] = pos.z;

            particle_data.vel_x[i] = initial_velocity.x;
            particle_data.vel_y[i] = initial_velocity.y;
            particle_data.vel_z[i] = initial_velocity.z;

            particle_data.scale_x[i] = scale_constant.x;
            particle_data.scale_y[i] = scale_constant.y;

            particle_data.col_r[i] = color_constant.r;
            particle_data.col_g[i] = color_constant.g;
            particle_data.col_b[i] = color_constant.b;
            particle_data.col_a[i] = color_constant.a;
        }

        particle_data.alive_particle_count = end;
    }

    void Emitter::Kill(uint32_t index)
    {
        uint32_t last = particle_data.alive_particle_count - 1;

        if (index != last)
        {
            // Swap all individual float components
            particle_data.pos_x[index] = particle_data.pos_x[last];
            particle_data.pos_y[index] = particle_data.pos_y[last];
            particle_data.pos_z[index] = particle_data.pos_z[last];

            particle_data.vel_x[index] = particle_data.vel_x[last];
            particle_data.vel_y[index] = particle_data.vel_y[last];
            particle_data.vel_z[index] = particle_data.vel_z[last];

            particle_data.col_r[index] = particle_data.col_r[last];
            particle_data.col_g[index] = particle_data.col_g[last];
            particle_data.col_b[index] = particle_data.col_b[last];
            particle_data.col_a[index] = particle_data.col_a[last];

            particle_data.scale_x[index] = particle_data.scale_x[last];
            particle_data.scale_y[index] = particle_data.scale_y[last];

            particle_data.inv_lifetimes[index] = particle_data.inv_lifetimes[last];
            particle_data.normalized_lifetimes[index] = particle_data.normalized_lifetimes[last];
        }

        --particle_data.alive_particle_count;
    }

    inline math::Vector3 Emitter::RandomPointInSphere(float radius, uint32_t particle_id, uint64_t frame_id)
    {
        // build deterministic seeds
        uint32_t seed0 = particle_id * 73856093u ^ (uint32_t)frame_id;
        uint32_t seed1 = particle_id * 19349663u ^ (uint32_t)(frame_id >> 32);
        uint32_t seed2 = particle_id * 83492791u ^ (uint32_t)(frame_id + 17);
        uint32_t seed3 = particle_id * 2654435761u ^ (uint32_t)(frame_id + 101);

        float x = random_float(seed0) * 2.0f - 1.0f;
        float y = random_float(seed1) * 2.0f - 1.0f;
        float z = random_float(seed2) * 2.0f - 1.0f;

        float inv_mag = 1.0f / std::sqrt(x * x + y * y + z * z + 1e-8f);

        float u = random_float(seed3);
        float scale = radius * u * u;

        return math::Vector3(
            x * inv_mag * scale,
            y * inv_mag * scale,
            z * inv_mag * scale
        );
    }
}
