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

//= INCLUDES ================================
#include "pch.h"
#include "../ImGui/ImGui_Extension.h"
#include "ButtonColorPicker.h"
#include "EmitterViewer.h"
#include "../runtime/ParticleSystem/EmitterModule.h"

#include "../runtime/ParticleSystem/EmitterModules/EM_AddVelocity.h"
#include "../runtime/ParticleSystem/EmitterModules/EM_ApplyVelocity.h"
#include "../runtime/ParticleSystem/EmitterModules/EM_Gravity.h"
#include "../runtime/ParticleSystem/EmitterModules/EM_SetLifetime.h"
#include "../runtime/ParticleSystem/EmitterModules/EM_SphereShape.h"
#include "../runtime/ParticleSystem/EmitterModules/EM_SetColor.h"
//===========================================

namespace spartan
{
    std::vector<std::string> EmitterViewer::GetAvailableInitializationModules() {
        return { "Set Lifetime", "Sphere Shape", "Add Velocity", "Set Color" };
    }

    std::vector<std::string> EmitterViewer::GetAvailableUpdateModules() {
        return { "Add Velocity", "Gravity", "Apply Velocity", "Set Color" };
    }

    std::string EmitterViewer::TypeToString(EmitterModuleType type) {
        if (type == EmitterModuleType::Gravity)         return "Gravity";
        if (type == EmitterModuleType::SphereShape)     return "Sphere Shape";
        if (type == EmitterModuleType::AddVelocity)     return "Add Velocity";
        if (type == EmitterModuleType::ApplyVelocity)   return "Apply Velocity";
        if (type == EmitterModuleType::SetLifetime)     return "Set Lifetime";
        if (type == EmitterModuleType::SetColor)        return "Set Color";

        assert(false && "TypeToString: Unknown EmitterModuleType");
        return {};        
    }

    EmitterModuleType EmitterViewer::StringToType(const std::string& name) {
            if (name == "Gravity")          return EmitterModuleType::Gravity;
            if (name == "Sphere Shape")     return EmitterModuleType::SphereShape;
            if (name == "Add Velocity")     return EmitterModuleType::AddVelocity;
            if (name == "Apply Velocity")   return EmitterModuleType::ApplyVelocity;
            if (name == "Set Lifetime")     return EmitterModuleType::SetLifetime;
            if (name == "Set Color")        return EmitterModuleType::SetColor;
    
            assert(false && "StringToType: Unknown module name");
            return EmitterModuleType::Max;        
    }

        ButtonColorPicker& EmitterViewer::GetColorPicker(const char* label, const void* key)
        {
            static std::unordered_map<const void*, ButtonColorPicker> pickers;

            auto it = pickers.find(key);
            if (it == pickers.end())
            {
                std::string title =
                    "Particle System Color Picker " + std::string(label) + "##" +
                    std::to_string(reinterpret_cast<uintptr_t>(key));

                it = pickers.emplace(key, ButtonColorPicker(title)).first;
            }

            return it->second;
        }
    
        void EmitterViewer::DrawValue(const char* label, float* value) {
            ImGui::DragFloat(label, value, 0.1f);
        }
    
        void EmitterViewer::DrawValue(const char* label, math::Vector3* value) {
            float arr[3] = { value->x, value->y, value->z };
            if (ImGui::DragFloat3(label, arr, 0.1f)) {
                value->x = arr[0];
                value->y = arr[1];
                value->z = arr[2];
            }
        }
    
        void EmitterViewer::DrawValue(const char* label, spartan::Color* value) {
            ImGui::PushID(label);

            ButtonColorPicker& picker = GetColorPicker(label, value);

            picker.SetColor(*value);
            picker.Update();
            *value = picker.GetColor();

            ImGui::PopID();
        }
    
        void EmitterViewer::DrawProperty(const char* name, ParticleFloat& property)
        {
            ImGui::PushID(name);

            float dropdown_width = 120.0f;
            float dropdown_pos_x = ImGui::GetContentRegionAvail().x - dropdown_width;

            // Line 1: Label and Type Selector
            ImGui::Text(name);
            ImGui::SameLine(dropdown_pos_x);
            ImGui::SetNextItemWidth(dropdown_width);

            int type = static_cast<int>(property.type);
            if (ImGui::Combo("##type", &type, "Constant\0Range\0Curve\0"))
            {
                property.type = static_cast<particle_property_type>(type);
            }

            // Line 2: Values aligned under the dropdown
            ImGui::SetCursorPosX(dropdown_pos_x);
            ImGui::BeginGroup();
            {
                if (property.type == particle_property_type::CONSTANT)
                {
                    ImGui::SetNextItemWidth(dropdown_width);
                    DrawValue("##value", &property.const_value);
                }
                else if (property.type == particle_property_type::RANGE)
                {

                    ImGui::SetNextItemWidth(dropdown_width);
                    DrawValue("Min", &property.range_value.first);

                    ImGui::SetCursorPosX(dropdown_pos_x);
                    ImGui::SetNextItemWidth(dropdown_width);
                    DrawValue("Max", &property.range_value.second);
                }
            }
            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::PopID();
        }
    
