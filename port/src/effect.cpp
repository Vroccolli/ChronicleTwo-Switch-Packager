#include "common.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <limits>
#include <string_view>
#include <utility>

#include "bounded_text.hpp"
#include "command_replay.hpp"
#include "effect.hpp"
#include "scriptinterpreter.hpp"
#include "stubs/effect.hpp"

#ifdef __SWITCH__
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "mglib_port.hpp"

namespace {
struct ParticleSample {
    int slot = 0, age = 0, life = 0;
    float alpha = 0, x = 0, y = 0, z = 0;
};
struct ParticleObservation {
    const CEffectManager *identity = nullptr; // Compared only; never dereferenced later.
    const CEffect *pool = nullptr;
    char name[33]{}, image[33]{};
    unsigned long long steps = 0, draws = 0, step_frame = 0, draw_frame = 0, sample_frame = 0;
    std::chrono::steady_clock::time_point sampled{};
    int run = 0, total = 0, scanned = 0, active = 0, expired = 0, nonfinite = 0;
    int min_age = 0, max_age = 0;
    std::array<ParticleSample, 4> samples{};
    unsigned sample_count = 0;
};
std::array<ParticleObservation, 64> particle_observations;
unsigned particle_evictions = 0;
}

// Observe at the actual update/draw sites. Keep copied values, never live pool pointers.
void ctSwitchObserveParticles(const CEffectManager *manager, bool drawing) {
    ParticleObservation *record = nullptr;
    for (auto &entry : particle_observations) {
        if (entry.identity == manager) { record = &entry; break; }
    }
    if (!record) {
        for (auto &entry : particle_observations) {
            if (!entry.identity) { record = &entry; break; }
        }
    }
    if (!record) {
        record = &*std::min_element(particle_observations.begin(), particle_observations.end(),
            [](const auto &a, const auto &b) { return std::max(a.step_frame, a.draw_frame) < std::max(b.step_frame, b.draw_frame); });
        ++particle_evictions;
        *record = {};
    }
    if (record->identity != manager || record->pool != manager->effects ||
        std::memcmp(record->name, manager->name, 32) != 0) {
        *record = {};
        record->identity = manager;
        record->pool = manager->effects;
        std::memcpy(record->name, manager->name, 32);
        std::memcpy(record->image, manager->img_name, 32);
    }
    const auto frame = static_cast<unsigned long long>(mgPortFrameCount());
    if (drawing) { ++record->draws; record->draw_frame = frame; }
    else { ++record->steps; record->step_frame = frame; }
    const auto now = std::chrono::steady_clock::now();
    if (record->sampled.time_since_epoch().count() && now - record->sampled < std::chrono::milliseconds(500)) return;
    record->sampled = now;
    record->sample_frame = frame;
    record->run = manager->run;
    record->total = manager->effect_num;
    record->scanned = manager->effects ? std::clamp(manager->effect_num, 0, 512) : 0;
    record->active = record->expired = record->nonfinite = record->sample_count = 0;
    record->min_age = std::numeric_limits<int>::max();
    record->max_age = 0;
    for (int i = 0; i < record->scanned; ++i) {
        const auto &particle = manager->effects[i];
        if (!particle.active) continue;
        ++record->active;
        record->expired += particle.frame > particle.param.life;
        record->nonfinite += !std::isfinite(particle.alpha);
        record->min_age = std::min(record->min_age, particle.frame);
        record->max_age = std::max(record->max_age, particle.frame);
        if (record->sample_count < record->samples.size()) {
            record->samples[record->sample_count++] = {i, particle.frame, particle.param.life,
                particle.alpha, particle.pos[0], particle.pos[1], particle.pos[2]};
        }
    }
    if (!record->active) record->min_age = 0;
}

void ctSwitchWriteParticleState(FILE *file) {
    std::fprintf(file, "Ordinary particles: 64 retained managers, 512 slots scanned per sample; evictions %u\n", particle_evictions);
    for (const auto &entry : particle_observations) {
        if (!entry.identity) continue;
        std::fprintf(file, "pool %p name %.32s image %.32s run %d total %d scanned %d active %d expired %d nonfinite %d age %d..%d steps %llu draws %llu step-frame %llu draw-frame %llu sample-frame %llu\n",
            static_cast<const void *>(entry.identity), entry.name, entry.image, entry.run, entry.total, entry.scanned,
            entry.active, entry.expired, entry.nonfinite, entry.min_age, entry.max_age, entry.steps, entry.draws,
            entry.step_frame, entry.draw_frame, entry.sample_frame);
        for (unsigned i = 0; i < entry.sample_count; ++i) {
            const auto &sample = entry.samples[i];
            std::fprintf(file, " particle %d age %d life %d opacity %.4f xyz %.3f %.3f %.3f\n",
                sample.slot, sample.age, sample.life, sample.alpha, sample.x, sample.y, sample.z);
        }
    }
}
#endif

