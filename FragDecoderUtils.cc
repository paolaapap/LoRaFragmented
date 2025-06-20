#include "FragDecoderUtils.h"
#include "FragEncoderUtils.h"
#include<algorithm>

namespace fragmentedApp{

uint8_t GetParity(uint16_t index, uint8_t *matrixRow) {
    uint8_t parity;
    parity = matrixRow[index >> 3];
    parity = (parity >> (7 - (index % 8))) & 0x01;
    return parity;
}

void SetParity(uint16_t index, uint8_t *matrixRow, uint8_t parity) {
    uint8_t mask = 0xFF - (1 << (7 - (index % 8)));
    parity = parity << (7 - (index % 8));
    matrixRow[index >> 3] = (matrixRow[index >> 3] & mask) + parity;
}

void XorDataLine(uint8_t *line1, uint8_t *line2, int32_t size) {
    for (int32_t i = 0; i < size; i++) {
        line1[i] = line1[i] ^ line2[i];
    }
}

void XorParityLine(uint8_t *line1, uint8_t *line2, int32_t size) {
    for (int32_t i = 0; i < size; i++) {
        SetParity(i, line1, (GetParity(i, line1) ^ GetParity(i, line2)));
    }
}

void FragGetParityMatrixRow(int32_t n, int32_t m, uint8_t *matrixRow) {
    //l'ho modificata in modo da adattare la matrix_line dell'encoder (MATLAB)
    //con la matrix_line del decoder (codice st)
    std::vector<uint8_t> line = fragmentedApp::matrix_line(n, m);
    std::copy(line.begin(), line.end(), matrixRow);
}

uint16_t BitArrayFindFirstOne(uint8_t *bitArray, uint16_t size) {
    for (uint16_t i = 0; i < size; i++) {
        if (GetParity(i, bitArray) == 1) {
            return i;
        }
    }
    return 0;
}

uint8_t BitArrayIsAllZeros(uint8_t *bitArray, uint16_t size) {
    for (uint16_t i = 0; i < size; i++) {
        if (GetParity(i, bitArray) == 1) {
            return 0;
        }
    }
    return 1;
}
}
