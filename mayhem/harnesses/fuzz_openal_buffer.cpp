// Fuzz axmol's vendored OpenAL-soft buffer loading + mixing (3rdparty/openal), the path AudioCache
// (axmol/audio/AudioCache.cpp) drives when a game plays a sound: alBufferi(AL_UNPACK_BLOCK_ALIGNMENT_SOFT)
// + alBufferData(<decoded format>) + alSourcePlay, then the mixer decodes the samples
// (core/voice.cpp LoadSamples<IMA4Data>/<MSADPCMData>/...). A loopback device renders the mix in-process,
// so no sound hardware is needed.
//
// Input layout: [0] format selector, [1..2] block alignment (samples per block, LE; 0 = default),
// [3..4] sample rate (LE), [5] pitch, [6..7] sample offset (LE), [8] flags (bit0 looping), [9..] sample data.
// State kept across inputs: the loopback device + context (created once). Each input gets a fresh buffer
// and source, both deleted before returning, so no input sees another's data.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#define AL_ALEXT_PROTOTYPES
#include "AL/al.h"
#include "AL/alc.h"
#include "AL/alext.h"

static ALCdevice* g_device;
static ALCcontext* g_context;

static const ALenum kFormats[] = {
    AL_FORMAT_MONO_MSADPCM_SOFT, AL_FORMAT_STEREO_MSADPCM_SOFT, AL_FORMAT_MONO_IMA4, AL_FORMAT_STEREO_IMA4,
    AL_FORMAT_MONO16,            AL_FORMAT_STEREO16,            AL_FORMAT_MONO8,     AL_FORMAT_STEREO8,
    AL_FORMAT_MONO_FLOAT32,      AL_FORMAT_STEREO_FLOAT32,      AL_FORMAT_MONO_MULAW, AL_FORMAT_STEREO_MULAW,
    AL_FORMAT_MONO_ALAW_EXT,     AL_FORMAT_STEREO_ALAW_EXT,     AL_FORMAT_MONO_MSADPCM_SOFT, AL_FORMAT_STEREO_IMA4,
};

extern "C" int LLVMFuzzerInitialize(int*, char***)
{
    g_device = alcLoopbackOpenDeviceSOFT(nullptr);
    if (!g_device)
        abort();
    const ALCint attrs[] = {ALC_FORMAT_CHANNELS_SOFT, ALC_STEREO_SOFT, ALC_FORMAT_TYPE_SOFT, ALC_FLOAT_SOFT,
                            ALC_FREQUENCY, 44100, 0};
    g_context = alcCreateContext(g_device, attrs);
    if (!g_context || !alcMakeContextCurrent(g_context))
        abort();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 9)
        return -1;
    const ALenum format = kFormats[data[0] % (sizeof(kFormats) / sizeof(kFormats[0]))];
    const ALint align   = data[1] | (data[2] << 8);
    ALsizei freq        = data[3] | (data[4] << 8);
    if (freq == 0)
        freq = 22050;
    const float pitch    = 0.25f + data[5] / 32.0f;
    const ALint offset   = data[6] | (data[7] << 8);
    const bool looping   = data[8] & 1;
    const uint8_t* pcm   = data + 9;
    const ALsizei pcmLen = static_cast<ALsizei>(size - 9);

    alGetError();
    ALuint buffer = 0, source = 0;
    alGenBuffers(1, &buffer);
    alGenSources(1, &source);
    if (align)
        alBufferi(buffer, AL_UNPACK_BLOCK_ALIGNMENT_SOFT, align);
    // Copy so the buffer data is exactly the input bytes (catches overreads past pcmLen).
    std::vector<uint8_t> copy(pcm, pcm + pcmLen);
    alBufferData(buffer, format, copy.data(), pcmLen, freq);
    if (alGetError() == AL_NO_ERROR)
    {
        alSourcei(source, AL_BUFFER, static_cast<ALint>(buffer));
        alSourcef(source, AL_PITCH, pitch);
        alSourcei(source, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
        alSourcei(source, AL_SAMPLE_OFFSET, offset);
        alSourcePlay(source);
        static float out[1024 * 2];
        for (int i = 0; i < 16; ++i)
            alcRenderSamplesSOFT(g_device, out, 1024);
        alSourceStop(source);
        alSourcei(source, AL_BUFFER, 0);
    }
    alDeleteSources(1, &source);
    alDeleteBuffers(1, &buffer);
    alGetError();
    return 0;
}
