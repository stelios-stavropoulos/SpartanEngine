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
#include "ParticleSystemCPU.h"
#include "../../ParticleSystem/Emitter.h"
#include "../../World/Entity.h"
#include "../../Profiling/Profiler.h"
#include <fstream>
#include <chrono>
//===========================================

namespace spartan
{


    spartan::ParticleSystemCPU::ParticleSystemCPU(Entity* entity) : Component(entity)
    {
        SP_REGISTER_ATTRIBUTE_VALUE_VALUE(emitters, std::vector<Emitter* >);

        BenchmarkBegin("particle_benchmark.csv");
    }

    ParticleSystemCPU::~ParticleSystemCPU()
    {
        for (Emitter* emitter : emitters)
        {
            delete emitter; // replace
        }
        emitters.clear();
    }

    void spartan::ParticleSystemCPU::Tick()
    {
        //SP_PROFILE_CPU_START("Particle System")

        auto t_begin = std::chrono::high_resolution_clock::now();

        const double dt = Timer::GetDeltaTimeSec();

        for (Emitter* emitter : emitters)
        {
            emitter->Update(dt);
        }

        auto t_end = std::chrono::high_resolution_clock::now();

        BenchmarkTick(dt);

        //SP_PROFILE_CPU_END()
    }

    void ParticleSystemCPU::BenchmarkBegin(const std::string& path)
    {
        benchmark_file.open(path);

        benchmark_file
            << "variant_index,"
            << "lifetime,"
            << "scale,"
            << "color,"
            << "velocity,"
            << "avg_frame_ms,"
            << "avg_system_ms,"
            << "avg_update_ms,"
            << "avg_kill_ms,"
            << "avg_spawn_ms,"
            << "sample_frames"
            << std::endl;

        benchmark_variants.clear();

        const auto lifetimes =
        {
            Emitter::LifetimeMode::Constant,
            Emitter::LifetimeMode::RandomRange
        };

        const auto scales =
        {
            Emitter::ScaleMode::Constant,
            Emitter::ScaleMode::RandomRange,
            Emitter::ScaleMode::LerpConstant,
            Emitter::ScaleMode::LerpRandomRange
        };

        const auto colors =
        {
            Emitter::ColorMode::Constant,
            Emitter::ColorMode::RandomRange,
            Emitter::ColorMode::LerpConstant,
            Emitter::ColorMode::LerpRandomRange
        };

        const auto velocities =
        {
            Emitter::VelocityMode::None,
            Emitter::VelocityMode::Constant,
            Emitter::VelocityMode::RandomRange,
            Emitter::VelocityMode::OutwardConstant,
            Emitter::VelocityMode::OutwardRandomRange
        };

        for (auto lifetime : lifetimes)
        {
            for (auto scale : scales)
            {
                for (auto color : colors)
                {
                    for (auto velocity : velocities)
                    {
                        benchmark_variants.push_back(
                            {
                                lifetime,
                                scale,
                                color,
                                velocity
                            });
                    }
                }
            }
        }

        benchmark_variant_index = 0;
        benchmark_running = true;

        BenchmarkResetAccumulators();
        BenchmarkApplyVariant();
    }

    static const char* ToString(Emitter::LifetimeMode mode)
    {
        switch (mode)
        {
        case Emitter::LifetimeMode::Constant: return "Constant";
        case Emitter::LifetimeMode::RandomRange: return "RandomRange";
        }

        return "Unknown";
    }

    static const char* ToString(Emitter::ScaleMode mode)
    {
        switch (mode)
        {
        case Emitter::ScaleMode::Constant: return "Constant";
        case Emitter::ScaleMode::RandomRange: return "RandomRange";
        case Emitter::ScaleMode::LerpConstant: return "LerpConstant";
        case Emitter::ScaleMode::LerpRandomRange: return "LerpRandomRange";
        }

        return "Unknown";
    }

    static const char* ToString(Emitter::ColorMode mode)
    {
        switch (mode)
        {
        case Emitter::ColorMode::Constant: return "Constant";
        case Emitter::ColorMode::RandomRange: return "RandomRange";
        case Emitter::ColorMode::LerpConstant: return "LerpConstant";
        case Emitter::ColorMode::LerpRandomRange: return "LerpRandomRange";
        }

        return "Unknown";
    }