namespace {

/**
 *
 * Emitter between an EFFECT_START tag and its EFFECT_END, owned by the loader.
 *
 */
std::unique_ptr<CEffectCtrl> open_emitter;

/**
 *
 * Takes the settings of tags written outside an EFFECT_START block, which retail wrote through a null emitter.
 *
 */
CEffectCtrl &DiscardEmitter() {
    static CEffectCtrl emitter;
    return emitter;
}

/**
 *
 * Gives a tag argument, inside the interpreter's argument stack.
 *
 */
SPI_STACK *Argument(SPI_STACK *args, int index) {
    return index < SPI_STACK_SIZE ? args + index : nullptr;
}

/**
 *
 * Gives an integer tag argument, or zero outside the argument stack.
 *
 */
int IntArgument(SPI_STACK *args, int index) {
    SPI_STACK *argument = Argument(args, index);
    return argument != nullptr ? spiGetStackInt(argument) : 0;
}

/**
 *
 * Gives a string tag argument as text, empty when the argument is not a string.
 *
 */
std::string_view TextArgument(SPI_STACK *args, int index) {
    SPI_STACK *argument = Argument(args, index);
    return argument != nullptr ? bounded_text::View(spiGetStackString(argument)) : std::string_view();
}

/**
 *
 * Sets the particle and emitter pools, holding the emitters to the manager's per-emitter tables.
 *
 */
int BufferSize(SPI_STACK *args, int /*unused*/) {
    const int particles = std::max(IntArgument(args, 0), 0);
    const int emitters = std::clamp(IntArgument(args, 1), 0, static_cast<int>(std::size(g_tmp_effm->wait_frame)));
    g_tmp_effm->SetEffectNums(particles, emitters);
    return 1;
}

/**
 *
 * Opens a named emitter definition, discarding one left open by a missing EFFECT_END.
 *
 */
int EffectStart(SPI_STACK *args, int /*unused*/) {
    bounded_text::Copy(g_tmp_eff_name, TextArgument(args, 0));
    open_emitter = std::make_unique<CEffectCtrl>();
    g_tmp_effc = open_emitter.get();
    return 1;
}

/**
 *
 * Enters the open emitter into the manager when loading, and closes it.
 *
 */
int EffectEnd(SPI_STACK * /*unused*/, int /*unused*/) {
    if (open_emitter != nullptr && g_eff_entry_flag != 0) {
        g_tmp_effm->EnterEffectCtrl(*open_emitter, g_tmp_eff_name);
    }
    open_emitter.reset();
    g_tmp_effc = &DiscardEmitter();
    return 1;
}

/**
 *
 * Sets the start delay of one of the manager's emitters.
 *
 */
int WaitFrame(SPI_STACK *args, int /*unused*/) {
    const int index = IntArgument(args, 0);
    if (index >= 0 && std::cmp_less(index, std::size(g_tmp_effm->wait_frame))) {
        g_tmp_effm->wait_frame[index] = IntArgument(args, 1);
    }
    return 1;
}

/**
 *
 * Names the texture archive the effect needs.
 *
 */
int ImageName(SPI_STACK *args, int /*unused*/) {
    bounded_text::Copy(g_tmp_effm->img_name, TextArgument(args, 0));
    return 1;
}

/**
 *
 * Reads the open emitter's texture rectangles, no more than it has room for.
 *
 */
int TextureRects(SPI_STACK *args, int /*unused*/) {
    auto     &rects = g_tmp_effc->tex_rect;
    const int count = std::clamp(IntArgument(args, 0), 0, static_cast<int>(std::size(rects)));
    g_tmp_effc->tex_rect_num = count;
    for (int i = 0; i < count; ++i) {
        for (int j = 0; std::cmp_less(j, std::size(rects[i])); ++j) {
            rects[i][j] = IntArgument(args, 1 + i * static_cast<int>(std::size(rects[i])) + j);
        }
    }
    return 1;
}

/**
 *
 * Native handler that replaces a retail effect-script tag.
 *
 */
struct Replacement {
    std::string_view name;     /**< Tag the handler replaces. */
    SPI_TAG_FUNCTION function; /**< Native handler. */
};

/**
 *
 * Gives retail's effect-script tags with the native handlers in place of those that own memory or index tables.
 *
 */
SPI_TAG_PARAM *Tags() {
    static constexpr std::array<Replacement, 6> replacements{
        {
         {.name = "BUFFER_SIZE", .function = BufferSize},
         {.name = "EFFECT_START", .function = EffectStart},
         {.name = "EFFECT_END", .function = EffectEnd},
         {.name = "WAIT_FRAME", .function = WaitFrame},
         {.name = "IMG_NAME", .function = ImageName},
         {.name = "TEX_GET_RECT", .function = TextureRects},
         }
    };
    static auto table = [] {
        std::array<SPI_TAG_PARAM, std::size(effm_tag)> tags{};
        std::ranges::copy(effm_tag, tags.begin());
        for (auto &tag : tags) {
            if (tag.name == nullptr) {
                break;
            }
            const auto *const found = std::ranges::find(replacements, std::string_view(tag.name), &Replacement::name);
            if (found != replacements.end()) {
                tag.function = found->function;
            }
        }
        return tags;
    }();
    return table.data();
}

/**
 *
 * Runs an effect script against a manager, entering its emitters when entry is set.
 *
 */
void RunEffectScript(CEffectManager &manager, char *script, int size, int entry) {
    CScriptInterpreter interpreter;
    g_tmp_effm = &manager;
    g_eff_entry_flag = entry;
    g_tmp_effc = &DiscardEmitter();
    // Character effect scripts embedded in .game.json resources are typed command lists.
    if (command_replay::IsCommandBuffer(script, size)) {
        command_replay::RunBuffer(script, size, Tags());
    } else {
        interpreter.SetTag(Tags());
        interpreter.SetScript(script, size);
        interpreter.Run();
    }
    open_emitter.reset();
    g_tmp_effc = &DiscardEmitter();
}

} // namespace

// Retail allocates each emitter with new and frees it at EFFECT_END, leaking one left open, and its
// handlers index the manager's eight-entry tables and copy names without bounds.
PORT_OVERRIDE void CEffectManager::GetBufferNums(char *script, int size, int *effect_num, int *ctrl_num) {
    RunEffectScript(*this, script, size, 0);
    *effect_num = this->effect_num;
    *ctrl_num = this->ctrl_num;
}

PORT_OVERRIDE void CEffectManager::Load(char *script, int size) {
    RunEffectScript(*this, script, size, 1);
    load = 1;
}
