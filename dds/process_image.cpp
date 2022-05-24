#include "process_image.hpp"

#include "cmp_core.h"
#include "gli_format_names.h"

#include <fmt/core.h>
#include <gli/gli.hpp>
#include <gli/load.hpp>

struct ImageRef {
    ImageRef(gli::ivec2 extent, int comp, gsl::span<uint8_t const> data)
        : extent(extent), components(comp), data(data) {}

    uint8_t const *GetPixel(gli::ivec2 pixelCoord) const {
        int idx = pixelCoord.x + extent.x * pixelCoord.y;
        return data.data() + components * idx;
    }

    gli::ivec2 extent{};
    int components{};
    gsl::span<uint8_t const> data;
};

std::shared_ptr<Image> ConvertCommand(gsl::span<uint8_t const> srcData, std::optional<Rect> cropOpt) {
    auto tex = gli::load(reinterpret_cast<char const*>(srcData.data()), srcData.size());
    if (tex.empty()) {
        throw std::runtime_error(fmt::format("could not load texture"));
    }

    gli::texture2d srcTex{tex};
    if (srcTex.empty()) {
        throw std::runtime_error(fmt::format("could not load texture"));
    }

    auto fmt = srcTex.format();
    auto extent = srcTex.extent(srcTex.base_level());

    if (gli::is_float(fmt)) {
        throw std::runtime_error(fmt::format("floating point textures unsupported"));
    }

    if (srcTex.layers() > 1 || srcTex.faces() > 1) {
        throw std::runtime_error(fmt::format("non-2D images unsupported"));
    }

    Rect crop;
    if (cropOpt) {
        crop = *cropOpt;
    } else {
        crop.origin = glm::ivec2(0, 0);
        crop.size = extent;
    }

    if (crop.origin.x < 0 || crop.origin.y < 0) {
        throw std::runtime_error(
            fmt::format("crop specification out of bounds: x={}, y={}, width={}, height={}", crop.origin.x,
                        crop.origin.y, crop.size.x, crop.size.y));
    }

    if (glm::ivec2 end = crop.origin + crop.size; end.x > extent.x || end.y > extent.y) {
        throw std::runtime_error(
            fmt::format("crop specification exceeds image size: x={}, y={}, width={}, height={}", crop.origin.x,
                        crop.origin.y, crop.size.x, crop.size.y));
    }

    gli::texture2d dstTex;
    std::optional<Image> dstImg;

    if (gli::is_compressed(fmt)) {
        gli::format midFormat;
        std::function<void(uint8_t const *, uint8_t *, void *)> decompressBlockFunc;

        switch (fmt) {
        case gli::FORMAT_RGBA_BP_UNORM_BLOCK16: {
            midFormat = gli::FORMAT_RGBA8_UNORM_PACK8;
            decompressBlockFunc = DecompressBlockBC7;
        } break;
        case gli::FORMAT_RGBA_DXT1_UNORM_BLOCK8: {
            midFormat = gli::FORMAT_RGBA8_UNORM_PACK8;
            decompressBlockFunc = DecompressBlockBC1;
        } break;
        case gli::FORMAT_RGBA_DXT3_UNORM_BLOCK16: {
            midFormat = gli::FORMAT_RGBA8_UNORM_PACK8;
            decompressBlockFunc = DecompressBlockBC2;
        } break;
        case gli::FORMAT_RGBA_DXT5_SRGB_BLOCK16: {
            midFormat = gli::FORMAT_RGBA8_UNORM_PACK8;
            decompressBlockFunc = DecompressBlockBC3;
        } break;
        default:
            throw std::runtime_error(fmt::format("unhandled format {} ({})", GliFormatName(fmt), fmt));
        }

        auto srcSpan = gsl::make_span(srcTex.data<uint8_t>(0, 0, 0), srcTex.size(0));

        auto blockSize = gli::block_size(fmt);
        auto blockExtent = glm::ivec2(gli::block_extent(fmt));
        auto blockCount = (extent + glm::ivec2(blockExtent - 1)) / glm::ivec2(blockExtent);
        auto components = gli::component_count(fmt);

        glm::ivec2 firstBlock(crop.origin / blockExtent);
        glm::ivec2 lastBlock((crop.origin + crop.size + blockExtent - 1) / blockExtent);

        Image workImg({4, 4}, 4);
        dstImg = Image(crop.size, 4);

        // Decode all the blocks that cover the desired pixel region
        for (int blockY = firstBlock.y; blockY < lastBlock.y; ++blockY) {
            for (int blockX = firstBlock.x; blockX < lastBlock.x; ++blockX) {
                auto blockIdx = blockX + blockY * blockCount.x;
                auto inSpan = srcSpan.subspan(blockIdx * blockSize, blockSize);
                decompressBlockFunc(inSpan.data(), workImg.GetPixel({0, 0}), nullptr);
                glm::ivec2 blockBase = glm::ivec2(blockX, blockY) * blockExtent;

                auto blockRel = blockBase - crop.origin;
                for (int midY = 0; midY < 4; ++midY) {
                    auto pixRelY = blockRel.y + midY;
                    if (pixRelY >= 0 && pixRelY < crop.size.y) {
                        for (int midX = 0; midX < 4; ++midX) {
                            auto pixRelX = blockRel.x + midX;
                            if (pixRelX >= 0 && pixRelX < crop.size.x) {
                                memcpy(dstImg->GetPixel({pixRelX, pixRelY}), workImg.GetPixel({midX, midY}),
                                       workImg.components);
                            }
                        }
                    }
                }
            }
        }
    } else {
        enum class MapTo {
            Red = 0,
            Green = 1,
            Blue = 2,
            Alpha = 3,
            One,
            Zero,
        };
        glm::vec<4, MapTo> compRemap{MapTo::Red, MapTo::Green, MapTo::Blue, MapTo::Alpha};
        std::optional<ImageRef> srcImg;
        glm::ivec2 srcExtent = srcTex.extent();
        auto srcSpan = gsl::make_span(srcTex.data<uint8_t>(0, 0, 0), srcTex.size(0));
        switch (fmt) {
        case gli::FORMAT_BGR8_UNORM_PACK32: {
            compRemap = {MapTo::Blue, MapTo::Green, MapTo::Red, MapTo::One};
            srcImg = ImageRef(srcExtent, 4, srcSpan);
            dstImg = Image(crop.size, 3);
        } break;
        case gli::FORMAT_BGRA8_UNORM_PACK8: {
            compRemap = {MapTo::Blue, MapTo::Green, MapTo::Red, MapTo::Alpha};
            srcImg = ImageRef(srcExtent, 4, srcSpan);
            dstImg = Image(crop.size, 4);
        } break;
        // case gli::FORMAT_R16_SFLOAT_PACK16: {} break;
        // case gli::FORMAT_R32_SFLOAT_PACK32: {} break;
        // case gli::FORMAT_RG16_SFLOAT_PACK16: {} break;
        case gli::FORMAT_RG8_UNORM_PACK8: {
            compRemap = {MapTo::Red, MapTo::Green, MapTo::Zero, MapTo::One};
            srcImg = ImageRef(srcExtent, 2, srcSpan);
            dstImg = Image(crop.size, 3);
        } break;
        // case gli::FORMAT_RGBA32_SFLOAT_PACK32: {} break;
        case gli::FORMAT_RGBA8_SRGB_PACK8:
        case gli::FORMAT_RGBA8_UNORM_PACK8: {
            srcImg = ImageRef(srcExtent, 4, srcSpan);
            dstImg = Image(crop.size, 4);
        } break;
        default:
            throw std::runtime_error(fmt::format("unhandled format {}", fmt));
        }
        for (int row = 0; row < srcImg->extent.y; ++row) {
            for (int col = 0; col < srcImg->extent.x; ++col) {
                glm::ivec2 dstCoord{col, row};
                auto *srcPixel = srcImg->GetPixel(crop.origin + dstCoord);
                auto *dstPixel = dstImg->GetPixel(dstCoord);
                for (int comp = 0; comp < dstImg->components; ++comp) {
                    uint8_t val{};
                    auto remap = compRemap[comp];
                    switch (remap) {
                    case MapTo::Red:
                    case MapTo::Blue:
                    case MapTo::Green:
                    case MapTo::Alpha:
                        val = srcPixel[(int)remap];
                        break;
                    case MapTo::One:
                        val = 0xFF;
                        break;
                    case MapTo::Zero:
                        val = 0x00;
                        break;
                    }
                    dstPixel[comp] = val;
                }
            }
        }
    }

    // At this point, we have R 8, RG 8.8, RGB 8.8.8 or RGBA 8.8.8.8 unsigned integer texture data
    return std::make_shared<Image>(std::move(*dstImg));
}
