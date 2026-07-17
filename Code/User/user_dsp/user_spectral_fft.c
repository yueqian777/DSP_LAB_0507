/**
 * user_spectral_fft.c
 *
 * Caller-owned buffers and twiddles keep this kernel independent of all
 * WOLA, equalizer, and analyzer state.
 */

#include "user_spectral_fft.h"
#include "math.h"

#define SPECTRAL_FFT_PI 3.14159265358979323846f

static int SpectralFFT_SizeValid(int fft_size)
{
    unsigned int value;

    if (fft_size < 2)
    {
        return 0;
    }
    value = (unsigned int)fft_size;
    return ((value & (value - 1U)) == 0U) ? 1 : 0;
}

static void SpectralFFT_BitReverse(float *real, float *imag, int fft_size)
{
    unsigned int i;
    unsigned int j;

    j = 0U;
    for (i = 1U; i < (unsigned int)fft_size; i++)
    {
        unsigned int bit;

        bit = (unsigned int)fft_size >> 1;
        while ((j & bit) != 0U)
        {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;
        if (i < j)
        {
            float temporary_real;
            float temporary_imag;

            temporary_real = real[i];
            temporary_imag = imag[i];
            real[i] = real[j];
            imag[i] = imag[j];
            real[j] = temporary_real;
            imag[j] = temporary_imag;
        }
    }
}

int SpectralFFT_GenerateTwiddles(float *twiddle_re,
                                 float *twiddle_im,
                                 int fft_size)
{
    int index;
    float phase;

    if ((twiddle_re == 0) || (twiddle_im == 0) ||
        (SpectralFFT_SizeValid(fft_size) == 0))
    {
        return 0;
    }
    for (index = 0; index < fft_size / 2; index++)
    {
        phase = (-2.0f * SPECTRAL_FFT_PI * (float)index) /
                (float)fft_size;
        twiddle_re[index] = cosf(phase);
        twiddle_im[index] = sinf(phase);
    }
    return 1;
}

int SpectralFFT_Forward(float *real,
                        float *imag,
                        const float *twiddle_re,
                        const float *twiddle_im,
                        int fft_size)
{
    int length;

    if ((real == 0) || (imag == 0) ||
        (twiddle_re == 0) || (twiddle_im == 0) ||
        (SpectralFFT_SizeValid(fft_size) == 0))
    {
        return 0;
    }

    SpectralFFT_BitReverse(real, imag, fft_size);
    for (length = 2; length <= fft_size; length <<= 1)
    {
        int half;
        int twiddle_step;
        int start;

        half = length >> 1;
        twiddle_step = fft_size / length;
        for (start = 0; start < fft_size; start += length)
        {
            int offset;

            for (offset = 0; offset < half; offset++)
            {
                int even_index;
                int odd_index;
                int twiddle_index;
                float wr;
                float wi;
                float even_real;
                float even_imag;
                float odd_real;
                float odd_imag;

                even_index = start + offset;
                odd_index = even_index + half;
                twiddle_index = offset * twiddle_step;
                wr = twiddle_re[twiddle_index];
                wi = twiddle_im[twiddle_index];
                even_real = real[even_index];
                even_imag = imag[even_index];
                odd_real = real[odd_index] * wr - imag[odd_index] * wi;
                odd_imag = real[odd_index] * wi + imag[odd_index] * wr;
                real[even_index] = even_real + odd_real;
                imag[even_index] = even_imag + odd_imag;
                real[odd_index] = even_real - odd_real;
                imag[odd_index] = even_imag - odd_imag;
            }
        }
    }
    return 1;
}
