#ifndef FRAG_ENCODER_UTILS_H
#define FRAG_ENCODER_UTILS_H

#include <stdint.h>
#include <vector>

namespace fragmentedApp {
    int32_t prbs23(int32_t start);
    // N: indice della riga
    // M: numero totale di frammenti non codificati
    std::vector<uint8_t> matrix_line(int32_t N, int32_t M);
}
#endif



