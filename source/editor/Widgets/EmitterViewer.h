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

//= INCLUDES ======
#include "ButtonColorPicker.h"
//=================

namespace spartan
{
    struct ParticleFloat;
    struct ParticleVector3;
    struct ParticleColor;
    class EmitterModule;
    class Color;
    enum class EmitterModuleType : uint32_t;

    class EmitterViewer
    {
    public:
        static std::vector<std::string> GetAvailableInitializationModules();

        static std::vector<std::string> GetAvailableUpdateModules();

        static std::string TypeToString(EmitterModuleType type);

        static EmitterModuleType StringToType(const std::string& name);

        static ButtonColorPicker& GetColorPicker(const char* label, const void* key);

        static void DrawValue(const char* label, float* value);

        static void DrawValue(const char* label, math::Vector3* value);

        static void DrawValue(const char* label, spartan::Color* value);

        static void DrawProperty(const char* name, ParticleFloat& property);

        static void DrawProperty(const char* name, ParticleVector3& property);

        static void DrawProperty(const char* name, ParticleColor& property);

        static void ShowEmitter(EmitterModule* module);
    };
}
