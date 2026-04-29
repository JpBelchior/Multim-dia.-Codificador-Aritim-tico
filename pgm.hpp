#pragma once
/*
 * pgm.hpp
 *
 * Leitura e gravação de imagens PGM P2 (ASCII).
 *
 * Formato do arquivo:
 *   P2              ← número mágico obrigatório
 *   # comentário    ← linhas opcionais com '#'
 *   512 512         ← largura altura
 *   255             ← valor máximo (0=preto, 255=branco)
 *   143 144 148 ... ← pixels linha por linha
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cctype>

// Estrutura que representa uma imagem PGM na memória
struct PGMImage {
    int width  = 0;
    int height = 0;
    int maxval = 0;
    std::vector<std::string> comments;  // preservadas para gravar de volta
    std::vector<int>         pixels;    // acesso: pixels[row * width + col]
};


// readPGM — lê um arquivo .pgm P2 do disco
//
// Exemplo:
//   PGMImage img;
//   readPGM("lena_ascii.pgm", img);
//   // img.pixels agora tem todos os valores de cinza

bool readPGM(const std::string& filename, PGMImage& img) {
    std::ifstream f(filename);
    if (!f.is_open()) {
        std::cerr << "[ERRO] Não foi possível abrir: " << filename << "\n";
        return false;
    }

    // Verifica o número mágico "P2"
    std::string magic;
    f >> magic;
    if (magic != "P2") {
        std::cerr << "[ERRO] Formato inválido (esperado P2): " << filename << "\n";
        return false;
    }

    img.comments.clear();

    // Pula espaços/quebras de linha
    auto skipWS = [&]() {
        while (!f.eof() && std::isspace(static_cast<unsigned char>(f.peek())))
            f.get();
    };

    // Lê comentários (linhas começando com '#')
    skipWS();
    while (!f.eof() && f.peek() == '#') {
        std::string line;
        std::getline(f, line);
        img.comments.push_back(line);
        skipWS();
    }

    // Lê largura e altura: ex "512 512"
    f >> img.width >> img.height;

    skipWS();
    while (!f.eof() && f.peek() == '#') {
        std::string line;
        std::getline(f, line);
        img.comments.push_back(line);
        skipWS();
    }

    // Lê valor máximo: ex "255"
    f >> img.maxval;

    if (img.width <= 0 || img.height <= 0 || img.maxval <= 0 || img.maxval > 65535) {
        std::cerr << "[ERRO] Cabeçalho inválido em: " << filename << "\n";
        return false;
    }

    // Lê todos os pixels num vetor plano
    // Para imagem 3x2: pixels = [l0c0, l0c1, l0c2, l1c0, l1c1, l1c2]
    int total = img.width * img.height;
    img.pixels.resize(total);
    for (int i = 0; i < total; i++) {
        if (!(f >> img.pixels[i])) {
            std::cerr << "[ERRO] Pixels insuficientes em: " << filename << "\n";
            return false;
        }
    }
    return true;
}

// writePGM — grava uma PGMImage no disco como .pgm P2
//
// Exemplo:
//   PGMImage rec;
//   rec.width = 512; rec.height = 512; rec.maxval = 255;
//   rec.pixels = decoded;
//   writePGM("lena_ascii-rec.pgm", rec);
bool writePGM(const std::string& filename, const PGMImage& img) {
    std::ofstream f(filename);
    if (!f.is_open()) {
        std::cerr << "[ERRO] Não foi possível criar: " << filename << "\n";
        return false;
    }

    // Cabeçalho
    f << "P2\n";
    for (const auto& c : img.comments) f << c << "\n";
    f << img.width << " " << img.height << "\n";
    f << img.maxval << "\n";

    // Pixels linha por linha, separados por espaço
    for (int row = 0; row < img.height; row++) {
        for (int col = 0; col < img.width; col++) {
            f << img.pixels[row * img.width + col];
            if (col < img.width - 1) f << " ";
        }
        f << "\n";
    }
    return true;
}
