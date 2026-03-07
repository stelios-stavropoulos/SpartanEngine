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
#include "EmitterModule.h"
#include "../Rendering/Renderer.h"
#include "../Rendering/Material.h"
#include <RHI/RHI_Texture.h>
#include "../Resource/ResourceCache.h"
#include "../World/Components/Renderable.h"
#include "../Profiling/Profiler.h"
#include "../RHI/RHI_Buffer.h"
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
        particle_data.Resize(static_cast<uint32_t>(spawn_rate * 2.0f));

        renderable->SetParticleInstances(particle_data.max_particle_count);

        RHI_Buffer* instances_buffer = renderable->GetInstanceBuffer();
        if (!instances_buffer)
            return;
        buffer_data = static_cast<Instance*>(instances_buffer->GetMappedData());
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

        uint32_t i = 0;

        while (i < particle_data.alive_particle_count)
        {
            // Lifetime
            particle_data.normalized_lifetimes[i] += dt / particle_data.lifetimes[i];

            if (particle_data.normalized_lifetimes[i] >= 1.0f)
            {
                Kill(i);
                continue;
            }

            // Physics
            particle_data.velocities[i] += gravity_dt;
            particle_data.positions[i] += particle_data.velocities[i] * dt;
            particle_data.velocities[i] *= damping_dt;

            float life_t = particle_data.normalized_lifetimes[i];

            // GPU instance update
            buffer_data[i].SetParticleInstanceData(
                particle_data.positions[i],
                particle_data.scales[i]
            );

            ++i;
        }

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

            particle_data.lifetimes[i] = lifetime;
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
            particle_data.lifetimes[index] = particle_data.lifetimes[last];
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
