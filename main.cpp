/*
 * main.cpp
 *
 * Codificação e Decodificação Aritmética de Imagens PGM P2
 * Uso:
 *   ./arith_codec
 *
 * Fluxo para cada imagem:
 *   .pgm → readPGM → pixels → Model::build → arithmeticEncode
 *        → saveCodestream → .bin
 *        → loadCodestream → arithmeticDecode → writePGM → -rec.pgm
 */

#include "pgm.hpp"
#include "bitstream.hpp"
#include "model.hpp"
#include "codec.hpp"
#include "codestream.hpp"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <string>
#include <cstdint>

int main() {

    // Define os três trabalhos: entrada → codestream → reconstrução
    struct Job {
        std::string input;
        std::string codestream;
        std::string output;
    };

    std::vector<Job> jobs = {
        { "lena_ascii.pgm",     "lena_ascii.bin",     "lena_ascii-rec.pgm"     },
        { "baboon_ascii.pgm",   "baboon_ascii.bin",   "baboon_ascii-rec.pgm"   },
        { "quadrado_ascii.pgm", "quadrado_ascii.bin", "quadrado_ascii-rec.pgm" }
    };

    std::cout << std::fixed << std::setprecision(4);

    std::cout << "  Codificacao/Decodificacao Aritmetica de Imagens PGM\n";

    for (const auto& job : jobs) {
        std::cout << "Imagem: " << job.input << "\n";

        // 1. Lê a imagem original
        PGMImage img;
        if (!readPGM(job.input, img)) continue;

        int num_pixels = img.width * img.height;
        std::cout << "  Resolucao : " << img.width << " x " << img.height
                  << "  (" << num_pixels << " pixels)\n";
        std::cout << "  Maxval    : " << img.maxval << "\n";

        // 2. Constrói o modelo de probabilidades
        Model model;
        model.build(img.pixels, img.maxval);

        double entropy = calcEntropy(model);
        std::cout << "  Entropia  : " << entropy << " bits/pixel\n";

        // 3. Codificação aritmética
        std::vector<uint8_t> bitbuf;
        bitbuf.reserve(num_pixels);
        BitWriter bw(bitbuf);
        arithmeticEncode(img.pixels, model, bw);
        bw.flush();
        uint64_t coded_bits = bw.total;

        // 4. Salva a codestream
        if (!saveCodestream(job.codestream, model, bitbuf,
                            img.width, img.height, img.maxval, coded_bits))
            continue;

        // 5. Calcula e exibe métricas
        uint64_t header_bytes    = 20;
        uint64_t model_bytes     = static_cast<uint64_t>(model.num_symbols) * 8;
        uint64_t payload_bytes   = (coded_bits + 7) / 8;
        uint64_t total_bin_bytes = header_bytes + model_bytes + payload_bytes;

        std::ifstream orig_file(job.input, std::ios::binary | std::ios::ate);
        uint64_t orig_bytes = static_cast<uint64_t>(orig_file.tellg());
        orig_file.close();

        double raw_bpp   = 8.0;
        double coded_bpp = static_cast<double>(coded_bits) / num_pixels;
        double cr_vs_raw = raw_bpp / coded_bpp;
        double cr_vs_pgm = static_cast<double>(orig_bytes) / static_cast<double>(total_bin_bytes);

        std::cout << "\n  === CODIFICACAO ===\n";
        std::cout << "  Bits codificados         : " << coded_bits << " bits\n";
        std::cout << "  Bits/pixel codificado    : " << coded_bpp  << " bpp\n";
        std::cout << "  Bits/pixel (binario puro): " << raw_bpp    << " bpp\n";
        std::cout << "  Eficiencia vs entropia   : " << (entropy / coded_bpp) * 100.0 << " %\n";
        std::cout << "\n  === ARQUIVOS ===\n";
        std::cout << "  Arquivo PGM original     : " << orig_bytes      << " bytes\n";
        std::cout << "  Codestream total (.bin)  : " << total_bin_bytes << " bytes\n";
        std::cout << "    - Cabecalho            : " << header_bytes    << " bytes\n";
        std::cout << "    - Modelo (tabela freq) : " << model_bytes     << " bytes\n";
        std::cout << "    - Payload (bits cod.)  : " << payload_bytes   << " bytes\n";
        std::cout << "\n  === TAXAS DE COMPRESSAO ===\n";
        std::cout << "  CR (8bpp binario / codificado)  : " << cr_vs_raw << " : 1\n";
        std::cout << "  CR (PGM ASCII / .bin total)     : " << cr_vs_pgm << " : 1\n";

        // 6. Decodificação
        Model                model2;
        std::vector<uint8_t> bitbuf2;
        int      w2, h2, mv2;
        uint64_t coded_bits2;
        if (!loadCodestream(job.codestream, model2, bitbuf2, w2, h2, mv2, coded_bits2))
            continue;

        BitReader        br(bitbuf2);
        std::vector<int> decoded = arithmeticDecode(model2, br, w2 * h2);

        // 7. Grava imagem reconstruída
        PGMImage rec;
        rec.width    = w2;
        rec.height   = h2;
        rec.maxval   = mv2;
        rec.comments = img.comments;
        rec.pixels   = decoded;
        if (!writePGM(job.output, rec)) continue;

        // 8. Verifica se é lossless
        bool lossless = (decoded == img.pixels);
        std::cout << "\n  Reconstrucao lossless    : "
                  << (lossless ? "SIM " : "NAO ") << "\n";
        std::cout << "  Salvo em                 : " << job.output << "\n";

        if (!lossless) {
            int erros = 0;
            for (int i = 0; i < num_pixels; i++)
                if (decoded[i] != img.pixels[i]) erros++;
            std::cerr << "  [AVISO] " << erros << " pixels diferentes!\n";
        }
    }

    std::cout << "  Processamento concluido.\n";

    return 0;
}
