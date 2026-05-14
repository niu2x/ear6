/* blip_buf 1.1.0. http://www.slack.net/~ant/ */

#include "blip_buf.h"
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

#if ULONG_MAX / 0xFFFFFFFF > 0xFFFFFFFF
    typedef unsigned long fixed_t;
    enum { pre_shift = 32 };
#elif defined(ULLONG_MAX)
    typedef unsigned long long fixed_t;
    enum { pre_shift = 32 };
#else
    typedef unsigned fixed_t;
    enum { pre_shift = 0 };
#endif

enum { time_bits = pre_shift + 20 };
static fixed_t const time_unit = (fixed_t)1 << time_bits;

enum { bass_shift = 9 };
enum { end_frame_extra = 2 };
enum { half_width = 8 };
enum { buf_extra = half_width * 2 + end_frame_extra };
enum { phase_bits = 5 };
enum { phase_count = 1 << phase_bits };
enum { delta_bits = 15 };
enum { delta_unit = 1 << delta_bits };
enum { frac_bits = time_bits - pre_shift };

struct blip_t {
    fixed_t factor;
    fixed_t offset;
    int avail;
    int size;
    int integrator;
};

typedef int buf_t;

#define SAMPLES(buf) ((buf_t*)((buf) + 1))
#define ARITH_SHIFT(n, shift) ((n) >> (shift))

enum { max_sample = +32767 };
enum { min_sample = -32768 };

#define CLAMP(n) \
    { if ((short)n != n) n = ARITH_SHIFT(n, 16) ^ max_sample; }

static short const bl_step[phase_count + 1][half_width] = {
    {43, -115, 350, -488, 1136, -914, 5861, 21022},
    {44, -118, 348, -473, 1076, -799, 5274, 21001},
    {45, -121, 344, -454, 1011, -677, 4706, 20936},
    {46, -122, 336, -431, 942, -549, 4156, 20829},
    {47, -123, 327, -404, 868, -418, 3629, 20679},
    {47, -122, 316, -375, 792, -285, 3124, 20488},
    {47, -120, 303, -344, 714, -151, 2644, 20256},
    {46, -117, 289, -310, 634, -17, 2188, 19985},
    {46, -114, 273, -275, 553, 117, 1758, 19675},
    {44, -108, 255, -237, 471, 247, 1356, 19327},
    {43, -103, 237, -199, 390, 373, 981, 18944},
    {42, -98, 218, -160, 310, 495, 633, 18527},
    {40, -91, 198, -121, 231, 611, 314, 18078},
    {38, -84, 178, -81, 153, 722, 22, 17599},
    {36, -76, 157, -43, 80, 824, -241, 17092},
    {34, -68, 135, -3, 8, 919, -476, 16558},
    {32, -61, 115, 34, -60, 1006, -683, 16001},
    {29, -52, 94, 70, -123, 1083, -862, 15422},
    {27, -44, 73, 106, -184, 1152, -1015, 14824},
    {25, -36, 53, 139, -239, 1211, -1142, 14210},
    {22, -27, 34, 170, -290, 1261, -1244, 13582},
    {20, -20, 16, 199, -335, 1301, -1322, 12942},
    {18, -12, -3, 226, -375, 1331, -1376, 12293},
    {15, -4, -19, 250, -410, 1351, -1408, 11638},
    {13, 3, -35, 272, -439, 1361, -1419, 10979},
    {11, 9, -49, 292, -464, 1362, -1410, 10319},
    {9, 16, -63, 309, -483, 1354, -1383, 9660},
    {7, 22, -75, 322, -496, 1337, -1339, 9005},
    {6, 26, -85, 333, -504, 1312, -1280, 8355},
    {4, 31, -94, 341, -507, 1278, -1205, 7713},
    {3, 35, -102, 347, -506, 1238, -1119, 7082},
    {1, 40, -110, 350, -499, 1190, -1021, 6464},
    {0, 43, -115, 350, -488, 1136, -914, 5861}
};

blip_t* blip_new(int size) {
    blip_t* m;
    assert(size >= 0);
    m = (blip_t*)malloc(sizeof * m + (size + buf_extra) * sizeof(buf_t));
    if (m) {
        m->factor = time_unit / blip_max_ratio;
        m->size = size;
        blip_clear(m);
    }
    return m;
}

void blip_delete(blip_t* m) {
    if (m) { memset(m, 0, sizeof * m); free(m); }
}

