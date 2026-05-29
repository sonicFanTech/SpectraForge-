// SpectraForge Console v0.3
// Hybrid Win32 GUI + interactive console mode
//
// Double-click / normal launch:
//   Opens the GUI window.
//
// Console mode:
//   SpectraForgeConsole.exe --console
//
// Resource layout next to EXE:
//   bin\ffmpeg.exe
//   bin\ffprobe.exe
//
// Also accepted:
//   bin\FFmpeg\ffmpeg.exe
//   bin\ffmpeg\ffmpeg.exe
//   bin\FFmpeg\bin\ffmpeg.exe
//   bin\ffmpeg\bin\ffmpeg.exe
//
// Features:
// - Native Win32 GUI, no Qt/CMake needed for this prototype
// - Old interactive console menu still available
// - Image -> WAV sound
// - Audio/video -> waveform BMP
// - Audio/video -> real FFT spectrogram BMP
// - Basic image-to-sound effects/presets

#define NOMINMAX

#include <windows.h>
#include <wincodec.h>
#include <commdlg.h>
#include <conio.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace fs = std::filesystem;

static constexpr double PI = 3.1415926535897932384626433832795;

struct ImageRGBA
{
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> pixels; // RGBA
};

struct WavData16
{
    int sampleRate = 44100;
    int channels = 1;
    std::vector<int16_t> samples; // interleaved
};

enum class SoundEffect
{
    Clean = 0,
    DarkDrone = 1,
    BrightMelody = 2,
    Glitch = 3
};

struct AppSettings
{
    int sampleRate = 44100;
    double durationSeconds = 8.0;
    int voices = 48;
    double volume = 0.35;
    SoundEffect effect = SoundEffect::Clean;
};

struct ToolPaths
{
    fs::path exeDir;
    fs::path binDir;
    fs::path ffmpeg;
    fs::path ffprobe;
    bool ffmpegFound = false;
    bool ffprobeFound = false;
};

struct RGB
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

static std::wstring utf8ToWide(const std::string& input)
{
    if (input.empty())
        return L"";

    int needed = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, nullptr, 0);
    if (needed <= 0)
    {
        needed = MultiByteToWideChar(CP_ACP, 0, input.c_str(), -1, nullptr, 0);
        if (needed <= 0)
            return L"";

        std::wstring out(needed, L'\0');
        MultiByteToWideChar(CP_ACP, 0, input.c_str(), -1, out.data(), needed);
        if (!out.empty() && out.back() == L'\0')
            out.pop_back();
        return out;
    }

    std::wstring out(needed, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, out.data(), needed);
    if (!out.empty() && out.back() == L'\0')
        out.pop_back();
    return out;
}

static std::string wideToUtf8(const std::wstring& input)
{
    if (input.empty())
        return "";

    int needed = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 0)
        return "";

    std::string out(needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, out.data(), needed, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0')
        out.pop_back();
    return out;
}

static std::string trimCopy(std::string s)
{
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());

    if (s.size() >= 2)
    {
        if ((s.front() == '"' && s.back() == '"') ||
            (s.front() == '\'' && s.back() == '\''))
        {
            s = s.substr(1, s.size() - 2);
        }
    }

    return s;
}

static fs::path getExePath()
{
    wchar_t buffer[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return fs::path(buffer);
}

static fs::path getExeDir()
{
    return getExePath().parent_path();
}

static bool fileExists(const fs::path& p)
{
    std::error_code ec;
    return fs::exists(p, ec) && fs::is_regular_file(p, ec);
}

static ToolPaths detectTools()
{
    ToolPaths tools;
    tools.exeDir = getExeDir();
    tools.binDir = tools.exeDir / L"bin";

    std::vector<fs::path> ffmpegCandidates =
    {
        tools.binDir / L"ffmpeg.exe",
        tools.binDir / L"FFmpeg" / L"ffmpeg.exe",
        tools.binDir / L"ffmpeg" / L"ffmpeg.exe",
        tools.binDir / L"FFmpeg" / L"bin" / L"ffmpeg.exe",
        tools.binDir / L"ffmpeg" / L"bin" / L"ffmpeg.exe"
    };

    std::vector<fs::path> ffprobeCandidates =
    {
        tools.binDir / L"ffprobe.exe",
        tools.binDir / L"FFmpeg" / L"ffprobe.exe",
        tools.binDir / L"ffmpeg" / L"ffprobe.exe",
        tools.binDir / L"FFmpeg" / L"bin" / L"ffprobe.exe",
        tools.binDir / L"ffmpeg" / L"bin" / L"ffprobe.exe"
    };

    for (const auto& p : ffmpegCandidates)
    {
        if (fileExists(p))
        {
            tools.ffmpeg = p;
            tools.ffmpegFound = true;
            break;
        }
    }

    for (const auto& p : ffprobeCandidates)
    {
        if (fileExists(p))
        {
            tools.ffprobe = p;
            tools.ffprobeFound = true;
            break;
        }
    }

    return tools;
}

static std::wstring quoteW(const fs::path& path)
{
    std::wstring s = path.wstring();
    std::wstring out = L"\"";
    for (wchar_t ch : s)
    {
        if (ch == L'"')
            out += L"\\\"";
        else
            out += ch;
    }
    out += L"\"";
    return out;
}

static bool runProcessAndWait(const fs::path& exe, const std::wstring& args, DWORD& exitCode)
{
    exitCode = 999999;

    std::wstring cmd = quoteW(exe) + L" " + args;

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};

    std::vector<wchar_t> cmdMutable(cmd.begin(), cmd.end());
    cmdMutable.push_back(L'\0');

    BOOL ok = CreateProcessW(
        nullptr,
        cmdMutable.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi);

    if (!ok)
        return false;

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return true;
}

static fs::path makeTempPath(const std::wstring& extension)
{
    wchar_t tempDir[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tempDir);

    wchar_t tempFile[MAX_PATH]{};
    GetTempFileNameW(tempDir, L"SFC", 0, tempFile);

    fs::path p(tempFile);
    std::error_code ec;
    fs::remove(p, ec);

    p.replace_extension(extension);
    return p;
}

static void writeU16(std::ofstream& f, uint16_t v)
{
    f.put(static_cast<char>(v & 0xff));
    f.put(static_cast<char>((v >> 8) & 0xff));
}

static void writeU32(std::ofstream& f, uint32_t v)
{
    f.put(static_cast<char>(v & 0xff));
    f.put(static_cast<char>((v >> 8) & 0xff));
    f.put(static_cast<char>((v >> 16) & 0xff));
    f.put(static_cast<char>((v >> 24) & 0xff));
}

static uint32_t readU32LE(const std::vector<uint8_t>& data, size_t pos)
{
    return static_cast<uint32_t>(data[pos + 0]) |
           (static_cast<uint32_t>(data[pos + 1]) << 8) |
           (static_cast<uint32_t>(data[pos + 2]) << 16) |
           (static_cast<uint32_t>(data[pos + 3]) << 24);
}

static uint16_t readU16LE(const std::vector<uint8_t>& data, size_t pos)
{
    return static_cast<uint16_t>(data[pos + 0]) |
           (static_cast<uint16_t>(data[pos + 1]) << 8);
}