    static const char* ToString(Emitter::VelocityMode mode)
    {
        switch (mode)
        {
        case Emitter::VelocityMode::None: return "None";
        case Emitter::VelocityMode::Constant: return "Constant";
        case Emitter::VelocityMode::RandomRange: return "RandomRange";
        case Emitter::VelocityMode::OutwardConstant: return "OutwardConstant";
        case Emitter::VelocityMode::OutwardRandomRange: return "OutwardRandomRange";
        }

        return "Unknown";
    }

    void ParticleSystemCPU::BenchmarkApplyVariant()
    {
        if (emitters.empty())
            return;

        auto& variant = benchmark_variants[benchmark_variant_index];

        Emitter* emitter = emitters[0];

        emitter->benchmark_mode = true;
        emitter->benchmark_seed = benchmark_variant_index + 1;

        emitter->ApplyBenchmarkVariant(
            variant.lifetime,
            variant.scale,
            variant.color,
            variant.velocity
        );

        benchmark_elapsed = 0.0;

        BenchmarkResetAccumulators();

        SP_LOG_INFO(
            "Benchmark variant %u / %zu",
            benchmark_variant_index + 1,
            benchmark_variants.size()
        );
    }

    void ParticleSystemCPU::BenchmarkResetAccumulators()
    {
        benchmark_sample_elapsed = 0.0;

        benchmark_sample_count = 0;

        benchmark_frame_ms_sum = 0.0;
        benchmark_system_ms_sum = 0.0;
        benchmark_update_ms_sum = 0.0;
        benchmark_kill_ms_sum = 0.0;
        benchmark_spawn_ms_sum = 0.0;
    }

    void ParticleSystemCPU::BenchmarkWriteCurrent()
    {
        auto& variant = benchmark_variants[benchmark_variant_index];

        const double inv = benchmark_sample_count > 0
            ? 1.0 / static_cast<double>(benchmark_sample_count)
            : 0.0;

        benchmark_file
            << benchmark_variant_index << ","
            << ToString(variant.lifetime) << ","
            << ToString(variant.scale) << ","
            << ToString(variant.color) << ","
            << ToString(variant.velocity) << ","
            << benchmark_frame_ms_sum * inv << ","
            << benchmark_system_ms_sum * inv << ","
            << benchmark_update_ms_sum * inv << ","
            << benchmark_kill_ms_sum * inv << ","
            << benchmark_spawn_ms_sum * inv << ","
            << benchmark_sample_count
            << std::endl;

        benchmark_file.flush();
    }

    void ParticleSystemCPU::BenchmarkTick(double dt)
    {
        if (!benchmark_running)
            return;

        benchmark_elapsed += dt;

        const bool sample_window =
            benchmark_elapsed >= 5.0 &&
            benchmark_elapsed < 15.0;

        if (sample_window)
        {
            benchmark_sample_count++;

            benchmark_frame_ms_sum += Timer::GetDeltaTimeMs();

            if (!emitters.empty())
            {
                Emitter* emitter = emitters[0];

                benchmark_update_ms_sum += emitter->benchmark_metrics.update_ms;
                benchmark_kill_ms_sum += emitter->benchmark_metrics.kill_ms;
                benchmark_spawn_ms_sum += emitter->benchmark_metrics.spawn_ms;

                benchmark_system_ms_sum +=
                    emitter->benchmark_metrics.update_ms +
                    emitter->benchmark_metrics.kill_ms +
                    emitter->benchmark_metrics.spawn_ms;
            }
        }

        if (benchmark_elapsed >= 15.0)
        {
            BenchmarkWriteCurrent();

            benchmark_variant_index++;

            if (benchmark_variant_index >= benchmark_variants.size())
            {
                benchmark_running = false;

                benchmark_file.close();

                SP_LOG_INFO("Particle benchmark completed");

                return;
            }

            BenchmarkApplyVariant();
        }
    }

}
