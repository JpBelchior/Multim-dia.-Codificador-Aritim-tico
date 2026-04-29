#pragma once
/*
 * codestream.hpp
 *
 * Salva e carrega a codestream comprimida em arquivo binário (.bin).
 *
 * Formato do arquivo .bin:
 *   [ 4 bytes] largura  (int32)
 *   [ 4 bytes] altura   (int32)
 *   [ 4 bytes] maxval   (int32)
 *   [ 8 bytes] número de bits codificados (uint64)
 *   [N×8 bytes] tabela de frequências do modelo (N = maxval+1)
 *   [resto]    bytes comprimidos empacotados
 *
 * O modelo é gravado para que o decoder reconstrua as probabilidades
 * sem precisar da imagem original.
 */

#include "model.hpp"
#include <iostream>
#include <fstream>
#include <iterator>
#include <cstdint>
#include <string>
#include <vector>

// ---------------------------------------------------------------
// saveCodestream — grava modelo + bits comprimidos em arquivo .bin
//
// Exemplo:
//   saveCodestream("lena_ascii.bin", model, bitbuf,
//                  img.width, img.height, img.maxval, bw.total);
// ---------------------------------------------------------------
bool saveCodestream(const std::string&          filename,
                    const Model&                model,
                    const std::vector<uint8_t>& bits,
                    int      width,
                    int      height,
                    int      maxval,
                    uint64_t num_coded_bits)
{
    std::ofstream f(filename, std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "[ERRO] Não foi possível criar: " << filename << "\n";
        return false;
    }

    // Cabeçalho fixo (20 bytes)
    int32_t w32 = width, h32 = height, mv32 = maxval;
    f.write(reinterpret_cast<const char*>(&w32),            4);
    f.write(reinterpret_cast<const char*>(&h32),            4);
    f.write(reinterpret_cast<const char*>(&mv32),           4);
    f.write(reinterpret_cast<const char*>(&num_coded_bits), 8);

    // Tabela de frequências (num_symbols × 8 bytes)
    // O decoder usa isso para reconstruir o modelo identicamente
    for (int i = 0; i < model.num_symbols; i++) {
        uint64_t fq = model.freq[i];
        f.write(reinterpret_cast<const char*>(&fq), 8);
    }

    // Payload: bits comprimidos já empacotados em bytes pelo BitWriter
    f.write(reinterpret_cast<const char*>(bits.data()), bits.size());
    return true;
}

// ---------------------------------------------------------------
// loadCodestream — lê modelo + bits comprimidos de um arquivo .bin
//
// Exemplo:
//   Model model2; std::vector<uint8_t> bits2;
//   int w, h, mv; uint64_t nb;
//   loadCodestream("lena_ascii.bin", model2, bits2, w, h, mv, nb);
// ---------------------------------------------------------------
bool loadCodestream(const std::string&    filename,
                    Model&                model,
                    std::vector<uint8_t>& bits,
                    int&      width,
                    int&      height,
                    int&      maxval,
                    uint64_t& num_coded_bits)
{
    std::ifstream f(filename, std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "[ERRO] Não foi possível abrir: " << filename << "\n";
        return false;
    }

    // Lê cabeçalho
    int32_t w32, h32, mv32;
    f.read(reinterpret_cast<char*>(&w32),            4);
    f.read(reinterpret_cast<char*>(&h32),            4);
    f.read(reinterpret_cast<char*>(&mv32),           4);
    f.read(reinterpret_cast<char*>(&num_coded_bits), 8);
    width  = w32;
    height = h32;
    maxval = mv32;

    // Reconstrói o modelo a partir das frequências gravadas
    model.num_symbols = maxval + 1;
    model.freq.resize(model.num_symbols);
    model.cumul.resize(model.num_symbols + 1);
    for (int i = 0; i < model.num_symbols; i++)
        f.read(reinterpret_cast<char*>(&model.freq[i]), 8);

    model.cumul[0] = 0;
    for (int i = 0; i < model.num_symbols; i++)
        model.cumul[i + 1] = model.cumul[i] + model.freq[i];
    model.total = model.cumul[model.num_symbols];

    // Lê todos os bytes restantes (payload comprimido)
    bits.assign(std::istreambuf_iterator<char>(f),
                std::istreambuf_iterator<char>());
    return true;
}