static bool loadImageWithWIC(const fs::path& path, ImageRGBA& outImage, std::string& error)
{
    outImage = {};

    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));

    if (FAILED(hr) || !factory)
    {
        error = "Could not create WIC imaging factory.";
        return false;
    }

    IWICBitmapDecoder* decoder = nullptr;

    hr = factory->CreateDecoderFromFilename(
        path.wstring().c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder);

    if (FAILED(hr) || !decoder)
    {
        factory->Release();
        error = "Could not open image with WIC.";
        return false;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);

    if (FAILED(hr) || !frame)
    {
        decoder->Release();
        factory->Release();
        error = "Could not read the first image frame.";
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    frame->GetSize(&width, &height);

    if (width == 0 || height == 0)
    {
        frame->Release();
        decoder->Release();
        factory->Release();
        error = "Image has invalid dimensions.";
        return false;
    }

    IWICFormatConverter* converter = nullptr;
    hr = factory->CreateFormatConverter(&converter);

    if (FAILED(hr) || !converter)
    {
        frame->Release();
        decoder->Release();
        factory->Release();
        error = "Could not create WIC format converter.";
        return false;
    }

    hr = converter->Initialize(
        frame,
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);

    if (FAILED(hr))
    {
        converter->Release();
        frame->Release();
        decoder->Release();
        factory->Release();
        error = "Could not convert image to RGBA.";
        return false;
    }

    const UINT stride = width * 4;
    const UINT bufferSize = stride * height;

    outImage.width = width;
    outImage.height = height;
    outImage.pixels.resize(bufferSize);

    hr = converter->CopyPixels(nullptr, stride, bufferSize, outImage.pixels.data());

    converter->Release();
    frame->Release();
    decoder->Release();
    factory->Release();

    if (FAILED(hr))
    {
        outImage = {};
        error = "Could not copy pixels.";
        return false;
    }

    return true;
}

static bool ffmpegImageToBmp(const fs::path& input, const fs::path& outputBmp, const ToolPaths& tools, std::string& error)
{
    if (!tools.ffmpegFound)
    {
        error = "FFmpeg was not found in bin.";
        return false;
    }

    std::wstring args =
        L"-y -hide_banner -loglevel error -i " + quoteW(input) +
        L" -frames:v 1 " + quoteW(outputBmp);

    DWORD code = 0;
    if (!runProcessAndWait(tools.ffmpeg, args, code))
    {
        error = "Could not start ffmpeg.exe.";
        return false;
    }

    if (code != 0 || !fileExists(outputBmp))
    {
        error = "FFmpeg failed to convert this image.";
        return false;
    }

    return true;
}

static bool loadImageAny(const fs::path& path, ImageRGBA& outImage, const ToolPaths& tools, std::string& error)
{
    if (loadImageWithWIC(path, outImage, error))
        return true;

    if (!tools.ffmpegFound)
    {
        error = "WIC could not load this image, and FFmpeg was not found for fallback loading.";
        return false;
    }

    fs::path tempBmp = makeTempPath(L".bmp");
    std::string fferr;
    if (!ffmpegImageToBmp(path, tempBmp, tools, fferr))
    {
        error = "WIC and FFmpeg could not load this image. " + fferr;
        return false;
    }

    bool ok = loadImageWithWIC(tempBmp, outImage, error);

    std::error_code ec;
    fs::remove(tempBmp, ec);

    return ok;
}

static double pixelBrightness(const ImageRGBA& img, uint32_t x, uint32_t y)
{
    const size_t index = (static_cast<size_t>(y) * img.width + x) * 4;
    const double r = img.pixels[index + 0] / 255.0;
    const double g = img.pixels[index + 1] / 255.0;
    const double b = img.pixels[index + 2] / 255.0;
    const double a = img.pixels[index + 3] / 255.0;

    return (0.2126 * r + 0.7152 * g + 0.0722 * b) * a;
}

static double pseudoNoise01(int n, int v)
{
    double x = std::sin((n * 12.9898) + (v * 78.233)) * 43758.5453;
    return x - std::floor(x);
}

static const char* effectName(SoundEffect e)
{
    switch (e)
    {
    case SoundEffect::Clean: return "Clean Spectral";
    case SoundEffect::DarkDrone: return "Dark Drone";
    case SoundEffect::BrightMelody: return "Bright Melody";
    case SoundEffect::Glitch: return "Glitch Pixels";
    default: return "Unknown";
    }
}

static bool writeWav16Mono(const fs::path& path, int sampleRate, const std::vector<int16_t>& samples, std::string& error)
{
    std::ofstream f(path, std::ios::binary);
    if (!f)
    {
        error = "Could not create WAV file.";
        return false;
    }

    const uint16_t channels = 1;
    const uint16_t bitsPerSample = 16;
    const uint32_t byteRate = sampleRate * channels * bitsPerSample / 8;
    const uint16_t blockAlign = channels * bitsPerSample / 8;
    const uint32_t dataSize = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    const uint32_t riffSize = 36 + dataSize;

    f.write("RIFF", 4);
    writeU32(f, riffSize);
    f.write("WAVE", 4);

    f.write("fmt ", 4);
    writeU32(f, 16);
    writeU16(f, 1);
    writeU16(f, channels);
    writeU32(f, static_cast<uint32_t>(sampleRate));
    writeU32(f, byteRate);
    writeU16(f, blockAlign);
    writeU16(f, bitsPerSample);

    f.write("data", 4);
    writeU32(f, dataSize);
    f.write(reinterpret_cast<const char*>(samples.data()), dataSize);

    if (!f)
    {
        error = "Error while writing WAV data.";
        return false;
    }

    return true;
}

