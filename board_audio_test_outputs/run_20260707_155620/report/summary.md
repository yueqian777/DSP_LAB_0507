# Board + PC Audio Test Summary

## Board Runtime

- Realtime pass: 8/8
- Worst max runtime: 12540 us
- Frame budget: 20000 us

## PC Audio Compare

- Audio cases processed: 8
- Metrics rows: 72

## Notes

- Board data comes from CCS DSS Watch variables while the target runs AD/DA in real time.
- PC audio compare uses the current existing compare path. It exports raw input, WOLA, fixed denoise, direct codec, and denoise+codec WAVs.
- Mode 6 and mode 7 are covered by board runtime Watch data. The current existing compare exporter does not emit separate mode 6/7 WAV files without changing project source.

## Board Runtime Table

|   mode | mode_name          |   max_us |   budget_us |   cpu_percent | realtime_pass   |
|-------:|:-------------------|---------:|------------:|--------------:|:----------------|
|      0 | raw_bypass         |      405 |       20000 |             1 | True            |
|      1 | wola_only          |     2028 |       20000 |             9 | True            |
|      2 | fixed_denoise      |     6459 |       20000 |            31 | True            |
|      3 | mild_fixed_denoise |     6460 |       20000 |            31 | True            |
|      4 | hybrid_ms          |     8617 |       20000 |            42 | True            |
|      5 | mild_hybrid_ms     |     8617 |       20000 |            42 | True            |
|      6 | mcra_normal        |    12515 |       20000 |            61 | True            |
|      7 | mcra_strong        |    12540 |       20000 |            61 | True            |
