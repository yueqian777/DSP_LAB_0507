/**
 * user_subband_test.h
 *
 * PC algo-only tests for the WOLA subband processor.
 */

#ifndef _USER_SUBBAND_TEST_H_
#define _USER_SUBBAND_TEST_H_

typedef struct
{
    double max_error;
    double mse;
    double input_energy;
    double output_energy_ratio;
    int aligned_lag;
    double aligned_gain;
    double aligned_snr_db;
} SubbandWOLATestMetrics;

void SubbandWOLA_OfflineTest_All(void);

#endif /* _USER_SUBBAND_TEST_H_ */
