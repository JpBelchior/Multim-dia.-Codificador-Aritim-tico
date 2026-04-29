#pragma once
/*
 * bitstream.hpp
 *
 * Empacota e desempacota bits individuais em bytes.
 *
 * O codec aritmético emite/consome um bit por vez.
 * Como arquivos trabalham com bytes, precisamos dessa ponte.
 *
 * BitWriter:  bits soltos → bytes empacotados → buffer
 * BitReader:  buffer → bytes → bits soltos
 *
 * Exemplo:
 *   bits emitidos:  0 1 1 0 1 0 0 1
 *   byte gerado:    0x69  (01101001)
 *   bits lidos:     0 1 1 0 1 0 0 1  ← idênticos
 */

#include <cstdint>
#include <vector>

// BitWriter — empacota bits em bytes
class BitWriter {
public:
    std::vector<uint8_t>& buf;
    uint8_t  byte_buf  = 0;
    int      bit_count = 0;
    uint64_t total     = 0;   // total de bits escritos (para taxa de compressão)

    explicit BitWriter(std::vector<uint8_t>& b) : buf(b) {}

    // Encaixa um bit no byte atual; ao completar 8 bits, salva no buffer
    // Ex: writeBit(0), writeBit(1), writeBit(1)... até 8 → push byte
    void writeBit(int bit) {
        byte_buf = static_cast<uint8_t>((byte_buf << 1) | (bit & 1));
        bit_count++;
        total++;
        if (bit_count == 8) {
            buf.push_back(byte_buf);
            byte_buf  = 0;
            bit_count = 0;
        }
    }

    // Finaliza o último byte parcial com zeros à direita
    // Deve ser chamado UMA VEZ ao final da codificação
    // Ex: restaram bits "1 0 1" → salva "1010 0000" (0xA0)
    void flush() {
        if (bit_count > 0) {
            byte_buf = static_cast<uint8_t>(byte_buf << (8 - bit_count));
            buf.push_back(byte_buf);
            byte_buf  = 0;
            bit_count = 0;
        }
    }
};


// BitReader — desempacota bytes em bits
class BitReader {
public:
    const std::vector<uint8_t>& buf;
    size_t  pos       = 0;
    uint8_t byte_buf  = 0;
    int     bit_count = 0;

    explicit BitReader(const std::vector<uint8_t>& b) : buf(b) {}

    // Retorna o próximo bit do buffer (sempre o mais significativo)
    // Quando o buffer acaba, retorna 0 silenciosamente (padding do encoder)
    // Ex: byte 0x69 = 01101001 → readBit() devolve 0,1,1,0,1,0,0,1
    int readBit() {
        if (bit_count == 0) {
            byte_buf  = (pos < buf.size()) ? buf[pos++] : 0;
            bit_count = 8;
        }
        int bit = (byte_buf >> 7) & 1;
        byte_buf = static_cast<uint8_t>(byte_buf << 1);
        bit_count--;
        return bit;
    }
};