        void EmitterViewer::DrawProperty(const char* name, ParticleVector3& property)
        {
            ImGui::PushID(name);

            float dropdown_width = 120.0f;
            float dropdown_pos_x = ImGui::GetContentRegionAvail().x - dropdown_width;

            ImGui::Text(name);
            ImGui::SameLine(dropdown_pos_x);
            ImGui::SetNextItemWidth(dropdown_width);

            int type = static_cast<int>(property.type);
            if (ImGui::Combo("##type", &type, "Constant\0Range\0Curve\0"))
            {
                property.type = static_cast<particle_property_type>(type);
            }

            ImGui::SetCursorPosX(dropdown_pos_x);
            ImGui::BeginGroup();
            {
                if (property.type == particle_property_type::CONSTANT)
                {
                    ImGui::SetNextItemWidth(dropdown_width);
                    DrawValue("Const", &property.const_value);
                }
                else if (property.type == particle_property_type::RANGE)
                {
                    ImGui::SetNextItemWidth(dropdown_width);
                    DrawValue("Min", &property.range_value.first);

                    ImGui::SetCursorPosX(dropdown_pos_x);
                    ImGui::SetNextItemWidth(dropdown_width);
                    DrawValue("Max", &property.range_value.second);
                }
            }
            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::PopID();
        }
    
        void EmitterViewer::DrawProperty(const char* name, ParticleColor& property)
        {
            ImGui::PushID(name);

            float dropdown_width = 120.0f;
            float dropdown_pos_x = ImGui::GetContentRegionAvail().x - dropdown_width;

            ImGui::Text(name);
            ImGui::SameLine(dropdown_pos_x);
            ImGui::SetNextItemWidth(dropdown_width);

            int type = static_cast<int>(property.type);
            if (ImGui::Combo("##type", &type, "Constant\0Range\0Curve\0"))
            {
                property.type = static_cast<particle_property_type>(type);
            }

            ImGui::SetCursorPosX(dropdown_pos_x);
            ImGui::BeginGroup();
            {
                if (property.type == particle_property_type::CONSTANT)
                {
                    ImGui::SetNextItemWidth(dropdown_width);
                    DrawValue("##const_val", &property.const_value);
                }
                else if (property.type == particle_property_type::RANGE)
                {
                    ImGui::SetNextItemWidth(dropdown_width);
                    DrawValue("Min", &property.range_value.first);

                    ImGui::SetCursorPosX(dropdown_pos_x);
                    ImGui::SetNextItemWidth(dropdown_width);
                    DrawValue("Max", &property.range_value.second);
                }
            }
            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::PopID();
        }
    
        void EmitterViewer::ShowEmitter(EmitterModule* module)
        {
            if (!module) return;

            static ButtonColorPicker picker_const("Color Picker - Constant");
            static ButtonColorPicker picker_min("Color Picker - Min");
            static ButtonColorPicker picker_max("Color Picker - Max");

            switch (module->GetType())
            {
            case EmitterModuleType::Gravity:
            {
                EM_Gravity* gravity_module = static_cast<EM_Gravity*>(module);

                DrawProperty("Gravity Vector", gravity_module->gravity);
            }
            break;
            case EmitterModuleType::AddVelocity:
            {
                EM_AddVelocity* add_velocity_module = static_cast<EM_AddVelocity*>(module);

                DrawProperty("Velocity Vector", add_velocity_module->velocity);
            }
            break;
            case EmitterModuleType::SetLifetime:
            {
                EM_SetLifetime* set_lifetime_module = static_cast<EM_SetLifetime*>(module);
                DrawProperty("Lifetime (seconds)", set_lifetime_module->lifetime);
            }
            break;
            case EmitterModuleType::ApplyVelocity:
            {
                EM_ApplyVelocity* apply_velocity_module = static_cast<EM_ApplyVelocity*>(module);
                DrawProperty("Damping", apply_velocity_module->damping);
            }
            break;
            case EmitterModuleType::SetColor:
            {
                EM_SetColor* set_color_module = static_cast<EM_SetColor*>(module);
                DrawProperty("Particle Color", set_color_module->color);
            }
            break;
            case EmitterModuleType::SphereShape:
            {
                EM_SphereShape* shpere_shape = static_cast<EM_SphereShape*>(module);
                DrawProperty("Sphere Radius", shpere_shape->radius);
            }
            break;
            default:
                break;
            }
        }
}