static bool imageToWav(const fs::path& imagePath, const fs::path& outputWav, const AppSettings& settings, const ToolPaths& tools, std::string& error)
{
    ImageRGBA img;
    if (!loadImageAny(imagePath, img, tools, error))
        return false;

    const int sampleRate = std::clamp(settings.sampleRate, 8000, 192000);
    const int totalSamples = static_cast<int>(std::max(1.0, settings.durationSeconds * sampleRate));
    const int voices = std::clamp(settings.voices, 1, 160);

    double minHz = 80.0;
    double maxHz = 4200.0;

    if (settings.effect == SoundEffect::DarkDrone)
    {
        minHz = 35.0;
        maxHz = 1600.0;
    }
    else if (settings.effect == SoundEffect::BrightMelody)
    {
        minHz = 180.0;
        maxHz = 7200.0;
    }
    else if (settings.effect == SoundEffect::Glitch)
    {
        minHz = 70.0;
        maxHz = 5200.0;
    }

    std::vector<int16_t> samples(totalSamples);
    std::vector<double> phases(voices, 0.0);

    for (int n = 0; n < totalSamples; ++n)
    {
        const uint32_t x = static_cast<uint32_t>(
            std::min<uint64_t>(
                img.width - 1,
                (static_cast<uint64_t>(n) * img.width) / static_cast<uint64_t>(totalSamples)));

        double sum = 0.0;

        for (int v = 0; v < voices; ++v)
        {
            const double row01 = voices == 1 ? 0.0 : static_cast<double>(v) / static_cast<double>(voices - 1);
            const uint32_t y = static_cast<uint32_t>(
                std::min<uint32_t>(
                    img.height - 1,
                    static_cast<uint32_t>(row01 * static_cast<double>(img.height - 1))));

            double brightness = pixelBrightness(img, x, img.height - 1 - y);

            if (settings.effect == SoundEffect::Glitch)
            {
                const int block = std::max(1, sampleRate / 28);
                const bool gate = ((n / block) + v) % 3 != 0;
                brightness *= gate ? 1.0 : 0.12;
                brightness = std::clamp(brightness + (pseudoNoise01(n, v) - 0.5) * 0.12, 0.0, 1.0);
            }

            const double freq = minHz * std::pow(maxHz / minHz, row01);
            phases[v] += (2.0 * PI * freq) / static_cast<double>(sampleRate);
            if (phases[v] > 2.0 * PI)
                phases[v] = std::fmod(phases[v], 2.0 * PI);

            double wave = 0.0;

            if (settings.effect == SoundEffect::DarkDrone)
            {
                wave = 0.75 * std::sin(phases[v]) + 0.25 * std::sin(phases[v] * 0.5);
            }
            else if (settings.effect == SoundEffect::BrightMelody)
            {
                wave = 0.68 * std::sin(phases[v]) + 0.22 * std::sin(phases[v] * 2.0) + 0.10 * std::sin(phases[v] * 3.0);
            }
            else if (settings.effect == SoundEffect::Glitch)
            {
                double sine = std::sin(phases[v]);
                double squareish = sine >= 0.0 ? 1.0 : -1.0;
                wave = 0.72 * sine + 0.28 * squareish;
            }
            else
            {
                wave = std::sin(phases[v]);
            }

            sum += brightness * wave;
        }

        double s = (sum / static_cast<double>(voices)) * std::clamp(settings.volume, 0.01, 1.0);

        const int fadeSamples = std::min(sampleRate / 20, totalSamples / 4);
        if (fadeSamples > 0)
        {
            if (n < fadeSamples)
                s *= static_cast<double>(n) / static_cast<double>(fadeSamples);
            else if (n > totalSamples - fadeSamples)
                s *= static_cast<double>(totalSamples - n) / static_cast<double>(fadeSamples);
        }

        s = std::clamp(s, -1.0, 1.0);
        samples[n] = static_cast<int16_t>(s * 32767.0);
    }

    return writeWav16Mono(outputWav, sampleRate, samples, error);
}

static bool loadWav16PCM(const fs::path& path, WavData16& out, std::string& error)
{
    out = {};

    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        error = "Could not open WAV file.";
        return false;
    }

    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    if (bytes.size() < 44 ||
        std::string(reinterpret_cast<char*>(&bytes[0]), 4) != "RIFF" ||
        std::string(reinterpret_cast<char*>(&bytes[8]), 4) != "WAVE")
    {
        error = "This does not look like a valid RIFF/WAVE file.";
        return false;
    }

    bool foundFmt = false;
    bool foundData = false;

    uint16_t audioFormat = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    size_t dataOffset = 0;
    uint32_t dataSize = 0;

    size_t pos = 12;
    while (pos + 8 <= bytes.size())
    {
        std::string chunkId(reinterpret_cast<char*>(&bytes[pos]), 4);
        uint32_t chunkSize = readU32LE(bytes, pos + 4);
        size_t chunkData = pos + 8;

        if (chunkData + chunkSize > bytes.size())
            break;

        if (chunkId == "fmt ")
        {
            if (chunkSize < 16)
            {
                error = "Invalid fmt chunk.";
                return false;
            }

            audioFormat = readU16LE(bytes, chunkData + 0);
            channels = readU16LE(bytes, chunkData + 2);
            sampleRate = readU32LE(bytes, chunkData + 4);
            bitsPerSample = readU16LE(bytes, chunkData + 14);
            foundFmt = true;
        }
        else if (chunkId == "data")
        {
            dataOffset = chunkData;
            dataSize = chunkSize;
            foundData = true;
        }

        pos = chunkData + chunkSize;
        if (pos % 2 != 0)
            ++pos;
    }

    if (!foundFmt || !foundData)
    {
        error = "WAV file is missing fmt or data chunk.";
        return false;
    }

    if (audioFormat != 1)
    {
        error = "Only uncompressed PCM WAV is supported directly. Use FFmpeg import for other audio.";
        return false;
    }

    if (bitsPerSample != 16)
    {
        error = "Only 16-bit PCM WAV is supported directly. Use FFmpeg import for other WAV formats.";
        return false;
    }

    if (channels < 1 || channels > 8)
    {
        error = "Unsupported channel count.";
        return false;
    }

    const size_t sampleCount = dataSize / sizeof(int16_t);
    out.samples.resize(sampleCount);
    std::memcpy(out.samples.data(), bytes.data() + dataOffset, sampleCount * sizeof(int16_t));
    out.sampleRate = static_cast<int>(sampleRate);
    out.channels = static_cast<int>(channels);

    return true;
}

static bool ffmpegAudioToWav(const fs::path& input, const fs::path& outputWav, const ToolPaths& tools, int sampleRate, std::string& error)
{
    if (!tools.ffmpegFound)
    {
        error = "FFmpeg was not found in bin.";
        return false;
    }

    std::wstring args =
        L"-y -hide_banner -loglevel error -i " + quoteW(input) +
        L" -vn -ac 1 -ar " + std::to_wstring(sampleRate) +
        L" -sample_fmt s16 " + quoteW(outputWav);

    DWORD code = 0;
    if (!runProcessAndWait(tools.ffmpeg, args, code))
    {
        error = "Could not start ffmpeg.exe.";
        return false;
    }

    if (code != 0 || !fileExists(outputWav))
    {
        error = "FFmpeg failed to convert audio/video into temporary WAV.";
        return false;
    }

    return true;
}

static bool loadAudioAny(const fs::path& input, WavData16& out, const ToolPaths& tools, int preferredSampleRate, std::string& error)
{
    // Try direct WAV load first because it works without FFmpeg.
    std::string directError;
    if (input.extension() == L".wav" || input.extension() == L".WAV")
    {
        if (loadWav16PCM(input, out, directError))
            return true;
    }

    if (!tools.ffmpegFound)
    {
        error = "Could not load directly, and FFmpeg was not found in bin. Direct loader error: " + directError;
        return false;
    }

    fs::path tempWav = makeTempPath(L".wav");

    std::string fferr;
    if (!ffmpegAudioToWav(input, tempWav, tools, preferredSampleRate, fferr))
    {
        error = fferr;
        return false;
    }

    bool ok = loadWav16PCM(tempWav, out, error);

    std::error_code ec;
    fs::remove(tempWav, ec);

    return ok;
}

static void setPixel(std::vector<RGB>& img, int width, int height, int x, int y, RGB color)
{
    if (x < 0 || y < 0 || x >= width || y >= height)
        return;
    img[static_cast<size_t>(y) * width + x] = color;
}

