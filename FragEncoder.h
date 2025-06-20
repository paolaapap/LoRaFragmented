#define FRAG_CODEC_UTILS_H

#include <stdint.h>
#include <vector>

namespace fragmentedApp{
    std::vector<std::vector<uint8_t>> generateCodedFragments(
            const std::vector<uint8_t>& originalFileBuffer,
            uint16_t numFragments,
            uint8_t fragmentSize,
            uint16_t numCodedFragmentsToGenerate
    );
}
