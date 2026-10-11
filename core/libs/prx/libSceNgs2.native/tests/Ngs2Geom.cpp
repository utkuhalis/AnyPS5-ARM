#include "Ngs2Test.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>

extern "C" {
int APS5_VABI sceNgs2GeomResetListenerParam(Ngs2GeomListenerParam*);
int APS5_VABI sceNgs2GeomResetSourceParam(Ngs2GeomSourceParam*);
int APS5_VABI sceNgs2GeomCalcListener(const Ngs2GeomListenerParam*, Ngs2GeomListenerWork*, uint32_t);
int APS5_VABI sceNgs2GeomApply(const Ngs2GeomListenerWork*, const Ngs2GeomSourceParam*, Ngs2GeomAttribute*, uint32_t);
}

#define Expect(value) Require(value)

static bool Near(float value, float expected) {
    return std::abs(value - expected) < 1e-4f;
}

template<typename TFunction>
static bool ThrowsInvalidArgument(TFunction function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static constexpr std::uint32_t Angle = 1;
static constexpr std::uint32_t Doppler = 2;
static constexpr std::uint32_t Matrix = 4;
static constexpr std::uint32_t A3d = 8;

static Ngs2GeomListenerWork Listener(const Ngs2GeomListenerParam& param, std::uint32_t flags = 0) {
    Ngs2GeomListenerWork work{};
    Expect(sceNgs2GeomCalcListener(&param, &work, flags) == SCE_NGS2_OK);
    return work;
}

int main() {
    constexpr int invalidOutAddress = static_cast<int>(0x804A8010);
    Expect(sceNgs2GeomResetListenerParam(nullptr) == invalidOutAddress);
    Expect(sceNgs2GeomResetSourceParam(nullptr) == invalidOutAddress);
    Ngs2GeomListenerParam listenerParam{};
    listenerParam.reserved[0] = 7;
    Expect(sceNgs2GeomResetListenerParam(&listenerParam) == SCE_NGS2_OK);
    Expect(listenerParam.orient_front.z == 1.0f && listenerParam.orient_up.y == 1.0f && listenerParam.sound_speed == 340.0f && listenerParam.reserved[0] == 0);
    Ngs2GeomSourceParam source{};
    Expect(sceNgs2GeomResetSourceParam(&source) == SCE_NGS2_OK);
    Expect(source.rolloff.reference_distance == 1.0f && source.max_level == 1.0f && source.num_speakers == 2 && source.matrix_format == 2);

    // A listener at (10, 0, 0) looking down -x: a source at the origin is straight ahead, 10 away.
    listenerParam.position = {10.0f, 0.0f, 0.0f};
    listenerParam.orient_front = {-1.0f, 0.0f, 0.0f};
    Ngs2GeomListenerWork listener = Listener(listenerParam);
    Expect(listener.matrix[3][3] == 1.0f && listener.sound_speed == 340.0f && listener.coordinate == 0);
    Ngs2GeomAttribute attribute{};
    attribute.pitch_ratio = -1.0f;
    Expect(sceNgs2GeomApply(&listener, &source, &attribute, A3d) == SCE_NGS2_OK);
    Expect(Near(attribute.a3d_attrib.position.x, 0.0f) && Near(attribute.a3d_attrib.position.z, 10.0f) && Near(attribute.a3d_attrib.volume, 0.1f));
    Expect(attribute.pitch_ratio == -1.0f);

    // Left-handed, the source at the listener's right; right-handed, the same scene puts it at the left.
    source.position = {10.0f, 0.0f, 3.0f};
    Expect(sceNgs2GeomApply(&listener, &source, &attribute, A3d | Angle | Matrix) == SCE_NGS2_OK);
    Expect(Near(attribute.a3d_attrib.position.x, 3.0f) && Near(attribute.a3d_attrib.position.z, 0.0f));
    Expect(Near(attribute.levels[0], 0.0f) && Near(attribute.levels[1], 1.0f / 3.0f) && attribute.levels[2] == 0.0f);
    const Ngs2GeomListenerWork rightHanded = Listener(listenerParam, 1);
    Expect(sceNgs2GeomApply(&rightHanded, &source, &attribute, Angle | Matrix) == SCE_NGS2_OK);
    Expect(Near(attribute.levels[0], 1.0f / 3.0f) && Near(attribute.levels[1], 0.0f));
    Expect(sceNgs2GeomApply(&listener, &source, &attribute, Matrix) == SCE_NGS2_OK);
    Expect(Near(attribute.levels[0], attribute.levels[1]) && Near(attribute.levels[0] * attribute.levels[0] * 2.0f, 1.0f / 9.0f));

    // Linear rolloff, clamped, and the level limits.
    source.position = {0.0f, 0.0f, 0.0f};
    source.rolloff = {1, 20.0f, 1.0f, 2.0f};
    Expect(sceNgs2GeomApply(&listener, &source, &attribute, A3d) == SCE_NGS2_OK && Near(attribute.a3d_attrib.volume, 1.0f - 8.0f / 18.0f));
    source.rolloff = {4, 5.0f, 1.0f, 1.0f};
    Expect(sceNgs2GeomApply(&listener, &source, &attribute, A3d) == SCE_NGS2_OK && Near(attribute.a3d_attrib.volume, 0.0f));
    source.min_level = 0.25f;
    Expect(sceNgs2GeomApply(&listener, &source, &attribute, A3d) == SCE_NGS2_OK && Near(attribute.a3d_attrib.volume, 0.25f));
    source.min_level = 0.0f;
    source.rolloff = {2, 1000.0f, 2.0f, 1.0f};
    Expect(sceNgs2GeomApply(&listener, &source, &attribute, A3d) == SCE_NGS2_OK && Near(attribute.a3d_attrib.volume, 0.01f));

    // A cone pointing away from the listener leaves the outer level.
    source.rolloff = {3, 1000.0f, 0.0f, 1.0f};
    source.direction = {-1.0f, 0.0f, 0.0f};
    source.cone = {1.0f, 60.0f, 0.5f, 120.0f};
    Expect(sceNgs2GeomApply(&listener, &source, &attribute, A3d) == SCE_NGS2_OK && Near(attribute.a3d_attrib.volume, 0.5f));
    source.direction = {1.0f, 0.0f, 0.0f};
    Expect(sceNgs2GeomApply(&listener, &source, &attribute, A3d) == SCE_NGS2_OK && Near(attribute.a3d_attrib.volume, 1.0f));

    // A source moving towards the listener sounds higher, one moving away lower.
    source.velocity = {34.0f, 0.0f, 0.0f};
    Expect(sceNgs2GeomApply(&listener, &source, &attribute, Doppler) == SCE_NGS2_OK && Near(attribute.pitch_ratio, 340.0f / 306.0f));
    source.velocity = {-34.0f, 0.0f, 0.0f};
    Expect(sceNgs2GeomApply(&listener, &source, &attribute, Doppler) == SCE_NGS2_OK && Near(attribute.pitch_ratio, 340.0f / 374.0f));
    source.doppler_factor = 0.0f;
    Expect(sceNgs2GeomApply(&listener, &source, &attribute, Doppler) == SCE_NGS2_OK && attribute.pitch_ratio == 1.0f);

    // Inside its radius a source spreads over every speaker; surround layouts feed the LFE.
    source = {};
    Expect(sceNgs2GeomResetSourceParam(&source) == SCE_NGS2_OK);
    source.position = {10.0f, 0.0f, 0.5f};
    source.radius = 1.0f;
    source.lfe_level = 0.5f;
    source.num_speakers = 5;
    source.matrix_format = 6;
    Expect(sceNgs2GeomApply(&listener, &source, &attribute, Angle | Matrix) == SCE_NGS2_OK);
    Expect(attribute.levels[0] > 0.0f && attribute.levels[1] > attribute.levels[0] && Near(attribute.levels[3], 0.5f) && attribute.levels[6] == 0.0f);

    Expect(sceNgs2GeomApply(&listener, &source, nullptr, Matrix) == invalidOutAddress);
    Expect(ThrowsInvalidArgument([&] { sceNgs2GeomApply(nullptr, &source, &attribute, Matrix); }));
    Expect(ThrowsInvalidArgument([&] { sceNgs2GeomApply(&listener, &source, &attribute, 0x20); }));
    source.rolloff.model = 6;
    Expect(ThrowsInvalidArgument([&] { sceNgs2GeomApply(&listener, &source, &attribute, Matrix); }));
    listenerParam.orient_up = {-1.0f, 0.0f, 0.0f};
    Ngs2GeomListenerWork work{};
    Expect(ThrowsInvalidArgument([&] { sceNgs2GeomCalcListener(&listenerParam, &work, 0); }));
    listenerParam.orient_up = {0.0f, 1.0f, 0.0f};
    Expect(ThrowsInvalidArgument([&] { sceNgs2GeomCalcListener(&listenerParam, &work, 2); }));
    Expect(sceNgs2GeomCalcListener(&listenerParam, nullptr, 0) == invalidOutAddress);
    std::puts("NGS2 geometry tests passed");
    return 0;
}