static void drawLine(std::vector<RGB>& img, int width, int height, int x0, int y0, int x1, int y1, RGB color)
{
    int dx = std::abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true)
    {
        setPixel(img, width, height, x0, y0, color);

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

static bool writeBMP24(const fs::path& path, int width, int height, const std::vector<RGB>& pixels, std::string& error)
{
    if (width <= 0 || height <= 0 || pixels.size() != static_cast<size_t>(width * height))
    {
        error = "Invalid BMP dimensions.";
        return false;
    }

    std::ofstream f(path, std::ios::binary);
    if (!f)
    {
        error = "Could not create BMP file.";
        return false;
    }

    const int rowStride = ((width * 3 + 3) / 4) * 4;
    const uint32_t pixelDataSize = static_cast<uint32_t>(rowStride * height);
    const uint32_t fileSize = 14 + 40 + pixelDataSize;

    f.put('B');
    f.put('M');
    writeU32(f, fileSize);
    writeU16(f, 0);
    writeU16(f, 0);
    writeU32(f, 14 + 40);

    writeU32(f, 40);
    writeU32(f, static_cast<uint32_t>(width));
    writeU32(f, static_cast<uint32_t>(height));
    writeU16(f, 1);
    writeU16(f, 24);
    writeU32(f, 0);
    writeU32(f, pixelDataSize);
    writeU32(f, 2835);
    writeU32(f, 2835);
    writeU32(f, 0);
    writeU32(f, 0);

    std::vector<uint8_t> row(rowStride, 0);

    for (int y = height - 1; y >= 0; --y)
    {
        std::fill(row.begin(), row.end(), 0);
        for (int x = 0; x < width; ++x)
        {
            const RGB c = pixels[static_cast<size_t>(y) * width + x];
            row[x * 3 + 0] = c.b;
            row[x * 3 + 1] = c.g;
            row[x * 3 + 2] = c.r;
        }
        f.write(reinterpret_cast<const char*>(row.data()), row.size());
    }

    if (!f)
    {
        error = "Error while writing BMP data.";
        return false;
    }

    return true;
}

static bool audioToWaveformBmp(const fs::path& audioPath, const fs::path& outputBmp, const ToolPaths& tools, std::string& error)
{
    WavData16 wav;
    if (!loadAudioAny(audioPath, wav, tools, 44100, error))
        return false;

    const int width = 1400;
    const int height = 700;
    const int centerY = height / 2;

    std::vector<RGB> img(static_cast<size_t>(width) * height);
    const RGB bg{ 8, 10, 18 };
    const RGB grid{ 30, 38, 58 };
    const RGB wave{ 90, 220, 255 };
    const RGB center{ 80, 90, 120 };

    std::fill(img.begin(), img.end(), bg);

    for (int x = 0; x < width; x += 50)
        drawLine(img, width, height, x, 0, x, height - 1, grid);

    for (int y = 0; y < height; y += 50)
        drawLine(img, width, height, 0, y, width - 1, y, grid);

    drawLine(img, width, height, 0, centerY, width - 1, centerY, center);

    const size_t frames = wav.samples.size() / static_cast<size_t>(wav.channels);
    if (frames < 2)
    {
        error = "Audio file does not contain enough samples.";
        return false;
    }

    int prevX = 0;
    int prevY = centerY;

    for (int x = 0; x < width; ++x)
    {
        size_t startFrame = (static_cast<uint64_t>(x) * frames) / width;
        size_t endFrame = (static_cast<uint64_t>(x + 1) * frames) / width;
        if (endFrame <= startFrame)
            endFrame = startFrame + 1;
        endFrame = std::min(endFrame, frames);

        int16_t minS = 32767;
        int16_t maxS = -32768;

        for (size_t frame = startFrame; frame < endFrame; ++frame)
        {
            int32_t sum = 0;
            for (int ch = 0; ch < wav.channels; ++ch)
                sum += wav.samples[frame * wav.channels + ch];

            int16_t mono = static_cast<int16_t>(sum / wav.channels);
            minS = std::min(minS, mono);
            maxS = std::max(maxS, mono);
        }

        const double maxAmp = static_cast<double>(maxS) / 32768.0;
        const double minAmp = static_cast<double>(minS) / 32768.0;

        int y1 = centerY - static_cast<int>(maxAmp * (height * 0.45));
        int y2 = centerY - static_cast<int>(minAmp * (height * 0.45));

        if (y1 > y2)
            std::swap(y1, y2);

        drawLine(img, width, height, x, y1, x, y2, wave);

        const int midY = (y1 + y2) / 2;
        if (x > 0)
            drawLine(img, width, height, prevX, prevY, x, midY, wave);

        prevX = x;
        prevY = midY;
    }

    return writeBMP24(outputBmp, width, height, img, error);
}

static void fft(std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();

    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;

        if (i < j)
            std::swap(a[i], a[j]);
    }

    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * PI / static_cast<double>(len);
        const std::complex<double> wlen(std::cos(ang), std::sin(ang));

        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w(1.0, 0.0);
            for (size_t j = 0; j < len / 2; ++j)
            {
                const std::complex<double> u = a[i + j];
                const std::complex<double> v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

static RGB lerpColor(RGB a, RGB b, double t)
{
    t = std::clamp(t, 0.0, 1.0);
    RGB out;
    out.r = static_cast<uint8_t>(a.r + (b.r - a.r) * t);
    out.g = static_cast<uint8_t>(a.g + (b.g - a.g) * t);
    out.b = static_cast<uint8_t>(a.b + (b.b - a.b) * t);
    return out;
}

static RGB spectrogramColor(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    const RGB c0{ 4, 5, 20 };
    const RGB c1{ 30, 20, 90 };
    const RGB c2{ 20, 120, 220 };
    const RGB c3{ 255, 170, 60 };
    const RGB c4{ 255, 250, 210 };

    if (t < 0.25) return lerpColor(c0, c1, t / 0.25);
    if (t < 0.55) return lerpColor(c1, c2, (t - 0.25) / 0.30);
    if (t < 0.82) return lerpColor(c2, c3, (t - 0.55) / 0.27);
    return lerpColor(c3, c4, (t - 0.82) / 0.18);
}

static bool audioToSpectrogramBmp(const fs::path& audioPath, const fs::path& outputBmp, const ToolPaths& tools, std::string& error)
{
    WavData16 wav;
    if (!loadAudioAny(audioPath, wav, tools, 44100, error))
        return false;

    const size_t frames = wav.samples.size() / static_cast<size_t>(wav.channels);
    if (frames < 256)
    {
        error = "Audio file is too short for spectrogram export.";
        return false;
    }

    std::vector<double> mono(frames);
    for (size_t frame = 0; frame < frames; ++frame)
    {
        int32_t sum = 0;
        for (int ch = 0; ch < wav.channels; ++ch)
            sum += wav.samples[frame * wav.channels + ch];
        mono[frame] = (static_cast<double>(sum) / wav.channels) / 32768.0;
    }

    const int width = 1400;
    const int height = 800;
    const int fftSize = 2048;
    const int usableBins = fftSize / 2;

    std::vector<RGB> img(static_cast<size_t>(width) * height);
    const RGB bg{ 4, 5, 20 };
    std::fill(img.begin(), img.end(), bg);

    std::vector<double> hann(fftSize);
    for (int i = 0; i < fftSize; ++i)
        hann[i] = 0.5 * (1.0 - std::cos((2.0 * PI * i) / (fftSize - 1)));

    std::vector<std::complex<double>> buffer(fftSize);

    const double minFreq = 40.0;
    const double maxFreq = std::min(20000.0, wav.sampleRate * 0.5);

    for (int x = 0; x < width; ++x)
    {
        size_t start = 0;
        if (frames > static_cast<size_t>(fftSize))
            start = (static_cast<uint64_t>(x) * (frames - fftSize)) / static_cast<uint64_t>(width - 1);

        for (int i = 0; i < fftSize; ++i)
        {
            const size_t idx = start + static_cast<size_t>(i);
            const double sample = idx < mono.size() ? mono[idx] : 0.0;
            buffer[i] = std::complex<double>(sample * hann[i], 0.0);
        }

        fft(buffer);

        for (int y = 0; y < height; ++y)
        {
            const double top01 = 1.0 - (static_cast<double>(y) / static_cast<double>(height - 1));
            const double freq = minFreq * std::pow(maxFreq / minFreq, top01);
            int bin = static_cast<int>((freq / wav.sampleRate) * fftSize);
            bin = std::clamp(bin, 1, usableBins - 1);

            double mag = std::abs(buffer[bin]) / (fftSize * 0.5);
            double db = 20.0 * std::log10(mag + 1e-9);

            // Normalize roughly -95 dB to -15 dB.
            double t = (db + 95.0) / 80.0;
            t = std::clamp(t, 0.0, 1.0);
            t = std::pow(t, 0.72);

            img[static_cast<size_t>(y) * width + x] = spectrogramColor(t);
        }
    }

    // Add a small border/grid to make the exported image feel more intentional.
    const RGB border{ 70, 90, 130 };
    for (int x = 0; x < width; ++x)
    {
        setPixel(img, width, height, x, 0, border);
        setPixel(img, width, height, x, height - 1, border);
    }
    for (int y = 0; y < height; ++y)
    {
        setPixel(img, width, height, 0, y, border);
        setPixel(img, width, height, width - 1, y, border);
    }

    return writeBMP24(outputBmp, width, height, img, error);
}

static fs::path autoOutputPath(const fs::path& input, const std::wstring& suffix, const std::wstring& ext)
{
    fs::path out = input.parent_path() / (input.stem().wstring() + suffix + ext);
    return out;
}

// ----------------------------- Console Mode -----------------------------

static void clearScreen()
{
    system("cls");
}

static void pauseForUser()
{
    std::cout << "\nPress any key to continue...";
    (void)_getch();
}

static void printTitle()
{
    std::cout
        << "============================================================\n"
        << "  SpectraForge Console v0.3 - Image <-> Sound Prototype\n"
        << "============================================================\n\n";
}

static int interactiveMenu(const std::string& title, const std::vector<std::string>& items)
{
    int selected = 0;

    while (true)
    {
        clearScreen();
        printTitle();
        std::cout << title << "\n";
        std::cout << "Use UP/DOWN or W/S. Press ENTER to select. ESC goes back.\n\n";

        for (int i = 0; i < static_cast<int>(items.size()); ++i)
        {
            if (i == selected)
                std::cout << "  > " << items[i] << "\n";
            else
                std::cout << "    " << items[i] << "\n";
        }

        int key = _getch();

        if (key == 27)
            return -1;

        if (key == 13)
            return selected;

        if (key == 0 || key == 224)
        {
            int arrow = _getch();
            if (arrow == 72)
                selected = (selected - 1 + static_cast<int>(items.size())) % static_cast<int>(items.size());
            else if (arrow == 80)
                selected = (selected + 1) % static_cast<int>(items.size());
        }
        else
        {
            char c = static_cast<char>(std::tolower(key));
            if (c == 'w')
                selected = (selected - 1 + static_cast<int>(items.size())) % static_cast<int>(items.size());
            else if (c == 's')
                selected = (selected + 1) % static_cast<int>(items.size());
        }
    }
}

static std::string askPathConsole(const std::string& prompt)
{
    std::cout << prompt << "\n> ";
    std::string p;
    std::getline(std::cin, p);
    return trimCopy(p);
}

static double askDoubleConsole(const std::string& label, double current, double minValue, double maxValue)
{
    std::cout << label << " [" << current << "]: ";
    std::string s;
    std::getline(std::cin, s);
    s = trimCopy(s);
    if (s.empty())
        return current;

    try
    {
        double v = std::stod(s);
        return std::clamp(v, minValue, maxValue);
    }
    catch (...)
    {
        return current;
    }
}

static int askIntConsole(const std::string& label, int current, int minValue, int maxValue)
{
    std::cout << label << " [" << current << "]: ";
    std::string s;
    std::getline(std::cin, s);
    s = trimCopy(s);
    if (s.empty())
        return current;

    try
    {
        int v = std::stoi(s);
        return std::clamp(v, minValue, maxValue);
    }
    catch (...)
    {
        return current;
    }
}

static fs::path consolePathToFs(const std::string& s)
{
    return fs::path(utf8ToWide(s));
}

static void runConsoleImageToSound(const ToolPaths& tools, const AppSettings& settings)
{
    clearScreen();
    printTitle();

    std::cout << "IMAGE -> WAV SOUND\n\n";
    std::cout << "Effect: " << effectName(settings.effect) << "\n\n";

    std::string imagePathText = askPathConsole("Image file path:");
    if (imagePathText.empty())
        return;

    fs::path imagePath = consolePathToFs(imagePathText);
    fs::path defaultOut = autoOutputPath(imagePath, L"_sound", L".wav");

    std::cout << "\nOutput WAV path:\n";
    std::cout << "Leave blank for: " << wideToUtf8(defaultOut.wstring()) << "\n> ";

    std::string outputPathText;
    std::getline(std::cin, outputPathText);
    outputPathText = trimCopy(outputPathText);

    fs::path outputPath = outputPathText.empty() ? defaultOut : consolePathToFs(outputPathText);

    std::cout << "\nGenerating audio...\n";

    std::string error;
    if (imageToWav(imagePath, outputPath, settings, tools, error))
    {
        std::cout << "\nDone!\nCreated: " << wideToUtf8(outputPath.wstring()) << "\n";
    }
    else
    {
        std::cout << "\nFailed: " << error << "\n";
    }

    pauseForUser();
}

static void runConsoleAudioToWaveform(const ToolPaths& tools)
{
    clearScreen();
    printTitle();

    std::cout << "AUDIO/VIDEO -> BMP WAVEFORM\n\n";

    std::string inputText = askPathConsole("Audio/video file path:");
    if (inputText.empty())
        return;

    fs::path input = consolePathToFs(inputText);
    fs::path defaultOut = autoOutputPath(input, L"_waveform", L".bmp");

    std::cout << "\nOutput BMP path:\n";
    std::cout << "Leave blank for: " << wideToUtf8(defaultOut.wstring()) << "\n> ";

    std::string outputText;
    std::getline(std::cin, outputText);
    outputText = trimCopy(outputText);

    fs::path output = outputText.empty() ? defaultOut : consolePathToFs(outputText);

    std::cout << "\nGenerating waveform image...\n";

    std::string error;
    if (audioToWaveformBmp(input, output, tools, error))
    {
        std::cout << "\nDone!\nCreated: " << wideToUtf8(output.wstring()) << "\n";
    }
    else
    {
        std::cout << "\nFailed: " << error << "\n";
    }

    pauseForUser();
}

static void runConsoleAudioToSpectrogram(const ToolPaths& tools)
{
    clearScreen();
    printTitle();

    std::cout << "AUDIO/VIDEO -> REAL SPECTROGRAM BMP\n\n";
    std::cout << "This uses FFmpeg for import and an internal FFT renderer for the spectrogram.\n\n";

    std::string inputText = askPathConsole("Audio/video file path:");
    if (inputText.empty())
        return;

    fs::path input = consolePathToFs(inputText);
    fs::path defaultOut = autoOutputPath(input, L"_spectrogram", L".bmp");

    std::cout << "\nOutput BMP path:\n";
    std::cout << "Leave blank for: " << wideToUtf8(defaultOut.wstring()) << "\n> ";

    std::string outputText;
    std::getline(std::cin, outputText);
    outputText = trimCopy(outputText);

    fs::path output = outputText.empty() ? defaultOut : consolePathToFs(outputText);

    std::cout << "\nGenerating spectrogram image...\n";

    std::string error;
    if (audioToSpectrogramBmp(input, output, tools, error))
    {
        std::cout << "\nDone!\nCreated: " << wideToUtf8(output.wstring()) << "\n";
    }
    else
    {
        std::cout << "\nFailed: " << error << "\n";
    }

    pauseForUser();
}

static void runConsoleSettings(AppSettings& settings)
{
    while (true)
    {
        std::ostringstream title;
        title << "SETTINGS\n\n"
              << "Current settings:\n"
              << "  Sample Rate: " << settings.sampleRate << " Hz\n"
              << "  Duration:    " << settings.durationSeconds << " seconds\n"
              << "  Voices:      " << settings.voices << "\n"
              << "  Volume:      " << settings.volume << "\n"
              << "  Effect:      " << effectName(settings.effect) << "\n\n"
              << "Choose setting:";

        int choice = interactiveMenu(title.str(), {
            "Change sample rate",
            "Change image-to-sound duration",
            "Change voice count / frequency detail",
            "Change volume",
            "Change effect",
            "Reset defaults",
            "Back"
        });

        if (choice < 0 || choice == 6)
            return;

        clearScreen();
        printTitle();

        switch (choice)
        {
        case 0:
            std::cout << "Sample rate examples: 44100 or 48000\n";
            settings.sampleRate = askIntConsole("New sample rate", settings.sampleRate, 8000, 192000);
            break;
        case 1:
            settings.durationSeconds = askDoubleConsole("New duration in seconds", settings.durationSeconds, 0.5, 120.0);
            break;
        case 2:
            settings.voices = askIntConsole("New voice count", settings.voices, 1, 160);
            break;
        case 3:
            settings.volume = askDoubleConsole("New volume", settings.volume, 0.01, 1.0);
            break;
        case 4:
        {
            int effect = interactiveMenu("CHOOSE EFFECT", {
                "Clean Spectral",
                "Dark Drone",
                "Bright Melody",
                "Glitch Pixels"
            });
            if (effect >= 0)
                settings.effect = static_cast<SoundEffect>(effect);
            break;
        }
        case 5:
            settings = AppSettings{};
            std::cout << "Defaults restored.\n";
            pauseForUser();
            break;
        default:
            break;
        }
    }
}

static void showConsoleToolStatus(const ToolPaths& tools)
{
    clearScreen();
    printTitle();

    std::cout << "BIN / FFMPEG STATUS\n\n";
    std::cout << "EXE dir: " << wideToUtf8(tools.exeDir.wstring()) << "\n";
    std::cout << "bin dir: " << wideToUtf8(tools.binDir.wstring()) << "\n\n";

    if (tools.ffmpegFound)
        std::cout << "ffmpeg:  FOUND - " << wideToUtf8(tools.ffmpeg.wstring()) << "\n";
    else
        std::cout << "ffmpeg:  NOT FOUND\n";

    if (tools.ffprobeFound)
        std::cout << "ffprobe: FOUND - " << wideToUtf8(tools.ffprobe.wstring()) << "\n";
    else
        std::cout << "ffprobe: NOT FOUND\n";

    std::cout << "\nExpected location example:\n";
    std::cout << "  SpectraForgeConsole.exe\n";
    std::cout << "  bin\\ffmpeg.exe\n";
    std::cout << "  bin\\ffprobe.exe\n";

    pauseForUser();
}

static int runConsoleMode()
{
    AllocConsole();
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    FILE* dummy = nullptr;
    freopen_s(&dummy, "CONOUT$", "w", stdout);
    freopen_s(&dummy, "CONOUT$", "w", stderr);
    freopen_s(&dummy, "CONIN$", "r", stdin);

    HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    AppSettings settings;
    ToolPaths tools = detectTools();

    while (true)
    {
        int choice = interactiveMenu("MAIN MENU", {
            "Image -> WAV sound",
            "Audio/video -> BMP waveform image",
            "Audio/video -> BMP spectrogram image",
            "Settings / effects",
            "FFmpeg / bin resource status",
            "Refresh FFmpeg detection",
            "Exit"
        });

        if (choice < 0 || choice == 6)
            break;

        switch (choice)
        {
        case 0:
            runConsoleImageToSound(tools, settings);
            break;
        case 1:
            runConsoleAudioToWaveform(tools);
            break;
        case 2:
            runConsoleAudioToSpectrogram(tools);
            break;
        case 3:
            runConsoleSettings(settings);
            break;
        case 4:
            showConsoleToolStatus(tools);
            break;
        case 5:
            tools = detectTools();
            clearScreen();
            printTitle();
            std::cout << "FFmpeg detection refreshed.\n";
            pauseForUser();
            break;
        default:
            break;
        }
    }

    if (SUCCEEDED(comResult))
        CoUninitialize();

    return 0;
}

// ----------------------------- GUI Mode -----------------------------

#define IDC_IMG_INPUT          1001
#define IDC_IMG_BROWSE         1002
#define IDC_IMG_OUTPUT         1003
#define IDC_IMG_SAVE           1004
#define IDC_IMG_CONVERT        1005

#define IDC_AUDIO_INPUT        1101
#define IDC_AUDIO_BROWSE       1102
#define IDC_AUDIO_OUTPUT       1103
#define IDC_AUDIO_SAVE         1104
#define IDC_AUDIO_MODE         1105
#define IDC_AUDIO_EXPORT       1106

#define IDC_SAMPLE_RATE        1201
#define IDC_DURATION           1202
#define IDC_VOICES             1203
#define IDC_VOLUME             1204
#define IDC_EFFECT             1205
#define IDC_STATUS             1301
#define IDC_REFRESH            1302
#define IDC_CONSOLE_HELP       1303

static HINSTANCE gInstance = nullptr;
static HWND gMainWindow = nullptr;
static HWND gStatus = nullptr;
static HWND gImgInput = nullptr;
static HWND gImgOutput = nullptr;
static HWND gAudioInput = nullptr;
static HWND gAudioOutput = nullptr;
static HWND gAudioMode = nullptr;
static HWND gSampleRate = nullptr;
static HWND gDuration = nullptr;
static HWND gVoices = nullptr;
static HWND gVolume = nullptr;
static HWND gEffect = nullptr;
static HFONT gFont = nullptr;
static ToolPaths gTools;

static std::wstring getWindowTextWString(HWND hwnd)
{
    int len = GetWindowTextLengthW(hwnd);
    std::wstring buffer(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(hwnd, buffer.data(), len + 1);
    buffer.resize(static_cast<size_t>(len));
    return buffer;
}

static void setControlFont(HWND hwnd)
{
    if (gFont)
        SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(gFont), TRUE);
}

static HWND makeLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h)
{
    HWND hwnd = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
        x, y, w, h, parent, nullptr, gInstance, nullptr);
    setControlFont(hwnd);
    return hwnd;
}

