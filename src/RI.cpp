#include "MiniFB.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../deps/stb/stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../deps/stb/stb_image_resize2.h"

#include <thread>
#include <random>
#include <chrono>

struct Pixel
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
};

constexpr auto MAX_RENDER_SIZE_X = 4096;
constexpr auto MAX_RENDER_SIZE_Y = 4096;

static_assert(MAX_RENDER_SIZE_X <= (1 << 12));
static_assert(MAX_RENDER_SIZE_Y <= (1 << 12));

struct TargetPixelComponent
{
    uint32_t x : 12;
    uint32_t y : 12;
    uint32_t c : 8;
};

static std::atomic<uint32_t> g_NumFinishedWorkers = 0;

struct Worker
{
    // Flag to synchronize rendering and display
    std::atomic<bool> m_PendingUpdates = false;

    uint32_t m_PixelCount = 0;
    uint32_t m_FbStride = UINT32_MAX;
    Pixel* m_pFrameBuffer = nullptr;

    uint32_t m_RedChIdx = 0;
    uint32_t m_GreenChIdx = 0;
    uint32_t m_BlueChIdx = 0;
    uint32_t m_AlphaChIdx = 0;

    TargetPixelComponent* m_pRedList = nullptr;
    TargetPixelComponent* m_pGreenList = nullptr;
    TargetPixelComponent* m_pBlueList = nullptr;
    TargetPixelComponent* m_pAlphaList = nullptr;

    ~Worker()
    {
        delete[] m_pRedList;
        delete[] m_pGreenList;
        delete[] m_pBlueList;
        delete[] m_pAlphaList;
    }

    void Init(Pixel* pTargetImage, Pixel* pFrameBuffer, uint32_t pixelCount, uint32_t fbWidth)
    {
        m_pFrameBuffer = pFrameBuffer;
        m_PixelCount = pixelCount;
        m_FbStride = fbWidth;

        m_pRedList = new TargetPixelComponent[m_PixelCount];
        m_pGreenList = new TargetPixelComponent[m_PixelCount];
        m_pBlueList = new TargetPixelComponent[m_PixelCount];
        m_pAlphaList = new TargetPixelComponent[m_PixelCount];

        for (uint32_t i = 0; i < m_PixelCount; i++)
        {
            uint32_t x = i % m_FbStride;
            uint32_t y = i / m_FbStride;

            m_pRedList[i] = { x, y, pTargetImage[x + y * m_FbStride].red };
            m_pGreenList[i] = { x, y, pTargetImage[x + y * m_FbStride].green };
            m_pBlueList[i] = { x, y, pTargetImage[x + y * m_FbStride].blue };
            m_pAlphaList[i] = { x, y, pTargetImage[x + y * m_FbStride].alpha };
        }

        std::random_device rd;
        std::default_random_engine g(rd());

        // Shuffle per-channel arrays to better visualize randomness of rendering
        std::shuffle(m_pRedList, m_pRedList + m_PixelCount, g);
        std::shuffle(m_pGreenList, m_pGreenList + m_PixelCount, g);
        std::shuffle(m_pBlueList, m_pBlueList + m_PixelCount, g);
        std::shuffle(m_pAlphaList, m_pAlphaList + m_PixelCount, g);
    }

    void Run(uint32_t pixelIterSize)
    {
        std::random_device rd;
        std::default_random_engine gen(rd());
        std::uniform_int_distribution<uint32_t> dist{ 0, UINT8_MAX }; // 8bpc

        auto lbdRandomGuesser = [&dist, &gen](uint8_t val)
        {
            while (dist(gen) != val)
            {
                // Spin until we hit the jackpot
            }
        };

        bool isAnyWorkLeft = true;

        while (isAnyWorkLeft)
        {
            // Stall until pending updates are posted to the frame buffer
            m_PendingUpdates.wait(true);

            if (m_RedChIdx < m_PixelCount)
            {
                auto remRedCh = m_PixelCount - m_RedChIdx;
                auto numElems = std::min(remRedCh, pixelIterSize);

                for (uint32_t pc = 0; pc < numElems; pc++)
                {
                    auto rp = m_pRedList[m_RedChIdx++];

                    lbdRandomGuesser(rp.c);

                    m_pFrameBuffer[rp.y * m_FbStride + rp.x].red = rp.c;
                }
            }

            if (m_GreenChIdx < m_PixelCount)
            {
                auto remGreenCh = m_PixelCount - m_GreenChIdx;
                auto numElems = std::min(remGreenCh, pixelIterSize);

                for (uint32_t pc = 0; pc < numElems; pc++)
                {
                    auto gp = m_pGreenList[m_GreenChIdx++];

                    lbdRandomGuesser(gp.c);

                    m_pFrameBuffer[gp.y * m_FbStride + gp.x].green = gp.c;
                }
            }

            if (m_BlueChIdx < m_PixelCount)
            {
                auto remBlueCh = m_PixelCount - m_BlueChIdx;
                auto numElems = std::min(remBlueCh, pixelIterSize);

                for (uint32_t pc = 0; pc < numElems; pc++)
                {
                    auto bp = m_pBlueList[m_BlueChIdx++];

                    lbdRandomGuesser(bp.c);

                    m_pFrameBuffer[bp.y * m_FbStride + bp.x].blue = bp.c;
                }
            }

            if (m_AlphaChIdx < m_PixelCount)
            {
                auto remAlphaCh = m_PixelCount - m_AlphaChIdx;
                auto numElems = std::min(remAlphaCh, pixelIterSize);

                for (uint32_t pc = 0; pc < numElems; pc++)
                {
                    auto ap = m_pAlphaList[m_AlphaChIdx++];

                    lbdRandomGuesser(ap.c);

                    m_pFrameBuffer[ap.y * m_FbStride + ap.x].alpha = ap.c;
                }
            }

            m_PendingUpdates = true;
            m_PendingUpdates.notify_one();

            isAnyWorkLeft = ((m_RedChIdx < m_PixelCount) ||
                (m_GreenChIdx < m_PixelCount) ||
                (m_BlueChIdx < m_PixelCount) ||
                (m_AlphaChIdx < m_PixelCount));
        }

        g_NumFinishedWorkers++;
    }
};

