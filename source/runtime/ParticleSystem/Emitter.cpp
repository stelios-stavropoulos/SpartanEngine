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
    //==========================================================================
    // Helpers
    //==========================================================================

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

    //==========================================================================
    // Hot update path — 4 compiled variants, zero branches inside
    // ColorLerp : lerp col_r/g/b/a from spawn color to color_end over lifetime
    // ScaleLerp : lerp scale_x/y   from spawn scale to scale_end over lifetime
    //==========================================================================

    template<bool ColorLerp, bool ScaleLerp>
    static void UpdateParticles(Emitter::EmitterUpdateContext& ctx, int32_t start, int32_t end)
    {
        ParticleData& pd = *ctx.data;

#if defined(__AVX2__)
        const __m256 dt_ps = _mm256_set1_ps(ctx.dt);
        const __m256 damp_ps = _mm256_set1_ps(ctx.damping_dt);
        const __m256 gx = _mm256_set1_ps(ctx.grav_x);
        const __m256 gy = _mm256_set1_ps(ctx.grav_y);
        const __m256 gz = _mm256_set1_ps(ctx.grav_z);
        const __m256 one_ps = _mm256_set1_ps(1.0f);

        // Broadcast lerp endpoints once — instructions emitted only when
        // the corresponding template param is true
        [[maybe_unused]] const __m256 end_r = ColorLerp ? _mm256_set1_ps(ctx.end_col_r) : __m256{};
        [[maybe_unused]] const __m256 end_g = ColorLerp ? _mm256_set1_ps(ctx.end_col_g) : __m256{};
        [[maybe_unused]] const __m256 end_b = ColorLerp ? _mm256_set1_ps(ctx.end_col_b) : __m256{};
        [[maybe_unused]] const __m256 end_a = ColorLerp ? _mm256_set1_ps(ctx.end_col_a) : __m256{};
        [[maybe_unused]] const __m256 end_sx = ScaleLerp ? _mm256_set1_ps(ctx.end_scale_x) : __m256{};
        [[maybe_unused]] const __m256 end_sy = ScaleLerp ? _mm256_set1_ps(ctx.end_scale_y) : __m256{};

        int32_t i = start;
        for (; i <= end - 8; i += 8)
        {
            // -------- Lifetime --------
            __m256 life = _mm256_load_ps(&pd.normalized_lifetimes[i]);
            __m256 inv_life = _mm256_load_ps(&pd.inv_lifetimes[i]);
            life = _mm256_add_ps(life, _mm256_mul_ps(dt_ps, inv_life));
            _mm256_store_ps(&pd.normalized_lifetimes[i], life);

            // -------- Velocity --------
            __m256 vx = _mm256_load_ps(&pd.vel_x[i]);
            __m256 vy = _mm256_load_ps(&pd.vel_y[i]);
            __m256 vz = _mm256_load_ps(&pd.vel_z[i]);
            vx = _mm256_mul_ps(_mm256_add_ps(vx, gx), damp_ps);
            vy = _mm256_mul_ps(_mm256_add_ps(vy, gy), damp_ps);
            vz = _mm256_mul_ps(_mm256_add_ps(vz, gz), damp_ps);
            _mm256_store_ps(&pd.vel_x[i], vx);
            _mm256_store_ps(&pd.vel_y[i], vy);
            _mm256_store_ps(&pd.vel_z[i], vz);

            // -------- Position --------
            __m256 px = _mm256_load_ps(&pd.pos_x[i]);
            __m256 py = _mm256_load_ps(&pd.pos_y[i]);
            __m256 pz = _mm256_load_ps(&pd.pos_z[i]);
            px = _mm256_add_ps(px, _mm256_mul_ps(vx, dt_ps));
            py = _mm256_add_ps(py, _mm256_mul_ps(vy, dt_ps));
            pz = _mm256_add_ps(pz, _mm256_mul_ps(vz, dt_ps));
            _mm256_store_ps(&pd.pos_x[i], px);
            _mm256_store_ps(&pd.pos_y[i], py);
            _mm256_store_ps(&pd.pos_z[i], pz);

            // -------- Color lerp: result = spawn*(1-t) + end*t --------
            // Entire block compiled away when ColorLerp=false
            if constexpr (ColorLerp)
            {
                const __m256 t = life;
                const __m256 it = _mm256_sub_ps(one_ps, t);
                const __m256 sr = _mm256_load_ps(&pd.spawn_col_r[i]);
                const __m256 sg = _mm256_load_ps(&pd.spawn_col_g[i]);
                const __m256 sb = _mm256_load_ps(&pd.spawn_col_b[i]);
                const __m256 sa = _mm256_load_ps(&pd.spawn_col_a[i]);
                _mm256_store_ps(&pd.col_r[i], _mm256_add_ps(_mm256_mul_ps(sr, it), _mm256_mul_ps(end_r, t)));
                _mm256_store_ps(&pd.col_g[i], _mm256_add_ps(_mm256_mul_ps(sg, it), _mm256_mul_ps(end_g, t)));
                _mm256_store_ps(&pd.col_b[i], _mm256_add_ps(_mm256_mul_ps(sb, it), _mm256_mul_ps(end_b, t)));
                _mm256_store_ps(&pd.col_a[i], _mm256_add_ps(_mm256_mul_ps(sa, it), _mm256_mul_ps(end_a, t)));
            }

            // -------- Scale lerp: result = spawn*(1-t) + end*t --------
            // Entire block compiled away when ScaleLerp=false
            if constexpr (ScaleLerp)
            {
                const __m256 t = life;
                const __m256 it = _mm256_sub_ps(one_ps, t);
                const __m256 ssx = _mm256_load_ps(&pd.spawn_scale_x[i]);
                const __m256 ssy = _mm256_load_ps(&pd.spawn_scale_y[i]);
                _mm256_store_ps(&pd.scale_x[i], _mm256_add_ps(_mm256_mul_ps(ssx, it), _mm256_mul_ps(end_sx, t)));
                _mm256_store_ps(&pd.scale_y[i], _mm256_add_ps(_mm256_mul_ps(ssy, it), _mm256_mul_ps(end_sy, t)));
            }

            // -------- GPU write --------
            // Extract from registers rather than re-reading from SoA
            alignas(32) float tmp_px[8], tmp_py[8], tmp_pz[8];
            alignas(32) float tmp_sx[8], tmp_sy[8];
            alignas(32) float tmp_cr[8], tmp_cg[8], tmp_cb[8], tmp_ca[8];

            _mm256_store_ps(tmp_px, px);
            _mm256_store_ps(tmp_py, py);
            _mm256_store_ps(tmp_pz, pz);
            _mm256_store_ps(tmp_sx, _mm256_load_ps(&pd.scale_x[i]));
            _mm256_store_ps(tmp_sy, _mm256_load_ps(&pd.scale_y[i]));
            _mm256_store_ps(tmp_cr, _mm256_load_ps(&pd.col_r[i]));
            _mm256_store_ps(tmp_cg, _mm256_load_ps(&pd.col_g[i]));
            _mm256_store_ps(tmp_cb, _mm256_load_ps(&pd.col_b[i]));
            _mm256_store_ps(tmp_ca, _mm256_load_ps(&pd.col_a[i]));

            for (int j = 0; j < 8; j++)
            {
                auto& b = ctx.buffer_data[i + j];
                b.position.x = tmp_px[j];
                b.position.y = tmp_py[j];
                b.position.z = tmp_pz[j];
                b.scale.x = tmp_sx[j];
                b.scale.y = tmp_sy[j];
                b.color.r = tmp_cr[j];
                b.color.g = tmp_cg[j];
                b.color.b = tmp_cb[j];
                b.color.a = tmp_ca[j];
            }
        }

        // -------- Scalar tail --------
        for (; i < end; ++i)
        {
            pd.normalized_lifetimes[i] += ctx.dt * pd.inv_lifetimes[i];

            pd.vel_x[i] = (pd.vel_x[i] + ctx.grav_x) * ctx.damping_dt;
            pd.vel_y[i] = (pd.vel_y[i] + ctx.grav_y) * ctx.damping_dt;
            pd.vel_z[i] = (pd.vel_z[i] + ctx.grav_z) * ctx.damping_dt;

            pd.pos_x[i] += pd.vel_x[i] * ctx.dt;
            pd.pos_y[i] += pd.vel_y[i] * ctx.dt;
            pd.pos_z[i] += pd.vel_z[i] * ctx.dt;

            const float t = pd.normalized_lifetimes[i];
            const float it = 1.0f - t;

            if constexpr (ColorLerp)
            {
                pd.col_r[i] = pd.spawn_col_r[i] * it + ctx.end_col_r * t;
                pd.col_g[i] = pd.spawn_col_g[i] * it + ctx.end_col_g * t;
                pd.col_b[i] = pd.spawn_col_b[i] * it + ctx.end_col_b * t;
                pd.col_a[i] = pd.spawn_col_a[i] * it + ctx.end_col_a * t;
            }

            if constexpr (ScaleLerp)
            {
                pd.scale_x[i] = pd.spawn_scale_x[i] * it + ctx.end_scale_x * t;
                pd.scale_y[i] = pd.spawn_scale_y[i] * it + ctx.end_scale_y * t;
            }

            auto& b = ctx.buffer_data[i];
            b.position.x = pd.pos_x[i];
            b.position.y = pd.pos_y[i];
            b.position.z = pd.pos_z[i];
            b.scale.x = pd.scale_x[i];
            b.scale.y = pd.scale_y[i];
            b.color.r = pd.col_r[i];
            b.color.g = pd.col_g[i];
            b.color.b = pd.col_b[i];
            b.color.a = pd.col_a[i];
        }

#else
        // -------- Scalar fallback (non-AVX2 builds) --------
        for (int32_t i = start; i < end; ++i)
        {
            pd.normalized_lifetimes[i] += ctx.dt * pd.inv_lifetimes[i];

            pd.vel_x[i] = (pd.vel_x[i] + ctx.grav_x) * ctx.damping_dt;
            pd.vel_y[i] = (pd.vel_y[i] + ctx.grav_y) * ctx.damping_dt;
            pd.vel_z[i] = (pd.vel_z[i] + ctx.grav_z) * ctx.damping_dt;

            pd.pos_x[i] += pd.vel_x[i] * ctx.dt;
            pd.pos_y[i] += pd.vel_y[i] * ctx.dt;
            pd.pos_z[i] += pd.vel_z[i] * ctx.dt;

            const float t = pd.normalized_lifetimes[i];
            const float it = 1.0f - t;

            if constexpr (ColorLerp)
            {
                pd.col_r[i] = pd.spawn_col_r[i] * it + ctx.end_col_r * t;
                pd.col_g[i] = pd.spawn_col_g[i] * it + ctx.end_col_g * t;
                pd.col_b[i] = pd.spawn_col_b[i] * it + ctx.end_col_b * t;
                pd.col_a[i] = pd.spawn_col_a[i] * it + ctx.end_col_a * t;
            }

            if constexpr (ScaleLerp)
            {
                pd.scale_x[i] = pd.spawn_scale_x[i] * it + ctx.end_scale_x * t;
                pd.scale_y[i] = pd.spawn_scale_y[i] * it + ctx.end_scale_y * t;
            }

            auto& b = ctx.buffer_data[i];
            b.position.x = pd.pos_x[i];
            b.position.y = pd.pos_y[i];
            b.position.z = pd.pos_z[i];
            b.scale.x = pd.scale_x[i];
            b.scale.y = pd.scale_y[i];
            b.color.r = pd.col_r[i];
            b.color.g = pd.col_g[i];
            b.color.b = pd.col_b[i];
            b.color.a = pd.col_a[i];
        }
#endif
    }

    //==========================================================================
    // Position + optional outward velocity in one pass
    // OutwardVelocity : if true, sets vel from the normalized spawn direction
    // RandomSpeed     : if true, randomises the outward speed in a range
    //                   (only meaningful when OutwardVelocity=true)
    //==========================================================================
    template<bool OutwardVelocity, bool RandomSpeed>
    static void SpawnPositionAndVelocity(
        ParticleData& pd,
        const Emitter::EmitterSpawnContext& ctx,
        uint32_t                            i,
        float& out_vx,  // written when OutwardVelocity=true
        float& out_vy,
        float& out_vz)
    {
        const uint32_t s0 = i * 73856093u ^ (uint32_t)ctx.frame_id;
        const uint32_t s1 = i * 19349663u ^ (uint32_t)(ctx.frame_id >> 32);
        const uint32_t s2 = i * 83492791u ^ (uint32_t)(ctx.frame_id + 17);
        const uint32_t s3 = i * 2654435761u ^ (uint32_t)(ctx.frame_id + 101);

        float x = random_float(s0) * 2.0f - 1.0f;
        float y = random_float(s1) * 2.0f - 1.0f;
        float z = random_float(s2) * 2.0f - 1.0f;

        const float inv_mag = 1.0f / std::sqrt(x * x + y * y + z * z + 1e-8f);

        // Normalised outward direction — computed regardless, used for both
        // position placement and (when OutwardVelocity=true) velocity direction
        const float nx = x * inv_mag;
        const float ny = y * inv_mag;
        const float nz = z * inv_mag;

        // Radial placement: random depth inside sphere via u^2 distribution
        const float u = random_float(s3);
        const float pscale = ctx.sphere_radius * u * u;
        pd.pos_x[i] = nx * pscale;
        pd.pos_y[i] = ny * pscale;
        pd.pos_z[i] = nz * pscale;

        if constexpr (OutwardVelocity)
        {
            float speed;
            if constexpr (RandomSpeed)
            {
                const uint32_t sv = i * 362436069u ^ (uint32_t)(ctx.frame_id + 31);
                const float    t = random_float(sv);
                speed = ctx.velocity_scale_min + t * (ctx.velocity_scale_max - ctx.velocity_scale_min);
            }
            else
            {
                speed = ctx.velocity_scale;
            }

            out_vx = nx * speed;
            out_vy = ny * speed;
            out_vz = nz * speed;
        }
    }

    //==========================================================================
    // Hot spawn path — 4 compiled variants, zero branches inside
    // RandomLifetime : sample lifetime from [lifetime_min, lifetime_max]
    // RandomScale    : sample scale    from [scale_min,    scale_max]
    // Color is always sampled from [color_min, color_max]; when color_min ==
    // color_max (i.e. color_is_constant) the subtraction is zero-cost.
    //==========================================================================

    template<bool RandomLifetime, bool RandomScale, bool RandomColor, Emitter::VelocityMode VMode>
    static void SpawnParticlesImpl(Emitter::EmitterSpawnContext& ctx, uint32_t start, uint32_t end)
    {
        ParticleData& pd = *ctx.data;
        const float   inv_life_c = 1.0f / ctx.lifetime_constant;

        for (uint32_t i = start; i < end; ++i)
        {
            // ---- Lifetime ----
            if constexpr (RandomLifetime)
            {
                const uint32_t seed = i * 7919u ^ (uint32_t)ctx.frame_id;
                const float    t = random_float(seed);
                pd.inv_lifetimes[i] = 1.0f / (ctx.lifetime_min + t * (ctx.lifetime_max - ctx.lifetime_min));
            }
            else
            {
                pd.inv_lifetimes[i] = inv_life_c;
            }
            pd.normalized_lifetimes[i] = 0.0f;

            // ---- Position + optional outward velocity ----
            float vx = 0.0f, vy = 0.0f, vz = 0.0f;
            if constexpr (VMode == Emitter::VelocityMode::OutwardConstant)
                SpawnPositionAndVelocity<true, false>(pd, ctx, i, vx, vy, vz);
            else if constexpr (VMode == Emitter::VelocityMode::OutwardRandomRange)
                SpawnPositionAndVelocity<true, true>(pd, ctx, i, vx, vy, vz);
            else
                SpawnPositionAndVelocity<false, false>(pd, ctx, i, vx, vy, vz);

            // ---- Velocity ----
            if constexpr (VMode == Emitter::VelocityMode::None)
            {
                pd.vel_x[i] = 0.0f; pd.vel_y[i] = 0.0f; pd.vel_z[i] = 0.0f;
            }
            else if constexpr (VMode == Emitter::VelocityMode::Constant)
            {
                pd.vel_x[i] = ctx.initial_velocity.x;
                pd.vel_y[i] = ctx.initial_velocity.y;
                pd.vel_z[i] = ctx.initial_velocity.z;
            }
            else if constexpr (VMode == Emitter::VelocityMode::RandomRange)
            {
                const float tx = random_float(i * 217645177u ^ (uint32_t)(ctx.frame_id + 13));
                const float ty = random_float(i * 362436069u ^ (uint32_t)(ctx.frame_id + 17));
                const float tz = random_float(i * 521288629u ^ (uint32_t)(ctx.frame_id + 23));
                pd.vel_x[i] = ctx.velocity_min.x + tx * (ctx.velocity_max.x - ctx.velocity_min.x);
                pd.vel_y[i] = ctx.velocity_min.y + ty * (ctx.velocity_max.y - ctx.velocity_min.y);
                pd.vel_z[i] = ctx.velocity_min.z + tz * (ctx.velocity_max.z - ctx.velocity_min.z);
            }
            else // Outward* — already in vx/vy/vz
            {
                pd.vel_x[i] = vx; pd.vel_y[i] = vy; pd.vel_z[i] = vz;
            }

            // ---- Scale ----
            if constexpr (RandomScale)
            {
                const float t = random_float(i * 104729u ^ (uint32_t)(ctx.frame_id + 3));
                pd.scale_x[i] = ctx.scale_min.x + t * (ctx.scale_max.x - ctx.scale_min.x);
                pd.scale_y[i] = ctx.scale_min.y + t * (ctx.scale_max.y - ctx.scale_min.y);
            }
            else
            {
                // Constant and LerpConstant both use scale_start
                pd.scale_x[i] = ctx.scale_start.x;
                pd.scale_y[i] = ctx.scale_start.y;
            }
            // Write spawn snapshot only when streams are allocated (lerp modes)
            if (pd.spawn_scale_x)
            {
                pd.spawn_scale_x[i] = pd.scale_x[i];
                pd.spawn_scale_y[i] = pd.scale_y[i];
            }

            // ---- Color ----
            if constexpr (RandomColor)
            {
                const float t = random_float(i * 48271u ^ (uint32_t)(ctx.frame_id + 7));
                pd.col_r[i] = ctx.color_min.r + t * (ctx.color_max.r - ctx.color_min.r);
                pd.col_g[i] = ctx.color_min.g + t * (ctx.color_max.g - ctx.color_min.g);
                pd.col_b[i] = ctx.color_min.b + t * (ctx.color_max.b - ctx.color_min.b);
                pd.col_a[i] = ctx.color_min.a + t * (ctx.color_max.a - ctx.color_min.a);
            }
            else
            {
                // Constant and LerpConstant — write color_start directly, no random, no lerp math
                pd.col_r[i] = ctx.color_start.r;
                pd.col_g[i] = ctx.color_start.g;
                pd.col_b[i] = ctx.color_start.b;
                pd.col_a[i] = ctx.color_start.a;
            }
            if (pd.spawn_col_r)
            {
                pd.spawn_col_r[i] = pd.col_r[i];
                pd.spawn_col_g[i] = pd.col_g[i];
                pd.spawn_col_b[i] = pd.col_b[i];
                pd.spawn_col_a[i] = pd.col_a[i];
            }
        }

        const uint32_t padded = (end + 7) & ~7u;
        for (uint32_t i = end; i < padded; ++i)
            pd.normalized_lifetimes[i] = 0.0f;

        pd.alive_particle_count = end;
    }

    //==========================================================================
    // Emitter
    //==========================================================================

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
        spawn_accumulator = 0.0f;

        const float max_lifetime = (lifetime_mode == LifetimeMode::RandomRange)
            ? lifetime_max : lifetime_constant;
        const uint32_t capacity = static_cast<uint32_t>(std::ceil(spawn_rate * max_lifetime));

        const bool color_lerp = (color_mode == ColorMode::LerpConstant || color_mode == ColorMode::LerpRandomRange);
        const bool scale_lerp = (scale_mode == ScaleMode::LerpConstant || scale_mode == ScaleMode::LerpRandomRange);

        particle_data.Resize(capacity, color_lerp, scale_lerp);

        renderable->SetParticleInstances(particle_data.max_particle_count);

        RHI_Buffer* instances_buffer = renderable->GetInstanceBuffer();
        if (!instances_buffer) return;
        buffer_data = static_cast<ParticleInstance*>(instances_buffer->GetMappedData());

        SelectHotPaths();
    }

    void Emitter::SelectHotPaths()
    {
        const bool color_lerp = (color_mode == ColorMode::LerpConstant || color_mode == ColorMode::LerpRandomRange);
        const bool scale_lerp = (scale_mode == ScaleMode::LerpConstant || scale_mode == ScaleMode::LerpRandomRange);
        const bool random_color = (color_mode == ColorMode::RandomRange || color_mode == ColorMode::LerpRandomRange);
        const bool random_scale = (scale_mode == ScaleMode::RandomRange || scale_mode == ScaleMode::LerpRandomRange);
        const bool rand_life = (lifetime_mode == LifetimeMode::RandomRange);
        const VelocityMode vm = velocity_mode;

        // Update: 2x2 = 4 variants
        if (color_lerp && scale_lerp) fn_update = UpdateParticles<true, true>;
        else if (color_lerp && !scale_lerp) fn_update = UpdateParticles<true, false>;
        else if (!color_lerp && scale_lerp) fn_update = UpdateParticles<false, true>;
        else                                 fn_update = UpdateParticles<false, false>;

        // Spawn: 2 (lifetime) x 2 (scale) x 2 (color) x 5 (velocity) = 40 variants
        // Macro expands the 5 velocity cases for a given RL/RS/RC combination
#define SPAWN_VELOCITY(RL, RS, RC)                                                                                          \
        if      (vm == VelocityMode::None)             fn_spawn = SpawnParticlesImpl<RL, RS, RC, VelocityMode::None>;           \
        else if (vm == VelocityMode::Constant)         fn_spawn = SpawnParticlesImpl<RL, RS, RC, VelocityMode::Constant>;       \
        else if (vm == VelocityMode::RandomRange)      fn_spawn = SpawnParticlesImpl<RL, RS, RC, VelocityMode::RandomRange>;    \
        else if (vm == VelocityMode::OutwardConstant)  fn_spawn = SpawnParticlesImpl<RL, RS, RC, VelocityMode::OutwardConstant>;\
        else                                           fn_spawn = SpawnParticlesImpl<RL, RS, RC, VelocityMode::OutwardRandomRange>;

        if (rand_life && random_scale && random_color) { SPAWN_VELOCITY(true, true, true) }
        else if (rand_life && random_scale && !random_color) { SPAWN_VELOCITY(true, true, false) }
        else if (rand_life && !random_scale && random_color) { SPAWN_VELOCITY(true, false, true) }
        else if (rand_life && !random_scale && !random_color) { SPAWN_VELOCITY(true, false, false) }
        else if (!rand_life && random_scale && random_color) { SPAWN_VELOCITY(false, true, true) }
        else if (!rand_life && random_scale && !random_color) { SPAWN_VELOCITY(false, true, false) }
        else if (!rand_life && !random_scale && random_color) { SPAWN_VELOCITY(false, false, true) }
        else { SPAWN_VELOCITY(false, false, false) }

#undef SPAWN_VELOCITY
    }

    void Emitter::Update(const double& delta_time)
    {
        const float    dt = static_cast<float>(delta_time);
        const uint64_t frame_id = 0;
        //const uint64_t frame_id = Renderer::GetFrameNumber();

        // ---- Update ----
        //SP_PROFILE_CPU_START("CPU Particles Update")

        auto t_update_begin = std::chrono::high_resolution_clock::now();

        EmitterUpdateContext uctx;
        uctx.data = &particle_data;
        uctx.buffer_data = buffer_data;
        uctx.dt = dt;
        uctx.grav_x = gravity.x * dt;
        uctx.grav_y = gravity.y * dt;
        uctx.grav_z = gravity.z * dt;
        uctx.damping_dt = 1.0f - (1.0f - damping) * dt;
        uctx.end_col_r = color_end.r;
        uctx.end_col_g = color_end.g;
        uctx.end_col_b = color_end.b;
        uctx.end_col_a = color_end.a;
        uctx.end_scale_x = scale_end.x;
        uctx.end_scale_y = scale_end.y;

        if (particle_data.alive_particle_count > 0)
        {
            auto update_func = [this, &uctx](int32_t start, int32_t end)
                {
                    fn_update(uctx, start, end);
                };

            if (particle_data.alive_particle_count >= k_thread_threshold)
                ThreadPool::ParallelLoop(update_func, particle_data.alive_particle_count);
            else
                update_func(0, static_cast<int32_t>(particle_data.alive_particle_count));
        }

        //SP_PROFILE_CPU_END()

        auto t_update_end = std::chrono::high_resolution_clock::now();

        // ---- Kill ----
        //SP_PROFILE_CPU_START("CPU Particles Kill")

        auto t_kill_begin = std::chrono::high_resolution_clock::now();

        uint32_t i = 0;
        while (i < particle_data.alive_particle_count)
        {
            if (particle_data.normalized_lifetimes[i] >= 1.0f)
                Kill(i);
            else
                ++i;
        }

        //SP_PROFILE_CPU_END()
        auto t_kill_end = std::chrono::high_resolution_clock::now();

        // ---- Spawn ----
        //SP_PROFILE_CPU_START("CPU Particles Spawn")

        auto t_spawn_begin = std::chrono::high_resolution_clock::now();

        spawn_accumulator += dt * spawn_rate;
        uint32_t spawn_count = static_cast<uint32_t>(spawn_accumulator);
        const uint32_t available = particle_data.max_particle_count - particle_data.alive_particle_count;
        spawn_count = std::min(spawn_count, available);
        spawn_accumulator -= static_cast<float>(spawn_count);

        if (spawn_count > 0)
        {
            SpawnParticles(spawn_count, frame_id);
        }

        //SP_PROFILE_CPU_END()

        auto t_spawn_end = std::chrono::high_resolution_clock::now();

        // ------------------------------------------------------------
        // Benchmark metrics
        // ------------------------------------------------------------

        if (benchmark_mode)
        {
            benchmark_metrics.update_ms =
                std::chrono::duration<double, std::milli>(t_update_end - t_update_begin).count();

            benchmark_metrics.kill_ms =
                std::chrono::duration<double, std::milli>(t_kill_end - t_kill_begin).count();

            benchmark_metrics.spawn_ms =
                std::chrono::duration<double, std::milli>(t_spawn_end - t_spawn_begin).count();
        }

    }

    void Emitter::SpawnParticles(uint32_t count, uint64_t frame_id)
    {
        EmitterSpawnContext sctx;
        sctx.data = &particle_data;
        sctx.frame_id = frame_id;
        sctx.sphere_radius = sphere_radius;
        sctx.lifetime_constant = lifetime_constant;
        sctx.lifetime_min = lifetime_min;
        sctx.lifetime_max = lifetime_max;
        sctx.initial_velocity = initial_velocity;
        sctx.velocity_min = velocity_min;
        sctx.velocity_max = velocity_max;
        sctx.velocity_scale = velocity_scale;
        sctx.velocity_scale_min = velocity_scale_min;
        sctx.velocity_scale_max = velocity_scale_max;

        // Scale: start value is scale_constant for non-lerp modes, scale_start for lerp modes
        sctx.scale_start = (scale_mode == ScaleMode::LerpConstant || scale_mode == ScaleMode::LerpRandomRange)
            ? scale_start : scale_constant;
        sctx.scale_min = scale_min;
        sctx.scale_max = scale_max;

        sctx.color_start = (color_mode == ColorMode::LerpConstant || color_mode == ColorMode::LerpRandomRange)
            ? color_start : color_constant;
        sctx.color_min = color_min;
        sctx.color_max = color_max;

        const uint32_t start = particle_data.alive_particle_count;
        const uint32_t end = start + count;
        fn_spawn(sctx, start, end);
    }

    void Emitter::Kill(uint32_t index)
    {
        const uint32_t last = particle_data.alive_particle_count - 1;

        if (index != last)
        {
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

            // Copy spawn streams only if they are allocated
            if (particle_data.spawn_col_r)
            {
                particle_data.spawn_col_r[index] = particle_data.spawn_col_r[last];
                particle_data.spawn_col_g[index] = particle_data.spawn_col_g[last];
                particle_data.spawn_col_b[index] = particle_data.spawn_col_b[last];
                particle_data.spawn_col_a[index] = particle_data.spawn_col_a[last];
            }

            if (particle_data.spawn_scale_x)
            {
                particle_data.spawn_scale_x[index] = particle_data.spawn_scale_x[last];
                particle_data.spawn_scale_y[index] = particle_data.spawn_scale_y[last];
            }
        }

        --particle_data.alive_particle_count;
    }

    void Emitter::ApplyBenchmarkVariant(
        LifetimeMode lifetime,
        ScaleMode scale,
        ColorMode color,
        VelocityMode velocity)
    {
        lifetime_mode = lifetime;
        scale_mode = scale;
        color_mode = color;
        velocity_mode = velocity;

        const bool color_lerp =
            (
                color_mode == ColorMode::LerpConstant ||
                color_mode == ColorMode::LerpRandomRange
                );

        const bool scale_lerp =
            (
                scale_mode == ScaleMode::LerpConstant ||
                scale_mode == ScaleMode::LerpRandomRange
                );

        // preserve capacity
        const uint32_t capacity = particle_data.max_particle_count;

        particle_data.Resize(capacity, color_lerp, scale_lerp);

        renderable->SetParticleInstances(capacity);

        RHI_Buffer* instances_buffer = renderable->GetInstanceBuffer();

        if (instances_buffer)
        {
            buffer_data =
                static_cast<ParticleInstance*>(instances_buffer->GetMappedData());
        }

        SelectHotPaths();
        ResetParticles();

    }

    void Emitter::ResetParticles()
    {
        particle_data.alive_particle_count = 0;
        spawn_accumulator = 0.0f;

        benchmark_metrics.Reset();
    }

}