static HWND makeEdit(HWND parent, int id, const wchar_t* text, int x, int y, int w, int h)
{
    HWND hwnd = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        x, y, w, h, parent, reinterpret_cast<HMENU>(id), gInstance, nullptr);
    setControlFont(hwnd);
    return hwnd;
}

static HWND makeButton(HWND parent, int id, const wchar_t* text, int x, int y, int w, int h)
{
    HWND hwnd = CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, w, h, parent, reinterpret_cast<HMENU>(id), gInstance, nullptr);
    setControlFont(hwnd);
    return hwnd;
}

static HWND makeGroup(HWND parent, const wchar_t* text, int x, int y, int w, int h)
{
    HWND hwnd = CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x, y, w, h, parent, nullptr, gInstance, nullptr);
    setControlFont(hwnd);
    return hwnd;
}

static HWND makeCombo(HWND parent, int id, int x, int y, int w, int h)
{
    HWND hwnd = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        x, y, w, h, parent, reinterpret_cast<HMENU>(id), gInstance, nullptr);
    setControlFont(hwnd);
    return hwnd;
}

static void setStatus(const std::wstring& text)
{
    if (gStatus)
    {
        SetWindowTextW(gStatus, text.c_str());
        UpdateWindow(gStatus);
    }
}

static void updateToolStatusGui()
{
    gTools = detectTools();

    std::wstring status = L"bin resources: ";
    status += gTools.ffmpegFound ? L"FFmpeg found" : L"FFmpeg not found";
    status += L" | ";
    status += gTools.ffprobeFound ? L"FFprobe found" : L"FFprobe not found";
    status += L" | bin: ";
    status += gTools.binDir.wstring();

    setStatus(status);
}

