#include "FragEncoder.h"
#include <algorithm>
#include <numeric>

#include "FragEncoderUtils.h"

namespace fragmentedApp{
std::vector<std::vector<uint8_t>> generateCodedFragments(
            const std::vector<uint8_t>& originalFileBuffer,
            uint16_t numFragments,
            uint8_t fragmentSize,
            uint16_t numCodedFragmentsToGenerate
    ){
    uint16_t w = numFragments;
    uint8_t currentFragmentSize = fragmentSize;

    if(originalFileBuffer.size() != (size_t)w * currentFragmentSize){
        //EV_ERROR <<"Original file buffer size mismatch!";
        return {}; //ritorna un vett vuoto
    }

    std::vector<std::vector<uint8_t>> UNCODED_F(w, std::vector<uint8_t>(currentFragmentSize, 0));

    for(uint16_t k=0; k<w; k++){
        std::copy(originalFileBuffer.begin() + k*currentFragmentSize,
                 originalFileBuffer.begin() + (k+1) * currentFragmentSize,
                 UNCODED_F[k].begin());

    }

    std::vector<std::vector<uint8_t>> CODED_F;
    CODED_F.reserve(numCodedFragmentsToGenerate); //preallocazione

    for(uint16_t y=1; y<= numCodedFragmentsToGenerate; y++){
        std::vector<uint8_t> s(currentFragmentSize, 0);
        std::vector<uint8_t> A = fragmentedApp::matrix_line(y,w);
        for(uint16_t x=0; x<w; x++){
            if(A[x]==1){
                for(uint8_t i=0; i<currentFragmentSize; i++){
                    s[i] = s[i] ^ UNCODED_F[x][i];
                }
            }
        }
        CODED_F.push_back(s); //aggiunge s a CODED_F
    }
    return CODED_F;

}
}