void blip_set_rates(blip_t* m, double clock_rate, double sample_rate) {
    double factor = time_unit * sample_rate / clock_rate;
    m->factor = (fixed_t)factor;
    assert(0 <= factor - m->factor && factor - m->factor < 1);
    if (m->factor < factor) m->factor++;
}

void blip_clear(blip_t* m) {
    m->offset = m->factor / 2;
    m->avail = 0;
    m->integrator = 0;
    memset(SAMPLES(m), 0, (m->size + buf_extra) * sizeof(buf_t));
}

int blip_clocks_needed(const blip_t* m, int samples) {
    fixed_t needed;
    assert(samples >= 0 && m->avail + samples <= m->size);
    needed = (fixed_t)samples * time_unit;
    if (needed < m->offset) return 0;
    return (int)((needed - m->offset + m->factor - 1) / m->factor);
}

void blip_end_frame(blip_t* m, unsigned t) {
    fixed_t off = t * m->factor + m->offset;
    m->avail += off >> time_bits;
    m->offset = off & (time_unit - 1);
    assert(m->avail <= m->size);
}

int blip_samples_avail(const blip_t* m) { return m->avail; }

static void remove_samples(blip_t* m, int count) {
    buf_t* buf = SAMPLES(m);
    int remain = m->avail + buf_extra - count;
    m->avail -= count;
    memmove(&buf[0], &buf[count], remain * sizeof buf[0]);
    memset(&buf[remain], 0, count * sizeof buf[0]);
}

int blip_read_samples(blip_t* m, short out[], int count, int stereo) {
    assert(count >= 0);
    if (count > m->avail) count = m->avail;
    if (count) {
        int const step = stereo ? 2 : 1;
        buf_t const* in = SAMPLES(m);
        buf_t const* end = in + count;
        int sum = m->integrator;
        do {
            int s = ARITH_SHIFT(sum, delta_bits);
            sum += *in++;
            CLAMP(s);
            *out = s;
            out += step;
            sum -= s << (delta_bits - bass_shift);
        } while (in != end);
        m->integrator = sum;
        remove_samples(m, count);
    }
    return count;
}

void blip_add_delta(blip_t* m, unsigned time, int delta) {
    unsigned fixed = (unsigned)((time * m->factor + m->offset) >> pre_shift);
    buf_t* out = SAMPLES(m) + m->avail + (fixed >> frac_bits);
    int const phase_shift = frac_bits - phase_bits;
    int phase = fixed >> phase_shift & (phase_count - 1);
    short const* in = bl_step[phase];
    short const* rev = bl_step[phase_count - phase];
    int interp = fixed >> (phase_shift - delta_bits) & (delta_unit - 1);
    int delta2 = (delta * interp) >> delta_bits;
    delta -= delta2;
    assert(out <= &SAMPLES(m)[m->size + end_frame_extra]);
    out[0] += in[0] * delta + in[half_width + 0] * delta2;
    out[1] += in[1] * delta + in[half_width + 1] * delta2;
    out[2] += in[2] * delta + in[half_width + 2] * delta2;
    out[3] += in[3] * delta + in[half_width + 3] * delta2;
    out[4] += in[4] * delta + in[half_width + 4] * delta2;
    out[5] += in[5] * delta + in[half_width + 5] * delta2;
    out[6] += in[6] * delta + in[half_width + 6] * delta2;
    out[7] += in[7] * delta + in[half_width + 7] * delta2;
    in = rev;
    out[8] += in[7] * delta + in[7 - half_width] * delta2;
    out[9] += in[6] * delta + in[6 - half_width] * delta2;
    out[10] += in[5] * delta + in[5 - half_width] * delta2;
    out[11] += in[4] * delta + in[4 - half_width] * delta2;
    out[12] += in[3] * delta + in[3 - half_width] * delta2;
    out[13] += in[2] * delta + in[2 - half_width] * delta2;
    out[14] += in[1] * delta + in[1 - half_width] * delta2;
    out[15] += in[0] * delta + in[0 - half_width] * delta2;
}

void blip_add_delta_fast(blip_t* m, unsigned time, int delta) {
    unsigned fixed = (unsigned)((time * m->factor + m->offset) >> pre_shift);
    buf_t* out = SAMPLES(m) + m->avail + (fixed >> frac_bits);
    int interp = fixed >> (frac_bits - delta_bits) & (delta_unit - 1);
    int delta2 = delta * interp;
    assert(out <= &SAMPLES(m)[m->size + end_frame_extra]);
    out[7] += delta * delta_unit - delta2;
    out[8] += delta2;
}