static bool parseGuiSettings(AppSettings& out, std::wstring& error)
{
    try
    {
        out.sampleRate = std::clamp(std::stoi(getWindowTextWString(gSampleRate)), 8000, 192000);
        out.durationSeconds = std::clamp(std::stod(getWindowTextWString(gDuration)), 0.5, 120.0);
        out.voices = std::clamp(std::stoi(getWindowTextWString(gVoices)), 1, 160);
        out.volume = std::clamp(std::stod(getWindowTextWString(gVolume)), 0.01, 1.0);

        int effectIndex = static_cast<int>(SendMessageW(gEffect, CB_GETCURSEL, 0, 0));
        if (effectIndex < 0)
            effectIndex = 0;
        out.effect = static_cast<SoundEffect>(effectIndex);
        return true;
    }
    catch (...)
    {
        error = L"One of the settings fields is invalid.";
        return false;
    }
}

static std::wstring openFileDialog(HWND owner, const wchar_t* filter)
{
    wchar_t file[MAX_PATH]{};

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn))
        return file;

    return L"";
}

static std::wstring saveFileDialog(HWND owner, const wchar_t* filter, const wchar_t* defaultExt)
{
    wchar_t file[MAX_PATH]{};

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = defaultExt;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (GetSaveFileNameW(&ofn))
        return file;

    return L"";
}

