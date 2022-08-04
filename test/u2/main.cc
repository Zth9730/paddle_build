#include "decoder/pd_asr_model.h"
#include <algorithm>

int main(void){
    ppspeech::PaddleAsrModel model;
    model.Read("chunk_wenetspeech_static/export.jit");


    // auto encoder_out = paddle::full({1, 20, 512}, 1, paddle::DataType::FLOAT32, phi::CPUPlace());

    std::vector<std::vector<float>> chunk_feats; // [T,D=80]
    std::vector<std::vector<float>> out_prob;

    int T = 7;
    int D = 80;
    chunk_feats.resize(T);
    for (int i = 0; i < T; ++i){
        chunk_feats[i].resize(D);
        std::fill(chunk_feats[i].begin(), chunk_feats[i].end(), 0.1);
    }
    
    model.ForwardEncoderChunkImpl(chunk_feats, &out_prob);
    std::cout << "T: " << out_prob.size() << std::endl;
    std::cout << "D: " << out_prob[0].size() << std::endl;

    for (int i = 0; i < out_prob[0].size(); i++){
        std::cout << out_prob[0][i] << " ";
        if ((i+1)% 10 == 0){
            std::cout << std::endl;
        }
    } 
    std::cout << std::endl;

    // std::vector<float> scores;
    // std::vector<std::vector<int>> hyps(10, std::vector<int>(8, 10));
    // model.AttentionRescoring(hyps, 0, &scores);
    // std::cout << scores[0] << std::endl;

    return 0;
}