/**
 * user_spectral_fft.h
 *
 * Stateless radix-2 spectral FFT kernel shared by Projects 3.2 and 3.3.
 */

#ifndef _USER_SPECTRAL_FFT_H_
#define _USER_SPECTRAL_FFT_H_

int SpectralFFT_GenerateTwiddles(float *twiddle_re,
                                 float *twiddle_im,
                                 int fft_size);
int SpectralFFT_Forward(float *real,
                        float *imag,
                        const float *twiddle_re,
                        const float *twiddle_im,
                        int fft_size);

#endif /* _USER_SPECTRAL_FFT_H_ */