static void autoSetImageOutput()
{
    fs::path input(getWindowTextWString(gImgInput));
    if (!input.empty())
    {
        fs::path out = autoOutputPath(input, L"_sound", L".wav");
        SetWindowTextW(gImgOutput, out.wstring().c_str());
    }
}

static void autoSetAudioOutput()
{
    fs::path input(getWindowTextWString(gAudioInput));
    if (!input.empty())
    {
        int mode = static_cast<int>(SendMessageW(gAudioMode, CB_GETCURSEL, 0, 0));
        fs::path out = autoOutputPath(input, mode == 0 ? L"_waveform" : L"_spectrogram", L".bmp");
        SetWindowTextW(gAudioOutput, out.wstring().c_str());
    }
}

static void doGuiImageToSound()
{
    AppSettings settings;
    std::wstring settingsError;
    if (!parseGuiSettings(settings, settingsError))
    {
        MessageBoxW(gMainWindow, settingsError.c_str(), L"Settings Error", MB_ICONERROR);
        return;
    }

    fs::path input(getWindowTextWString(gImgInput));
    fs::path output(getWindowTextWString(gImgOutput));

    if (input.empty() || output.empty())
    {
        MessageBoxW(gMainWindow, L"Please choose an input image and output WAV path.", L"Missing Path", MB_ICONWARNING);
        return;
    }

    setStatus(L"Working: converting image to WAV...");
    EnableWindow(gMainWindow, FALSE);

    std::string error;
    bool ok = imageToWav(input, output, settings, gTools, error);

    EnableWindow(gMainWindow, TRUE);
    SetForegroundWindow(gMainWindow);

    if (ok)
    {
        setStatus(L"Done: image converted to WAV.");
        MessageBoxW(gMainWindow, (L"Created:\n" + output.wstring()).c_str(), L"Done", MB_ICONINFORMATION);
    }
    else
    {
        setStatus(L"Failed: image to WAV.");
        MessageBoxW(gMainWindow, utf8ToWide(error).c_str(), L"Conversion Failed", MB_ICONERROR);
    }
}

