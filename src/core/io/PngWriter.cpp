#include "PngWriter.h"

#include <cstdio>

#include <png.h>

namespace apo {

bool writePng(const std::string& path, int width, int height, int channels, const std::uint8_t* pixels) {
    if (width <= 0 || height <= 0 || pixels == nullptr) return false;
    if (channels != 3 && channels != 4) return false;

    // Write to a temporary path first, then rename into place - so a
    // failure or crash partway through never leaves a truncated/corrupt
    // file sitting at the caller's requested path (which a naive direct
    // write would: libpng writes incrementally as rows are compressed).
    const std::string tempPath = path + ".tmp";

    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    image.width = static_cast<png_uint_32>(width);
    image.height = static_cast<png_uint_32>(height);
    image.format = (channels == 4) ? PNG_FORMAT_RGBA : PNG_FORMAT_RGB;

    // Row stride is width*channels bytes (0 tells libpng to compute this
    // itself for a tightly-packed buffer, which is exactly what
    // Renderer::render() produces).
    const int ok = png_image_write_to_file(&image, tempPath.c_str(), /*convert_to_8bit=*/0, pixels,
                                            /*row_stride=*/0, /*colormap=*/nullptr);
    if (!ok) {
        std::remove(tempPath.c_str());
        return false;
    }

    if (std::rename(tempPath.c_str(), path.c_str()) != 0) {
        // rename() fails on Windows if `path` already exists - fall back
        // to remove-then-rename rather than leaving the freshly-written
        // image stranded at tempPath.
        std::remove(path.c_str());
        if (std::rename(tempPath.c_str(), path.c_str()) != 0) {
            std::remove(tempPath.c_str());
            return false;
        }
    }

    return true;
}

bool readPng(const std::string& path, int& outWidth, int& outHeight, std::vector<std::uint8_t>& outPixelsRgba) {
    png_image image{};
    image.version = PNG_IMAGE_VERSION;

    if (!png_image_begin_read_from_file(&image, path.c_str())) return false;

    image.format = PNG_FORMAT_RGBA;
    std::vector<std::uint8_t> buffer(PNG_IMAGE_SIZE(image));

    if (!png_image_finish_read(&image, /*background=*/nullptr, buffer.data(), /*row_stride=*/0,
                                /*colormap=*/nullptr)) {
        png_image_free(&image);
        return false;
    }

    outWidth = static_cast<int>(image.width);
    outHeight = static_cast<int>(image.height);
    outPixelsRgba = std::move(buffer);
    return true;
}

} // namespace apo
