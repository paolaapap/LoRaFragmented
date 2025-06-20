#define FRAG_DECODER_UTILS_H
#include <stdint.h>
#include<stdbool.h>

namespace fragmentedApp{

uint8_t GetParity(uint16_t index, uint8_t *matrixRow);
void SetParity(uint16_t index, uint8_t *matrixRow, uint8_t parity);
void XorDataLine(uint8_t *line1, uint8_t *line2, uint32_t size);
void XorParityLine(uint8_t *line1, uint8_t *line2, int32_t size);
void FragGetParityMatrixRow(int32_t n, int32_t m, uint8_t *matrixRow);
uint16_t BitArrayFindFirstOne(uint8_t *birArray, uint16_t size);
uint8_t BitArrayIsAllZeros(uint8_t *bitArray, uint16_t size);
}