static void doGuiAudioExport()
{
    fs::path input(getWindowTextWString(gAudioInput));
    fs::path output(getWindowTextWString(gAudioOutput));

    if (input.empty() || output.empty())
    {
        MessageBoxW(gMainWindow, L"Please choose an input audio/video file and output BMP path.", L"Missing Path", MB_ICONWARNING);
        return;
    }

    int mode = static_cast<int>(SendMessageW(gAudioMode, CB_GETCURSEL, 0, 0));
    if (mode < 0)
        mode = 0;

    if (mode == 0)
        setStatus(L"Working: generating waveform image...");
    else
        setStatus(L"Working: generating real FFT spectrogram image...");

    EnableWindow(gMainWindow, FALSE);

    std::string error;
    bool ok = false;

    if (mode == 0)
        ok = audioToWaveformBmp(input, output, gTools, error);
    else
        ok = audioToSpectrogramBmp(input, output, gTools, error);

    EnableWindow(gMainWindow, TRUE);
    SetForegroundWindow(gMainWindow);

    if (ok)
    {
        setStatus(mode == 0 ? L"Done: waveform image exported." : L"Done: spectrogram image exported.");
        MessageBoxW(gMainWindow, (L"Created:\n" + output.wstring()).c_str(), L"Done", MB_ICONINFORMATION);
    }
    else
    {
        setStatus(L"Failed: audio image export.");
        MessageBoxW(gMainWindow, utf8ToWide(error).c_str(), L"Export Failed", MB_ICONERROR);
    }
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        gMainWindow = hwnd;
        gFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

        makeLabel(hwnd, L"SpectraForge v0.3 - Image <-> Sound Tool", 18, 12, 520, 24);
        gStatus = makeLabel(hwnd, L"Checking bin resources...", 18, 42, 760, 20);

        makeGroup(hwnd, L"Image -> WAV Sound", 18, 75, 760, 135);
        makeLabel(hwnd, L"Input image:", 35, 105, 90, 22);
        gImgInput = makeEdit(hwnd, IDC_IMG_INPUT, L"", 125, 102, 505, 24);
        makeButton(hwnd, IDC_IMG_BROWSE, L"Browse...", 640, 101, 115, 26);

        makeLabel(hwnd, L"Output WAV:", 35, 138, 90, 22);
        gImgOutput = makeEdit(hwnd, IDC_IMG_OUTPUT, L"", 125, 135, 505, 24);
        makeButton(hwnd, IDC_IMG_SAVE, L"Save As...", 640, 134, 115, 26);

        makeButton(hwnd, IDC_IMG_CONVERT, L"Convert Image to WAV", 125, 170, 210, 28);

        makeGroup(hwnd, L"Audio / Video -> Image", 18, 225, 760, 155);
        makeLabel(hwnd, L"Input audio/video:", 35, 255, 110, 22);
        gAudioInput = makeEdit(hwnd, IDC_AUDIO_INPUT, L"", 150, 252, 480, 24);
        makeButton(hwnd, IDC_AUDIO_BROWSE, L"Browse...", 640, 251, 115, 26);

        makeLabel(hwnd, L"Output BMP:", 35, 288, 110, 22);
        gAudioOutput = makeEdit(hwnd, IDC_AUDIO_OUTPUT, L"", 150, 285, 480, 24);
        makeButton(hwnd, IDC_AUDIO_SAVE, L"Save As...", 640, 284, 115, 26);

        makeLabel(hwnd, L"Export type:", 35, 322, 110, 22);
        gAudioMode = makeCombo(hwnd, IDC_AUDIO_MODE, 150, 318, 220, 200);
        SendMessageW(gAudioMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Waveform BMP"));
        SendMessageW(gAudioMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Real Spectrogram BMP"));
        SendMessageW(gAudioMode, CB_SETCURSEL, 1, 0);

        makeButton(hwnd, IDC_AUDIO_EXPORT, L"Export Audio Image", 390, 317, 190, 28);

        makeGroup(hwnd, L"Image-to-Sound Settings / Effects", 18, 395, 760, 100);
        makeLabel(hwnd, L"Sample Rate:", 35, 425, 90, 22);
        gSampleRate = makeEdit(hwnd, IDC_SAMPLE_RATE, L"44100", 125, 422, 80, 24);

        makeLabel(hwnd, L"Duration:", 225, 425, 70, 22);
        gDuration = makeEdit(hwnd, IDC_DURATION, L"8.0", 295, 422, 60, 24);

        makeLabel(hwnd, L"Voices:", 375, 425, 55, 22);
        gVoices = makeEdit(hwnd, IDC_VOICES, L"48", 430, 422, 55, 24);

        makeLabel(hwnd, L"Volume:", 505, 425, 55, 22);
        gVolume = makeEdit(hwnd, IDC_VOLUME, L"0.35", 560, 422, 60, 24);

        makeLabel(hwnd, L"Effect:", 35, 458, 90, 22);
        gEffect = makeCombo(hwnd, IDC_EFFECT, 125, 454, 220, 160);
        SendMessageW(gEffect, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Clean Spectral"));
        SendMessageW(gEffect, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Dark Drone"));
        SendMessageW(gEffect, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Bright Melody"));
        SendMessageW(gEffect, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Glitch Pixels"));
        SendMessageW(gEffect, CB_SETCURSEL, 0, 0);

        makeButton(hwnd, IDC_REFRESH, L"Refresh FFmpeg Detection", 18, 515, 210, 30);
        makeButton(hwnd, IDC_CONSOLE_HELP, L"Console Mode Help", 245, 515, 170, 30);

        updateToolStatusGui();
        return 0;
    }

    case WM_COMMAND:
    {
        const int id = LOWORD(wParam);
        const int code = HIWORD(wParam);

        if (id == IDC_IMG_BROWSE)
        {
            const wchar_t filter[] =
                L"Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff;*.webp;*.avif;*.jxl;*.ico\0"
                L"All Files\0*.*\0";
            std::wstring p = openFileDialog(hwnd, filter);
            if (!p.empty())
            {
                SetWindowTextW(gImgInput, p.c_str());
                autoSetImageOutput();
            }
        }
        else if (id == IDC_IMG_SAVE)
        {
            const wchar_t filter[] = L"WAV Audio\0*.wav\0All Files\0*.*\0";
            std::wstring p = saveFileDialog(hwnd, filter, L"wav");
            if (!p.empty())
                SetWindowTextW(gImgOutput, p.c_str());
        }
        else if (id == IDC_IMG_CONVERT)
        {
            doGuiImageToSound();
        }
        else if (id == IDC_AUDIO_BROWSE)
        {
            const wchar_t filter[] =
                L"Audio/Video Files\0*.wav;*.mp3;*.ogg;*.flac;*.m4a;*.aac;*.wma;*.mp4;*.mkv;*.webm;*.avi;*.mov\0"
                L"All Files\0*.*\0";
            std::wstring p = openFileDialog(hwnd, filter);
            if (!p.empty())
            {
                SetWindowTextW(gAudioInput, p.c_str());
                autoSetAudioOutput();
            }
        }
        else if (id == IDC_AUDIO_SAVE)
        {
            const wchar_t filter[] = L"BMP Image\0*.bmp\0All Files\0*.*\0";
            std::wstring p = saveFileDialog(hwnd, filter, L"bmp");
            if (!p.empty())
                SetWindowTextW(gAudioOutput, p.c_str());
        }
        else if (id == IDC_AUDIO_EXPORT)
        {
            doGuiAudioExport();
        }
        else if (id == IDC_AUDIO_MODE && code == CBN_SELCHANGE)
        {
            autoSetAudioOutput();
        }
        else if (id == IDC_REFRESH)
        {
            updateToolStatusGui();
        }
        else if (id == IDC_CONSOLE_HELP)
        {
            MessageBoxW(hwnd,
                L"To use the old interactive console mode, run:\n\n"
                L"SpectraForgeConsole.exe --console\n\n"
                L"The console mode still uses arrow keys / W-S, not number-only commands.",
                L"Console Mode",
                MB_ICONINFORMATION);
        }

        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static int runGuiMode(HINSTANCE hInstance)
{
    gInstance = hInstance;

    HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    const wchar_t* className = L"SpectraForgeConsoleGuiWindow";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = className;

    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        className,
        L"SpectraForge v0.3",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        820,
        610,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hwnd)
    {
        if (SUCCEEDED(comResult))
            CoUninitialize();
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (SUCCEEDED(comResult))
        CoUninitialize();

    return static_cast<int>(msg.wParam);
}

int main(int argc, char** argv)
{
    bool consoleMode = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        std::transform(arg.begin(), arg.end(), arg.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (arg == "--console" || arg == "/console" || arg == "-console")
            consoleMode = true;
    }

    if (consoleMode)
    {
        return runConsoleMode();
    }

    // The project is intentionally built as a Console subsystem app so that
    // --console works easily. For GUI mode, hide the console immediately.
    FreeConsole();

    return runGuiMode(GetModuleHandleW(nullptr));
}
