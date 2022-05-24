#pragma once

#include <gli/gli.hpp>
#include <gsl/span>

#include <memory>
#include <optional>
#include <vector>

struct Rect {
    gli::ivec2 origin;
    gli::ivec2 size;
};

struct Image {
    Image(gli::ivec2 extent, int components)
        : extent(extent), components(components), data(extent.x * extent.y * components) {}

    uint8_t *GetPixel(gli::ivec2 pixelCoord) {
        int idx = pixelCoord.x + extent.x * pixelCoord.y;
        return data.data() + components * idx;
    }

    int GetStride() const { return extent.x * components; }

    gli::ivec2 extent{};
    int components{};
    std::vector<uint8_t> data;
};

std::shared_ptr<Image> ConvertCommand(gsl::span<uint8_t const> srcData, std::optional<Rect> crop);