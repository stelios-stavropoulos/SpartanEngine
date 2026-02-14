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
#include <RHI/RHI_Texture.h>
#include "../Resource/ResourceCache.h"
#include "../World/Components/Renderable.h"
//===========================================

namespace spartan
{
    Emitter::Emitter(Renderable* renderable)
    {
        renderable->SetMesh(MeshType::Quad);
        renderable->SetMaterial(Renderer::GetStandardMaterial());
        
        ChangeSpawnRate(spawn_rate);

        renderable->SetInstances(transforms);

        this->renderable = renderable;

        particle_texture = new RHI_Texture(ResourceCache::GetResourceDirectory(ResourceDirectory::Textures) + "/particle.png");
    }
    Emitter::~Emitter()
    {
        particle_data.particles.clear();

        initialization_modules.clear();
        update_modules.clear();

        delete(particle_texture);
    }

    void Emitter::ChangeSpawnRate(const float new_spawn_rate)
    {
        spawn_rate = new_spawn_rate;
        particle_data.max_particle_count = (uint32_t)spawn_rate * 2;

        if (particle_data.alive_particle_count > particle_data.max_particle_count)
        {
            particle_data.alive_particle_count = particle_data.max_particle_count;
        }

        particle_data.particles.resize(particle_data.max_particle_count);

        transforms.resize(particle_data.max_particle_count);
    }

    void Emitter::InitializeParticles(uint32_t start_index, uint32_t end_index)
    {
        for (EmitterModule* module : initialization_modules)
        {
            module->OnInitialize(particle_data, start_index, end_index);
        }
    }

    void Emitter::Update(const double& delta_time)
    {
        const float delta_time_float = static_cast<float>(delta_time);

        for (uint32_t i = 0; i < particle_data.alive_particle_count; )
        {
            particle_data.particles[i].normalized_lifetime += delta_time_float / particle_data.particles[i].lifetime;

            if (particle_data.particles[i].normalized_lifetime >= 1.0f)
            {
                if (particle_data.alive_particle_count == 0)
                {
                    continue;
                }
                // Swap with the last alive particle
                const uint32_t last_alive_index = particle_data.alive_particle_count - 1;
                particle_data.particles[i].lifetime = particle_data.particles[last_alive_index].lifetime;
                particle_data.particles[i].normalized_lifetime = particle_data.particles[last_alive_index].normalized_lifetime;
                particle_data.particles[i].position = particle_data.particles[last_alive_index].position;
                particle_data.particles[i].velocity = particle_data.particles[last_alive_index].velocity;
                particle_data.particles[i].color = particle_data.particles[last_alive_index].color;

                particle_data.particles[last_alive_index].normalized_lifetime = 1.0f;

                // Decrease alive count
                --particle_data.alive_particle_count;
            }
            else
            {
                Renderer::DrawSphere(
                    particle_data.particles[i].position,
                    0.03f,
                    8,
                    particle_data.particles[i].color
                );

                transforms[i] = math::Matrix::CreateTranslation(particle_data.particles[i].position);

                //Renderer::DrawIconAtPosition(particle_texture, particle_data.particles[i].position);

                ++i;
            }
        }

        renderable->SetInstances(transforms);

        for (EmitterModule* module : update_modules)
        {
            module->OnUpdate(particle_data, delta_time);
        }

        spawn_accumulator += static_cast<float>(delta_time) * spawn_rate;
        const uint32_t max_spawn_count = static_cast<uint32_t>(spawn_accumulator);
        spawn_accumulator -= static_cast<float>(max_spawn_count);

        if (max_spawn_count > 0)
        {
            const uint32_t start_index = particle_data.alive_particle_count;
            const uint32_t end_index = std::min(start_index + max_spawn_count, particle_data.max_particle_count);

            particle_data.alive_particle_count = end_index;
            InitializeParticles(start_index, end_index);
        }
    }
}
