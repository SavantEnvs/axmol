// Known-answer functional test for axmol's vendored OpenAL-soft (3rdparty/openal), built with normal flags.
// Plays stereo buffers on a loopback device with direct channels (no panning/resampling: buffer rate ==
// device rate) and checks the rendered float samples equal the decoded input values.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#define AL_ALEXT_PROTOTYPES
#include "AL/al.h"
#include "AL/alc.h"
#include "AL/alext.h"

static int g_pass, g_fail;

static void check(const char* name, bool ok)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    (ok ? g_pass : g_fail)++;
}

// Render a buffer and return the output frame at index `frame` (L,R).
static bool render(ALCdevice* dev, ALenum fmt, const void* data, ALsizei len, ALint align, float* l, float* r)
{
    ALuint buf = 0, src = 0;
    alGetError();
    alGenBuffers(1, &buf);
    alGenSources(1, &src);
    if (align)
        alBufferi(buf, AL_UNPACK_BLOCK_ALIGNMENT_SOFT, align);
    alBufferData(buf, fmt, data, len, 44100);
    if (alGetError() != AL_NO_ERROR)
        return false;
    alSourcei(src, AL_BUFFER, (ALint)buf);
    alSourcei(src, AL_DIRECT_CHANNELS_SOFT, AL_TRUE);
    alSourcei(src, AL_LOOPING, AL_TRUE);
    alSourcePlay(src);
    std::vector<float> out(512 * 2);
    alcRenderSamplesSOFT(dev, out.data(), 512);
    alcRenderSamplesSOFT(dev, out.data(), 512);
    *l = out[2 * 300];
    *r = out[2 * 300 + 1];
    alSourceStop(src);
    alDeleteSources(1, &src);
    alDeleteBuffers(1, &buf);
    return alGetError() == AL_NO_ERROR;
}

static bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

int main()
{
    ALCdevice* dev = alcLoopbackOpenDeviceSOFT(nullptr);
    const ALCint attrs[] = {ALC_FORMAT_CHANNELS_SOFT, ALC_STEREO_SOFT, ALC_FORMAT_TYPE_SOFT, ALC_FLOAT_SOFT,
                            ALC_FREQUENCY, 44100, 0};
    ALCcontext* ctx = dev ? alcCreateContext(dev, attrs) : nullptr;
    check("loopback_device", dev && ctx && alcMakeContextCurrent(ctx));
    if (!ctx)
        return 1;
    check("ext_direct_channels", alIsExtensionPresent("AL_SOFT_direct_channels"));
    check("ext_msadpcm", alIsExtensionPresent("AL_SOFT_MSADPCM"));

    float l = 0, r = 0;
    // PCM16 stereo: L=16384 (0.5), R=-8192 (-0.25)
    std::vector<int16_t> pcm(2048);
    for (size_t i = 0; i < pcm.size(); i += 2) { pcm[i] = 16384; pcm[i + 1] = -8192; }
    bool ok = render(dev, AL_FORMAT_STEREO16, pcm.data(), (ALsizei)(pcm.size() * 2), 0, &l, &r);
    check("pcm16_stereo_values", ok && near(l, 0.5f) && near(r, -0.25f));

    // 8-bit unsigned stereo: 192 -> 0.5, 64 -> -0.5
    std::vector<uint8_t> u8(2048);
    for (size_t i = 0; i < u8.size(); i += 2) { u8[i] = 192; u8[i + 1] = 64; }
    ok = render(dev, AL_FORMAT_STEREO8, u8.data(), (ALsizei)u8.size(), 0, &l, &r);
    check("u8_stereo_values", ok && near(l, 0.5f) && near(r, -0.5f));

    // IMA4 stereo, 65 samples per block: per-channel header (predictor LE16, step index, pad), then
    // 4-byte nibble chunks per channel. All-zero nibbles keep each channel at its predictor.
    const int spb = 65, blockBytes = 2 * (4 + (spb - 1) / 2);
    std::vector<uint8_t> ima(blockBytes * 8, 0);
    for (int b = 0; b < 8; ++b)
    {
        uint8_t* p = &ima[b * blockBytes];
        p[0] = 0x00; p[1] = 0x40; // L predictor 16384
        p[4] = 0x00; p[5] = 0xE0; // R predictor -8192
    }
    ok = render(dev, AL_FORMAT_STEREO_IMA4, ima.data(), (ALsizei)ima.size(), spb, &l, &r);
    check("ima4_stereo_values", ok && near(l, 0.5f) && near(r, -0.25f));

    // MS ADPCM stereo, 64 samples/block: header = predictor idx[2], delta LE16[2], sample1 LE16[2],
    // sample2 LE16[2]; then nibbles. Coef set 0 (256,0): zero nibbles -> each sample == previous sample.
    const int mspb = 64, msBlock = 2 * 7 + (mspb - 2);
    std::vector<uint8_t> ms(msBlock * 8, 0);
    for (int b = 0; b < 8; ++b)
    {
        uint8_t* p = &ms[b * msBlock];
        p[2] = 16; p[4] = 16;                 // deltas
        p[6] = 0x00; p[7] = 0x40;             // L sample1 16384
        p[8] = 0x00; p[9] = 0xE0;             // R sample1 -8192
        p[10] = 0x00; p[11] = 0x40;           // L sample2
        p[12] = 0x00; p[13] = 0xE0;           // R sample2
    }
    ok = render(dev, AL_FORMAT_STEREO_MSADPCM_SOFT, ms.data(), (ALsizei)ms.size(), mspb, &l, &r);
    check("msadpcm_stereo_values", ok && near(l, 0.5f) && near(r, -0.25f));

    // Invalid size for IMA4 block alignment must be rejected.
    ALuint buf = 0;
    alGetError();
    alGenBuffers(1, &buf);
    alBufferi(buf, AL_UNPACK_BLOCK_ALIGNMENT_SOFT, spb);
    alBufferData(buf, AL_FORMAT_STEREO_IMA4, ima.data(), blockBytes - 1, 44100);
    check("ima4_bad_size_rejected", alGetError() == AL_INVALID_VALUE);
    alDeleteBuffers(1, &buf);

    alcMakeContextCurrent(nullptr);
    alcDestroyContext(ctx);
    alcCloseDevice(dev);
    printf("RESULT passed=%d failed=%d\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
