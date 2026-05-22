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
#include "Component.h"
#include <vector>
//============================================

#include "../../ParticleSystem/Emitter.h"

namespace spartan
{
    //class Emitter;

    struct ParticleBenchmarkVariant
    {
        Emitter::LifetimeMode lifetime;
        Emitter::ScaleMode scale;
        Emitter::ColorMode color;
        Emitter::VelocityMode velocity;
    };

    class ParticleSystemCPU : public Component
    {

    public:
        std::vector<Emitter*> emitters;

        bool benchmark_running = false;

        std::vector<ParticleBenchmarkVariant> benchmark_variants;

        uint32_t benchmark_variant_index = 0;

        double benchmark_elapsed = 0.0;
        double benchmark_sample_elapsed = 0.0;

        uint64_t benchmark_sample_count = 0;

        double benchmark_frame_ms_sum = 0.0;
        double benchmark_system_ms_sum = 0.0;
        double benchmark_update_ms_sum = 0.0;
        double benchmark_kill_ms_sum = 0.0;
        double benchmark_spawn_ms_sum = 0.0;

        std::ofstream benchmark_file;

    public:
        ParticleSystemCPU(Entity* entity);
        ~ParticleSystemCPU();

        // icomponent
        void Tick() override;

        void BenchmarkBegin(const std::string& path);
        void BenchmarkTick(double dt);
        void BenchmarkApplyVariant();
        void BenchmarkWriteCurrent();
        void BenchmarkResetAccumulators();
    };
}
