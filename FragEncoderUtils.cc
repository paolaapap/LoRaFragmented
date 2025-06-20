#include "FragEncoderUtils.h"

#include <cmath>


namespace fragmentedApp {

//da MATLAB
int32_t prbs23(int32_t start) {
    int32_t x = start;
    int32_t b0 = x & 0x01;
    int32_t b1 = (x & 0x20) >> 5;
    return (x >> 1) + ((b0 ^ b1) << 22);
}

std::vector<uint8_t> matrix_line(int32_t N, int32_t M) {
    std::vector<uint8_t> line(M, 0);

    if (N <= M) {
        if (N > 0) {
            line[N - 1] = 1;
        }
        return line;
    }


    int32_t m;
    if (M > 0 && M == (1 << static_cast<int>(std::floor(std::log2(M))))) {
        m = 1;
    } else {
        m = 0;
    }

    int32_t x = 1 + 1001 * N;
    int32_t nb_coeff = 0;

    while (nb_coeff < static_cast<int>(std::floor(static_cast<double> (M) / 2.0))) {
        int32_t r = 1 << 16;

        while (r>=M){
            x = prbs23(x);
            int32_t r = x % (M + m);
        }

        if (line[r] == 0) {
            line[r] = 1;
            nb_coeff++;
        }
    }

    return line;
}
}
