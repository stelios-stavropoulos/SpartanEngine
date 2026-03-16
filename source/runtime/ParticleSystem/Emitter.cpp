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
        SP_PROFILE_CPU_START("CPU Particles Update")

        const float dt = static_cast<float>(delta_time);

        uint64_t frame_id = Renderer::GetFrameNumber();

        const math::Vector3 gravity_dt = gravity * dt;
        float damping_dt = 1.0f - (1.0f - damping) * dt;

        int32_t i = 0;

        int32_t last_alive_index = (int32_t)particle_data.alive_particle_count - 1;

        auto my_func = [this, dt, gravity_dt, damping_dt](int32_t start, int32_t end) {
                for (int32_t i = start; i < end - 3; i+=4)
                {
                    // Lifetime
                    particle_data.normalized_lifetimes[i] += dt * particle_data.inv_lifetimes[i];
                    particle_data.normalized_lifetimes[i + 1] += dt * particle_data.inv_lifetimes[i + 1];
                    particle_data.normalized_lifetimes[i + 2] += dt * particle_data.inv_lifetimes[i + 2];
                    particle_data.normalized_lifetimes[i + 3] += dt * particle_data.inv_lifetimes[i + 3];
                    //if (particle_data.normalized_lifetimes[i] >= 1.0f)
                    //{
                    //    Kill(i);
                    //    continue;
                    //}
                    // Color
                    // Physics
                    particle_data.velocities[i] = (particle_data.velocities[i] + gravity_dt) * damping_dt;
                    particle_data.velocities[i + 1] = (particle_data.velocities[i + 1] + gravity_dt) * damping_dt;
                    particle_data.velocities[i + 2] = (particle_data.velocities[i + 2] + gravity_dt) * damping_dt;
                    particle_data.velocities[i + 3] = (particle_data.velocities[i + 3] + gravity_dt) * damping_dt;
                    particle_data.positions[i] += particle_data.velocities[i] * dt;
                    particle_data.positions[i + 1] += particle_data.velocities[i + 1] * dt;
                    particle_data.positions[i + 2] += particle_data.velocities[i + 2] * dt;
                    particle_data.positions[i + 3] += particle_data.velocities[i + 3] * dt;
                    particle_data.positions[i + 4] += particle_data.velocities[i + 4] * dt;
                    // Update GPU buffer
                    buffer_data[i].position = particle_data.positions[i];
                    buffer_data[i + 1].position = particle_data.positions[i + 1];
                    buffer_data[i + 2].position = particle_data.positions[i + 2];
                    buffer_data[i + 3].position = particle_data.positions[i + 3];
                    buffer_data[i].scale = particle_data.scales[i];
                    buffer_data[i + 1].scale = particle_data.scales[i + 1];
                    buffer_data[i + 2].scale = particle_data.scales[i + 2];
                    buffer_data[i + 3].scale = particle_data.scales[i + 3];
                    buffer_data[i].color = particle_data.colors[i];
                    buffer_data[i + 1].color = particle_data.colors[i + 1];
                    buffer_data[i + 2].color = particle_data.colors[i + 2];
                    buffer_data[i + 3].color = particle_data.colors[i + 3];
                }
            };

        int32_t thread_count = ThreadPool::GetIdleThreadCount();

        int32_t chunk_size = particle_data.max_particle_count / thread_count;

        ThreadPool::ParallelLoop(my_func, static_cast<uint32_t>(particle_data.max_particle_count));

        //for (int32_t t = 0; t < thread_count; ++t)
        //{
        //    int32_t start = t * chunk_size;
        //    int32_t end = (t == thread_count - 1) ? particle_data.max_particle_count : start + chunk_size;

        //    ThreadPool::AddTask([this, dt, gravity_dt, damping_dt, start, end]() {
        //        for (int32_t i = start; i < end - 4; i+=4)
        //        {
        //            // Lifetime
        //            particle_data.normalized_lifetimes[i] += dt * particle_data.inv_lifetimes[i];
        //            particle_data.normalized_lifetimes[i+1] += dt * particle_data.inv_lifetimes[i+1];
        //            particle_data.normalized_lifetimes[i+2] += dt * particle_data.inv_lifetimes[i+2];
        //            particle_data.normalized_lifetimes[i+3] += dt * particle_data.inv_lifetimes[i+3];
        //            //if (particle_data.normalized_lifetimes[i] >= 1.0f)
        //            //{
        //            //    Kill(i);
        //            //    continue;
        //            //}
        //            // Color
        //            // Physics
        //            particle_data.velocities[i]   = (particle_data.velocities[i] + gravity_dt) * damping_dt;
        //            particle_data.velocities[i+1] = (particle_data.velocities[i+1] + gravity_dt) * damping_dt;
        //            particle_data.velocities[i+2] = (particle_data.velocities[i+2] + gravity_dt) * damping_dt;
        //            particle_data.velocities[i+3] = (particle_data.velocities[i+3] + gravity_dt) * damping_dt;
        //            particle_data.positions[i]   += particle_data.velocities[i] * dt;
        //            particle_data.positions[i+1] += particle_data.velocities[i+1] * dt;
        //            particle_data.positions[i+2] += particle_data.velocities[i+2] * dt;
        //            particle_data.positions[i+3] += particle_data.velocities[i+3] * dt;
        //            // Update GPU buffer
        //            buffer_data[i].position   = particle_data.positions[i];
        //            buffer_data[i+1].position = particle_data.positions[i+1];
        //            buffer_data[i+2].position = particle_data.positions[i+2];
        //            buffer_data[i+3].position = particle_data.positions[i+3];
        //            buffer_data[i].scale      = particle_data.scales[i];
        //            buffer_data[i+1].scale    = particle_data.scales[i+1];
        //            buffer_data[i+2].scale    = particle_data.scales[i+2];
        //            buffer_data[i+3].scale    = particle_data.scales[i+3];
        //            buffer_data[i].color      = particle_data.colors[i];
        //            buffer_data[i+1].color    = particle_data.colors[i+1];
        //            buffer_data[i+2].color    = particle_data.colors[i+2];
        //            buffer_data[i+3].color    = particle_data.colors[i+3];
        //        }
        //    });
        //}

        //ThreadPool::Flush();

        //while (i < last_alive_index)
        //{
        //    //// Lifetime
        //    //particle_data.normalized_lifetimes[i] += dt * particle_data.inv_lifetimes[i];

        //    //if (particle_data.normalized_lifetimes[i] >= 1.0f)
        //    //{
        //    //    Kill(i);
        //    //    continue;
        //    //}

        //    //// Color

        //    //// Physics
        //    //particle_data.velocities[i] += gravity_dt;
        //    //particle_data.positions[i] += particle_data.velocities[i] * dt;
        //    //particle_data.velocities[i] *= damping_dt;

        //    // Update GPU buffer
        //    buffer_data[i].position = particle_data.positions[i];
        //    buffer_data[i].scale = particle_data.scales[i];
        //    buffer_data[i].color = particle_data.colors[i];

        //    ++i;
        //}

        //renderable->SetParticleInstances();
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
            // Lifetime
            float lifetime = lifetime_constant;

            particle_data.inv_lifetimes[i] = 1.0f / lifetime;
            particle_data.normalized_lifetimes[i] = 0.0f;

            // Position
            particle_data.positions[i] = RandomPointInSphere(sphere_radius, i, frame_id);

            // Velocity
            particle_data.velocities[i] = initial_velocity;

            // Scale
            particle_data.scales[i] = scale_constant;

            // Color
            particle_data.colors[i] = color_constant;
        }

        particle_data.alive_particle_count = end;
    }

    void Emitter::Kill(uint32_t index)
    {
        uint32_t last = particle_data.alive_particle_count - 1;

        if (index != last)
        {
            particle_data.positions[index] = particle_data.positions[last];
            particle_data.velocities[index] = particle_data.velocities[last];
            particle_data.colors[index] = particle_data.colors[last];
            particle_data.scales[index] = particle_data.scales[last];
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
