#ifndef NET_MINECRAFT_WORLD_LEVEL__GrassColor_H__
#define NET_MINECRAFT_WORLD_LEVEL__GrassColor_H__

#include <vector>

// Java Beta's grass colour map.  It is deliberately a small header-only
// utility so all desktop targets share the same data path without a new
// platform-specific renderer dependency.
class GrassColor {
public:
    static void init(const unsigned char* rgba, int width, int height) {
        std::vector<int>& pixels = colorMap();
        pixels.clear();
        if (!rgba || width != 256 || height != 256) return;
        pixels.resize(256 * 256);
        for (int i = 0; i < 256 * 256; ++i) {
            pixels[i] = (rgba[i * 4] << 16) | (rgba[i * 4 + 1] << 8) | rgba[i * 4 + 2];
        }
    }

    static int get(float temperature, float rainfall) {
        std::vector<int>& pixels = colorMap();
        if (pixels.empty()) return 0x339933;
        rainfall *= temperature;
        int x = (int)((1.0f - temperature) * 255.0f);
        int y = (int)((1.0f - rainfall) * 255.0f);
        if (x < 0) x = 0; else if (x > 255) x = 255;
        if (y < 0) y = 0; else if (y > 255) y = 255;
        return pixels[(y << 8) | x];
    }

private:
    static std::vector<int>& colorMap() {
        static std::vector<int> pixels;
        return pixels;
    }
};

#endif
