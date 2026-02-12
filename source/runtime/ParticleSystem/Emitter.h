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
    class EmitterModule;
    class RHI_Texture;

    class Emitter
    {
    public:
        bool enabled = true;
        std::string name = "New Emitter";

        float spawn_rate = 1000.0f;

        std::vector<EmitterModule*> initialization_modules;
        std::vector<EmitterModule*> update_modules;

        RHI_Texture* particle_texture;

    public:
        Emitter();
        ~Emitter();

        void ChangeSpawnRate(const float new_spawn_rate);

        void InitializeParticles(uint32_t start_index, uint32_t end_index);
        void Update(const double& delta_time);

    private:
        ParticleData particle_data;
        float spawn_accumulator = 0.0f;
    };
}
