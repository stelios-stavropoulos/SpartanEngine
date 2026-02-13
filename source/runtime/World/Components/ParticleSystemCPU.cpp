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
//===========================================

namespace spartan
{

    spartan::ParticleSystemCPU::ParticleSystemCPU(Entity* entity) : Component(entity)
    {
        SP_REGISTER_ATTRIBUTE_VALUE_VALUE(emitters, std::vector<Emitter* >);
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
        SP_PROFILE_CPU_START("Particle System")

            const double dt = Timer::GetDeltaTimeSec();

        for (Emitter* emitter : emitters)
        {
            emitter->Update(dt);
        }

        SP_PROFILE_CPU_END()
    }

}
