#pragma once
/*
 * codec.hpp
 *
 * Codificação e Decodificação Aritmética (inteiros de 32 bits).
 *
 * Ideia central:
 *   O intervalo [0, 1) é subdividido proporcionalmente às probabilidades.
 *   A cada símbolo, entramos no sub-intervalo correspondente.
 *   O número final dentro desse intervalo representa toda a mensagem.
 *
 * Implementação usa inteiros de 32 bits (sem ponto flutuante) e
 * reescala progressiva para manter precisão sem overflow.
 *
 * Marcos do intervalo de 32 bits:
 *   0x00000000  ← low inicial
 *   0x40000000  ← FIRST_QTR  (1/4)
 *   0x80000000  ← HALF       (1/2)
 *   0xC0000000  ← THIRD_QTR  (3/4)
 *   0xFFFFFFFF  ← TOP_VALUE / high inicial
 */

#include "model.hpp"
#include "bitstream.hpp"
#include <cstdint>
#include <vector>

static const uint32_t CODE_BITS  = 32;
static const uint32_t TOP_VALUE  = UINT32_MAX;
static const uint32_t FIRST_QTR  = (TOP_VALUE / 4) + 1;  // 0x40000000
static const uint32_t HALF       = 2 * FIRST_QTR;         // 0x80000000
static const uint32_t THIRD_QTR  = 3 * FIRST_QTR;         // 0xC0000000

// arithmeticEncode — comprime os pixels e escreve bits via BitWriter
//
// Para cada pixel:
//   1. Calcula o sub-intervalo proporcional à probabilidade do símbolo
//   2. Reescala emitindo bits quando o intervalo cai numa metade
//   3. Trata underflow quando o intervalo cruza o centro sem decidir
//
// Exemplo de uso:
//   std::vector<uint8_t> bitbuf;
//   BitWriter bw(bitbuf);
//   arithmeticEncode(img.pixels, model, bw);
//   bw.flush();
void arithmeticEncode(const std::vector<int>& symbols,
                      const Model&            model,
                      BitWriter&              bw)
{
    uint32_t low       = 0;
    uint32_t high      = TOP_VALUE;
    int      underflow = 0;

    for (int sym : symbols) {
        // Subdivide o intervalo atual proporcionalmente ao símbolo
        // Multiplicações em uint64_t para evitar overflow de 32 bits
        uint64_t range    = static_cast<uint64_t>(high - low) + 1;
        uint32_t new_high = static_cast<uint32_t>(low + (range * model.cumul[sym + 1]) / model.total - 1);
        uint32_t new_low  = static_cast<uint32_t>(low + (range * model.cumul[sym])     / model.total);
        high = new_high;
        low  = new_low;

        // Reescala e emite bits
        // Caso A: intervalo inteiro abaixo de HALF → emite '0'
        // Caso B: intervalo inteiro acima de HALF  → emite '1'
        // Caso C: intervalo cruza o centro (underflow) → adia emissão
        while (true) {
            if (high < HALF) {
                bw.writeBit(0);
                for (; underflow > 0; underflow--) bw.writeBit(1);
            } else if (low >= HALF) {
                bw.writeBit(1);
                for (; underflow > 0; underflow--) bw.writeBit(0);
                low  -= HALF;
                high -= HALF;
            } else if (low >= FIRST_QTR && high < THIRD_QTR) {
                underflow++;
                low  -= FIRST_QTR;
                high -= FIRST_QTR;
            } else {
                break;
            }
            low  <<= 1;
            high  = (high << 1) | 1;
        }
    }

    // Finalização: emite bits suficientes para identificar o intervalo
    underflow++;
    if (low < FIRST_QTR) {
        bw.writeBit(0);
        for (; underflow > 0; underflow--) bw.writeBit(1);
    } else {
        bw.writeBit(1);
        for (; underflow > 0; underflow--) bw.writeBit(0);
    }
    // Padding: 32 zeros garantem que o decoder inicialize corretamente
    for (int i = 0; i < static_cast<int>(CODE_BITS); i++)
        bw.writeBit(0);
}

// arithmeticDecode — recupera os pixels originais do stream comprimido
//
// Mantém um registrador 'value' de 32 bits com os bits do stream.
// Para cada pixel, busca binária em cumul[] identifica o símbolo,
// atualiza o intervalo (idêntico ao encoder) e lê novos bits.
//
// Exemplo de uso:
//   BitReader br(bitbuf);
//   auto decoded = arithmeticDecode(model, br, width * height);
//   decoded == img.pixels (lossless)
std::vector<int> arithmeticDecode(const Model& model,
                                  BitReader&   br,
                                  int          num_pixels)
{
    uint32_t low   = 0;
    uint32_t high  = TOP_VALUE;
    uint32_t value = 0;

    // Inicializa 'value' com os primeiros 32 bits do stream
    for (int i = 0; i < static_cast<int>(CODE_BITS); i++)
        value = (value << 1) | static_cast<uint32_t>(br.readBit());

    std::vector<int> result;
    result.reserve(num_pixels);

    for (int i = 0; i < num_pixels; i++) {
        // Busca binária em cumul[] para identificar o símbolo
        // Usa multiplicação inteira (sem divisão) para evitar erros de arredondamento
        uint64_t range = static_cast<uint64_t>(high - low) + 1;
        uint64_t d     = static_cast<uint64_t>(value - low);
        uint64_t td1   = (d + 1) * model.total;

        int lo_s = 0, hi_s = model.num_symbols - 1, sym = 0;
        while (lo_s <= hi_s) {
            int      mid    = (lo_s + hi_s) / 2;
            uint64_t c_cur  = model.cumul[mid]     * range;
            uint64_t c_next = model.cumul[mid + 1] * range;
            if      (c_next < td1) { lo_s = mid + 1; }
            else if (c_cur >= td1) { hi_s = mid - 1; }
            else                   { sym  = mid; break; }
        }
        result.push_back(sym);

        // Atualiza intervalo (idêntico ao encoder)
        uint32_t new_high = static_cast<uint32_t>(low + (range * model.cumul[sym + 1]) / model.total - 1);
        uint32_t new_low  = static_cast<uint32_t>(low + (range * model.cumul[sym])     / model.total);
        high = new_high;
        low  = new_low;

        // Reescala lendo novos bits para repor 'value'
        while (true) {
            if      (high < HALF)                              { /* nada */ }
            else if (low >= HALF)                              { low -= HALF; high -= HALF; value -= HALF; }
            else if (low >= FIRST_QTR && high < THIRD_QTR)    { low -= FIRST_QTR; high -= FIRST_QTR; value -= FIRST_QTR; }
            else break;
            low   <<= 1;
            high   = (high << 1) | 1;
            value  = (value << 1) | static_cast<uint32_t>(br.readBit());
        }
    }
    return result;
}
