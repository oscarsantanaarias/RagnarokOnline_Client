#include "ro/resource/inflate.hpp"
#include <cstring>
#include <algorithm>

namespace ro::resource {

namespace {

struct TinfData {
    const uint8_t* source;
    size_t source_len;
    size_t source_pos;
    uint32_t tag;
    int bitcount;

    uint8_t* dest;
    size_t dest_len;
    size_t dest_pos;
};

struct TinfTree {
    uint16_t table[16];
    uint16_t trans[288];
};

static uint32_t get_bits(TinfData* d, int num) {
    uint32_t res = 0;
    for (int i = 0; i < num; ++i) {
        if (d->bitcount == 0) {
            if (d->source_pos < d->source_len) {
                d->tag = d->source[d->source_pos++];
            } else {
                d->tag = 0;
            }
            d->bitcount = 8;
        }
        res |= ((d->tag & 1) << i);
        d->tag >>= 1;
        --d->bitcount;
    }
    return res;
}

static void build_tree(TinfTree* t, const uint8_t* lengths, int num) {
    uint16_t count[16] = {0};
    uint16_t offsets[16] = {0};

    for (int i = 0; i < num; ++i) {
        if (lengths[i]) count[lengths[i]]++;
    }

    t->table[0] = 0;
    for (int i = 1; i <= 15; ++i) {
        t->table[i] = t->table[i - 1] + count[i - 1];
        offsets[i] = t->table[i];
    }

    for (int i = 0; i < num; ++i) {
        if (lengths[i]) {
            t->trans[offsets[lengths[i]]++] = static_cast<uint16_t>(i);
        }
    }
}

static int decode_symbol(TinfData* d, const TinfTree* t) {
    int sum = 0, cur = 0, len = 1;
    do {
        cur = (cur << 1) | get_bits(d, 1);
        sum += (t->table[len] - t->table[len - 1]);
        cur -= (t->table[len] - t->table[len - 1]);
        if (cur < 0) {
            return t->trans[sum + cur];
        }
        len++;
    } while (len <= 15);
    return -1;
}

static const uint16_t length_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};

static const uint8_t length_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};

static const uint16_t dist_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
    8193, 12289, 16385, 24577
};

static const uint8_t dist_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

static const uint8_t clc_order[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

static bool inflate_block(TinfData* d, const TinfTree* lt, const TinfTree* dt) {
    while (true) {
        int sym = decode_symbol(d, lt);
        if (sym < 0) return false;
        if (sym < 256) {
            if (d->dest_pos >= d->dest_len) return false;
            d->dest[d->dest_pos++] = static_cast<uint8_t>(sym);
        } else if (sym == 256) {
            return true;
        } else {
            sym -= 257;
            if (sym >= 29) return false;
            int length = length_base[sym] + get_bits(d, length_extra[sym]);

            int dsym = decode_symbol(d, dt);
            if (dsym < 0 || dsym >= 30) return false;
            int dist = dist_base[dsym] + get_bits(d, dist_extra[dsym]);

            if (static_cast<size_t>(dist) > d->dest_pos) return false;
            if (d->dest_pos + length > d->dest_len) return false;

            for (int i = 0; i < length; ++i) {
                d->dest[d->dest_pos] = d->dest[d->dest_pos - dist];
                d->dest_pos++;
            }
        }
    }
}

} // namespace

bool Inflater::decompress_zlib(std::span<const uint8_t> source, std::vector<uint8_t>& dest) {
    if (source.size() < 6) return false;

    // Check zlib header (RFC 1950)
    const uint8_t cmf = source[0];
    const uint8_t flg = source[1];
    if ((cmf * 256 + flg) % 31 != 0) return false;
    if ((cmf & 0x0F) != 8) return false; // Deflate compression

    TinfData d{};
    d.source = source.data() + 2;
    d.source_len = source.size() - 6; // Exclude 2 bytes header + 4 bytes adler32
    d.source_pos = 0;
    d.tag = 0;
    d.bitcount = 0;

    d.dest = dest.data();
    d.dest_len = dest.size();
    d.dest_pos = 0;

    bool bfinal = false;
    while (!bfinal) {
        bfinal = (get_bits(&d, 1) != 0);
        const int btype = get_bits(&d, 2);

        if (btype == 0) {
            // Stored / uncompressed
            d.bitcount = 0;
            if (d.source_pos + 4 > d.source_len) return false;
            const uint16_t len = d.source[d.source_pos] | (d.source[d.source_pos + 1] << 8);
            d.source_pos += 4;
            if (d.source_pos + len > d.source_len || d.dest_pos + len > d.dest_len) return false;
            std::memcpy(&d.dest[d.dest_pos], &d.source[d.source_pos], len);
            d.source_pos += len;
            d.dest_pos += len;
        } else if (btype == 1) {
            // Fixed Huffman
            uint8_t lengths[288];
            for (int i = 0; i <= 143; ++i) lengths[i] = 8;
            for (int i = 144; i <= 255; ++i) lengths[i] = 9;
            for (int i = 256; i <= 279; ++i) lengths[i] = 7;
            for (int i = 280; i <= 287; ++i) lengths[i] = 8;

            uint8_t dlengths[32];
            for (int i = 0; i < 32; ++i) dlengths[i] = 5;

            TinfTree lt{}, dt{};
            build_tree(&lt, lengths, 288);
            build_tree(&dt, dlengths, 32);

            if (!inflate_block(&d, &lt, &dt)) return false;
        } else if (btype == 2) {
            // Dynamic Huffman
            const int hlit = get_bits(&d, 5) + 257;
            const int hdist = get_bits(&d, 5) + 1;
            const int hclen = get_bits(&d, 4) + 4;

            uint8_t clen_lengths[19] = {0};
            for (int i = 0; i < hclen; ++i) {
                clen_lengths[clc_order[i]] = static_cast<uint8_t>(get_bits(&d, 3));
            }

            TinfTree ct{};
            build_tree(&ct, clen_lengths, 19);

            uint8_t lengths[320] = {0};
            int num = 0;
            const int total = hlit + hdist;
            while (num < total) {
                int sym = decode_symbol(&d, &ct);
                if (sym < 16) {
                    lengths[num++] = static_cast<uint8_t>(sym);
                } else if (sym == 16) {
                    if (num == 0) return false;
                    const uint8_t prev = lengths[num - 1];
                    const int repeat = get_bits(&d, 2) + 3;
                    for (int r = 0; r < repeat && num < total; ++r) lengths[num++] = prev;
                } else if (sym == 17) {
                    const int repeat = get_bits(&d, 3) + 3;
                    for (int r = 0; r < repeat && num < total; ++r) lengths[num++] = 0;
                } else if (sym == 18) {
                    const int repeat = get_bits(&d, 7) + 11;
                    for (int r = 0; r < repeat && num < total; ++r) lengths[num++] = 0;
                } else {
                    return false;
                }
            }

            TinfTree lt{}, dt{};
            build_tree(&lt, lengths, hlit);
            build_tree(&dt, lengths + hlit, hdist);

            if (!inflate_block(&d, &lt, &dt)) return false;
        } else {
            return false;
        }
    }

    dest.resize(d.dest_pos);
    return true;
}

} // namespace ro::resource