struct Config
{
    // If unset, default to std::thread::hardware_concurrency
    uint32_t numThreads = 0;

    // Default window resolution
    uint32_t width = 1280;
    uint32_t height = 720;

    // How many pixels are guessed by each thread per frame
    uint32_t stepSizeInPixels = 512;
};

bool ParseCommandLine(int argc, char** ppArgv, Config& config)
{
    for (int i = 1; i < argc; i++)
    {
        if (strcmp("--help", ppArgv[i]) == 0)
        {
            printf("RI <path_to_target_image>\n\n");
            printf("Press ESC to exit or SPACE to pause\n\n");

            printf("Additional parameters:\n");
            printf("\t-n <NUMBER OF THREADS>\n");
            printf("\t-w <SCREEN WIDTH>\n");
            printf("\t-h <SCREEN HEIGHT>\n");
            printf("\t-p <PIXELS PER ITERATION>\n");

            return false;
        }
        else if (strcmp("-n", ppArgv[i]) == 0)
        {
            if (i + 1 >= argc)
            {
                printf("ERROR: -n requires an argument\n");
                return false;
            }

            auto res = atoi(ppArgv[++i]);
            if (res <= 0)
            {
                printf("ERROR: Invalid number of threads\n");
                return false;
            }
            else
            {
                config.numThreads = res;
            }
        }
        else if (strcmp("-w", ppArgv[i]) == 0)
        {
            if (i + 1 >= argc)
            {
                printf("ERROR: -w requires an argument\n");
                return false;
            }

            auto res = atoi(ppArgv[++i]);
            if (res < 0)
            {
                printf("ERROR: Invalid screen width\n");
                return false;
            }
            else
            {
                if (res > MAX_RENDER_SIZE_X)
                {
                    printf("ERROR: Too large a width\n");
                    return false;
                }

                config.width = res;
            }
        }
        else if (strcmp("-h", ppArgv[i]) == 0)
        {
            if (i + 1 >= argc)
            {
                printf("ERROR: -h requires an argument\n");
                return false;
            }

            auto res = atoi(ppArgv[++i]);
            if (res < 0)
            {
                printf("ERROR: Invalid screen height\n");
                return false;
            }
            else
            {
                if (res > MAX_RENDER_SIZE_Y)
                {
                    printf("ERROR: Too large a height\n");
                    return false;
                }

                config.height = res;
            }
        }
        else if (strcmp("-p", ppArgv[i]) == 0)
        {
            if (i + 1 >= argc)
            {
                printf("ERROR: -p requires an argument\n");
                return false;
            }

            auto res = atoi(ppArgv[++i]);
            if (res <= 0)
            {
                printf("ERROR: Invalid pixel iteration size\n");
                return false;
            }

            config.stepSizeInPixels = res;
        }
    }

    return true;
}

// Used as target image during initialization
// Used as frame buffer during rendering
Pixel g_pFrameBuffer[MAX_RENDER_SIZE_X * MAX_RENDER_SIZE_Y];

