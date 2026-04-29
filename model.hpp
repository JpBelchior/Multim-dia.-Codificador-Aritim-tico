#pragma once
/*
 * model.hpp
 *
 * Modelo estático de probabilidades para o codec aritmético.
 *
 * Cada nível de cinza (0–255) recebe uma fatia do intervalo [0, total)
 * proporcional à sua frequência na imagem.
 *
 * Exemplo com pixels [0,1,1,2] e maxval=2:
 *   freq  = [2, 3, 2]   (contagem real + 1 de Laplace)
 *   cumul = [0, 2, 5, 7]
 *   total = 7
 *
 *   Régua:  [0──0──2──────1──────5──2──7]
 *            p=2/7      p=3/7      p=2/7
 * Símbolo mais frequente → fatia maior → menos bits → compressão.
 */

#include <cstdint>
#include <cmath>
#include <vector>


// Model — armazena frequências e tabela cumulativa
struct Model {
    int num_symbols = 0;
    std::vector<uint64_t> freq;   // freq[s]  = ocorrências do símbolo s + 1 (Laplace)
    std::vector<uint64_t> cumul;  // cumul[s] = soma de freq[0..s-1]
    uint64_t total = 0;           // soma total de todas as frequências

    // build — constrói o modelo a partir dos pixels da imagem
    //
    // Passos:
    //   1. Cria freq[] com 1 em todos (Laplace: evita probabilidade zero)
    //   2. Incrementa freq[pixel] para cada pixel da imagem
    //   3. Calcula cumul[] (soma acumulada de freq[])
    //
    // Exemplo: pixels=[0,1,1,2], maxval=2
    //   após Laplace: freq=[1,1,1]
    //   após contagem: freq=[2,3,2]
    //   cumul=[0,2,5,7], total=7
    void build(const std::vector<int>& symbols, int maxval) {
        num_symbols = maxval + 1;
        freq.assign(num_symbols, 1);   // Laplace: +1 em todos

        for (int s : symbols)
            freq[s]++;

        cumul.resize(num_symbols + 1);
        cumul[0] = 0;
        for (int i = 0; i < num_symbols; i++)
            cumul[i + 1] = cumul[i] + freq[i];

        total = cumul[num_symbols];
    }
};

// calcEntropy — entropia de Shannon em bits/pixel
//
// H = -Σ p(s) * log2(p(s))
//
// É o limite teórico: nenhum compressor lossless pode usar
// menos bits/pixel do que H em média.
//
// Remove o +1 de Laplace antes de calcular (usa frequências reais).

double calcEntropy(const Model& model) {
    uint64_t n = 0;
    std::vector<uint64_t> raw(model.num_symbols);
    for (int i = 0; i < model.num_symbols; i++) {
        raw[i] = model.freq[i] - 1;   // desfaz Laplace
        n += raw[i];
    }
    if (n == 0) return 0.0;

    double H = 0.0;
    for (int i = 0; i < model.num_symbols; i++) {
        if (raw[i] > 0) {
            double p = static_cast<double>(raw[i]) / static_cast<double>(n);
            H -= p * std::log2(p);
        }
    }
    return H;
}