int main(int argc, char** ppargv)
{
    Config config = {};
    if (ParseCommandLine(argc, ppargv, config) == false)
        exit(EXIT_FAILURE);

    const auto numWorkers = (config.numThreads == 0) ? std::thread::hardware_concurrency() : config.numThreads;

    // Load input image and resize if necessary
    {
        int stbX, stbY, stbComp;
        uint8_t* pInputImageData = stbi_load(
            (argc == 1) ? "test.jpg" : ppargv[1], // Default run w/ test.jpg next to executable
            &stbX,
            &stbY,
            &stbComp,
            4); // Force 4 components

        if (pInputImageData == nullptr)
        {
            printf("ERROR: Unable to load input image\n");
            exit(EXIT_FAILURE);
        }

        if ((stbComp != 3) && (stbComp != 4))
        {
            printf("ERROR: Only 3- or 4-component images!\n");
            exit(EXIT_FAILURE);
        }

        bool resized = false;
        uint8_t* pResizedImage = pInputImageData;

        // Readjust window orientation
        if ((stbY > stbX) && (config.width > config.height))
            std::swap(config.width, config.height);

        if ((stbX != (int)config.width) || (stbY != (int)config.height))
        {
            // Resize image to window size
            pResizedImage = stbir_resize_uint8_srgb(
                pInputImageData,
                stbX,
                stbY,
                0,       // TODO: Handle both 3- and 4-channel images!
                nullptr, // Have stb allocate an output buffer
                config.width,
                config.height,
                0,
                STBIR_RGBA);

            stbi_image_free(pInputImageData);
            pInputImageData = nullptr;

            resized = true;
        }

        if (pResizedImage == nullptr)
        {
            printf("ERROR: Internal error resizing the input image to window\n");
            exit(EXIT_FAILURE);
        }

        for (uint32_t y = 0; y < config.height; y++)
        {
            for (uint32_t x = 0; x < config.width; x++)
            {
                // Map to BGRA format
                g_pFrameBuffer[y * config.width + x].blue = pResizedImage[(y * config.width + x) * 4 + 0];
                g_pFrameBuffer[y * config.width + x].green = pResizedImage[(y * config.width + x) * 4 + 1];
                g_pFrameBuffer[y * config.width + x].red = pResizedImage[(y * config.width + x) * 4 + 2];
                g_pFrameBuffer[y * config.width + x].alpha = pResizedImage[(y * config.width + x) * 4 + 3];

                // TODO: Handle alpha channel!
            }
        }

        stbi_image_free(pInputImageData);

        if (resized)
            STBI_FREE(pResizedImage);
    }

    auto pWindow = mfb_open_ex("RI", config.width, config.height, WF_BORDERLESS);
    if (pWindow == nullptr)
    {
        printf("ERROR: Failed to create a window!");
        exit(EXIT_FAILURE);
    }

    Worker* pWorkers = new Worker[numWorkers];
    assert(pWorkers != nullptr);

    const uint32_t perThreadPixelCount = (config.width * config.height) / numWorkers;

    // Divide flattened screen space (width * height pixels) into N [# of threads] regions
    // so that each thread can work independently until the screen refresh
    uint32_t pixelOffset = 0;
    for (uint32_t i = 0; i < numWorkers; i++)
    {
        uint32_t maxPerThreadPixelCount =
            (i == (numWorkers - 1))
            ? (perThreadPixelCount + ((config.width * config.height) % numWorkers))
            : perThreadPixelCount;

        pWorkers[i].Init(&g_pFrameBuffer[pixelOffset],
            &g_pFrameBuffer[pixelOffset],
            maxPerThreadPixelCount,
            config.width);

        pixelOffset += maxPerThreadPixelCount;
    }

    // Each worker has its own copy of target image buffer, start rendering with a blank screen
    memset(&g_pFrameBuffer[0], 0x0, sizeof(Pixel) * config.width * config.height);

    std::jthread* pWorkerThreads = new std::jthread[numWorkers];
    for (uint32_t i = 0; i < numWorkers; i++)
    {
        pWorkerThreads[i] = (std::jthread([&, i]()
            { pWorkers[i].Run(config.stepSizeInPixels); }));
    }

    bool renderDone = false;
    bool renderPaused = false;
    bool renderExit = false;

    auto lbdKeyCb = [&](mfb_window*, mfb_key key, mfb_key_mod, bool isPressed)
    {
        if (isPressed)
        {
            if (key == KB_KEY_SPACE)
                renderPaused = !renderPaused;
            else if (key == KB_KEY_ESCAPE)
                renderExit = true;
        }
    };

    mfb_set_keyboard_callback(lbdKeyCb, pWindow);

    auto start = std::chrono::high_resolution_clock::now();

    do
    {
        if (g_NumFinishedWorkers == numWorkers)
        {
            auto end = std::chrono::high_resolution_clock::now();

            if (!renderDone)
            {
                renderDone = true;
                printf("*** DONE in %.3f sec ***\n", std::chrono::duration<double>(end - start).count());
            }
        }
        else if (!renderPaused)
        {
            // Stall until all workers have progressed
            for (uint32_t i = 0; i < numWorkers; i++)
            {
                pWorkers[i].m_PendingUpdates.wait(false);
            }

            // Screen refresh
            int state = mfb_update(pWindow, &g_pFrameBuffer[0]);
            if (state < 0)
            {
                pWindow = nullptr;
                break;
            }

            // Continue rendering
            for (uint32_t i = 0; i < numWorkers; i++)
            {
                pWorkers[i].m_PendingUpdates = false;
                pWorkers[i].m_PendingUpdates.notify_one();
            }
        }
    } while (mfb_wait_sync(pWindow) && (!renderExit));

    if (!renderDone)
        exit(-1);

    delete[] pWorkerThreads;
    delete[] pWorkers;

    return 0;
}