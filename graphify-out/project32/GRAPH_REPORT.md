# Graph Report - C:\Users\zhangyueqian\lab8\DSP_LAB_0507  (2026-07-11)

## Corpus Check
- 37 files · ~35,923 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 482 nodes · 1285 edges · 16 communities (15 shown, 1 thin omitted)
- Extraction: 84% EXTRACTED · 16% INFERRED · 0% AMBIGUOUS · INFERRED: 200 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Touch LCD Scheduler|Touch LCD Scheduler]]
- [[_COMMUNITY_Noise Estimation Core|Noise Estimation Core]]
- [[_COMMUNITY_Subband Offline Evaluation|Subband Offline Evaluation]]
- [[_COMMUNITY_Signal Metrics and FFT|Signal Metrics and FFT]]
- [[_COMMUNITY_Realtime Subband Flow|Realtime Subband Flow]]
- [[_COMMUNITY_Codec Loopback Core|Codec Loopback Core]]
- [[_COMMUNITY_MCRA Evaluation Suite|MCRA Evaluation Suite]]
- [[_COMMUNITY_Audio Comparison Export|Audio Comparison Export]]
- [[_COMMUNITY_Codec Transform Engine|Codec Transform Engine]]
- [[_COMMUNITY_UI Contract Tests|UI Contract Tests]]
- [[_COMMUNITY_Minimum Statistics Evaluation|Minimum Statistics Evaluation]]
- [[_COMMUNITY_Codec Evaluation Suite|Codec Evaluation Suite]]
- [[_COMMUNITY_Touch Screen Check|Touch Screen Check]]
- [[_COMMUNITY_Loopback Evaluation|Loopback Evaluation]]
- [[_COMMUNITY_UI State Logic|UI State Logic]]

## God Nodes (most connected - your core abstractions)
1. `SubbandDenoise_Init()` - 29 edges
2. `SubbandEval_RunSNRImprovementTest()` - 26 edges
3. `SubbandTouchUIContractTest` - 24 edges
4. `SubbandWOLA_Init()` - 20 edges
5. `SubbandWOLA_SetBypass()` - 19 edges
6. `SubbandEval_RunNoiseSuppressionTest()` - 18 edges
7. `SubbandWOLA_OfflineTest_All()` - 18 edges
8. `Reset_WOLA_Test_State()` - 17 edges
9. `SubbandWOLA_ProcessFrame()` - 17 edges
10. `Subband_Apply_Demo_Mode()` - 16 edges

## Surprising Connections (you probably didn't know these)
- `main()` --calls--> `Subband_Touch_Check_Example()`  [INFERRED]
  Code/main.c → Code/User/user_dsp/user_subband_touch_check.c
- `AudioCompare_Process_Denoise_Mode()` --calls--> `SubbandWOLA_ProcessFrame()`  [INFERRED]
  Code/User/user_dsp/user_subband_audio_compare.c → Code/User/user_dsp/user_subband_wola.c
- `AudioCompare_Process_Wola()` --calls--> `SubbandWOLA_ProcessFrame()`  [INFERRED]
  Code/User/user_dsp/user_subband_audio_compare.c → Code/User/user_dsp/user_subband_wola.c
- `AudioCompare_Export_Codec()` --calls--> `SubbandCodecLoopback_SetEnabled()`  [INFERRED]
  Code/User/user_dsp/user_subband_audio_compare.c → Code/User/user_dsp/user_subband_codec_loopback.c
- `AudioCompare_Export_Codec()` --calls--> `SubbandCodec_ProcessPcm()`  [INFERRED]
  Code/User/user_dsp/user_subband_audio_compare.c → Code/User/user_dsp/user_subband_codec.c

## Import Cycles
- None detected.

## Communities (16 total, 1 thin omitted)

### Community 0 - "Touch LCD Scheduler"
Cohesion: 0.08
Nodes (69): SubbandUIPhrase, tRectangle, SubbandUIPhrase, SubbandUIButtonDef, SubbandUIButtonType, tContext, tFont, SubbandUIFont_DrawPhrase() (+61 more)

### Community 1 - "Noise Estimation Core"
Cohesion: 0.13
Nodes (51): AudioCompare_Reset_Denoise_Mode(), AudioCompare_Reset_Wola_Only(), CodecEval_Reset_Denoise_Path(), LoopbackEval_Reset_For_Codec(), SubbandCodecLoopback_SetEnabled(), Clamp_Float(), Clear_MinStat_Block(), Debug_Power_To_Q16() (+43 more)

### Community 2 - "Subband Offline Evaluation"
Cohesion: 0.12
Nodes (42): FILE, SubbandEvalResult, SubbandDenoise_GetGainMax(), SubbandDenoise_GetGainMin(), SubbandDenoise_GetLearnCount(), SubbandDenoise_GetLearnProgress(), SubbandDenoise_GetNoisePsdAvg(), SubbandDenoise_GetTargetHops() (+34 more)

### Community 3 - "Signal Metrics and FFT"
Cohesion: 0.12
Nodes (39): SubbandWOLATestMetrics, Aligned_Energy_Ratio(), Compute_Aligned_Float_Metrics(), Compute_Aligned_Metrics(), Fill_Float_Noise(), Fill_Noise_Stream(), Fill_Sine(), Fill_Sine_Stream() (+31 more)

### Community 4 - "Realtime Subband Flow"
Cohesion: 0.09
Nodes (29): main(), SUBBAND_OFFLINE_METRICS, SUBBAND_REAL, Capture_Adc_Frame(), Clear_FilterBank_State(), Fill_Dac_Inactive_Buffer(), Fill_Dac_Ping_Buffer(), Fill_Dac_Pong_Buffer() (+21 more)

### Community 5 - "Codec Loopback Core"
Cohesion: 0.16
Nodes (31): LoopbackEval_Run_Api_Smoke(), Loop_Abs(), Loop_Allocate_Bits(), Loop_Analyze_Bands(), Loop_Apply_Target_Kbps(), Loop_Bin_Scalar_Count(), Loop_Clear_Band_Work(), Loop_Compute_Scales() (+23 more)

### Community 6 - "MCRA Evaluation Suite"
Cohesion: 0.16
Nodes (28): FILE, McraEvalCase, McraEvalMetric, McraEvalMode, McraEval_Clean_Energy(), McraEval_Clip_Count(), McraEval_Compute_Pass(), McraEval_Compute_Time_Metrics() (+20 more)

### Community 7 - "Audio Comparison Export"
Cohesion: 0.17
Nodes (20): FILE, SubbandCodecStats, AudioCompare_Clip_Count(), AudioCompare_Energy(), AudioCompare_Export_Codec(), AudioCompare_Fill_Loopback_Stats(), AudioCompare_Nonzero_Count(), AudioCompare_Pad_Count() (+12 more)

### Community 8 - "Codec Transform Engine"
Cohesion: 0.17
Nodes (25): SubbandCodecStats, Codec_Abs(), Codec_Allocate_Bits(), Codec_Analyze_Bands(), Codec_Bin_Scalar_Count(), Codec_Bit_Reverse(), Codec_Clear_Band_Work(), Codec_Current_Payload_Bits() (+17 more)

### Community 10 - "Minimum Statistics Evaluation"
Cohesion: 0.21
Nodes (21): FILE, MsEvalRow, MsEval_Clean_Energy(), MsEval_Compute_Pass(), MsEval_Fill_Clean_Speechlike(), MsEval_Fill_Noise_Unit(), MsEval_Input_Snr(), MsEval_Is_Valid() (+13 more)

### Community 11 - "Codec Evaluation Suite"
Cohesion: 0.19
Nodes (18): FILE, SubbandCodecStats, CodecEvalRow, CodecEval_Aligned_Snr(), CodecEval_Clean_Name(), CodecEval_Find_Lag(), CodecEval_Input_Snr(), CodecEval_Join_Path() (+10 more)

### Community 12 - "Touch Screen Check"
Cohesion: 0.22
Nodes (18): tRectangle, Subband_Touch_Check_Example(), TouchCheck_AppendHexByte(), TouchCheck_AppendText(), TouchCheck_AppendUnsigned(), TouchCheck_ClampX(), TouchCheck_ClampY(), TouchCheck_DrawMarker() (+10 more)

### Community 13 - "Loopback Evaluation"
Cohesion: 0.25
Nodes (14): FILE, LoopbackEvalResult, LoopbackEval_Capture_Debug(), LoopbackEval_Compute_Metrics(), LoopbackEval_Compute_Speechband_Snr(), LoopbackEval_Fill_Speechlike(), LoopbackEval_Is_Invalid_Float(), LoopbackEval_Nonzero_Count() (+6 more)

### Community 14 - "UI State Logic"
Cohesion: 0.20
Nodes (12): SubbandUILearningDisplayJob, SubbandUITouchLatch, SubbandUI_AppendBounded(), SubbandUI_AppendKbps(), SubbandUI_BuildProcessingChain(), SubbandUI_CopyBounded(), SubbandUI_LatchInit(), SubbandUI_LatchUpdate() (+4 more)

## Knowledge Gaps
- **13 isolated node(s):** `SubbandCodecStats`, `McraEvalCase`, `FILE`, `SUBBAND_REAL`, `SUBBAND_OFFLINE_METRICS` (+8 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **1 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Subband_Flow_Example()` connect `Realtime Subband Flow` to `Touch LCD Scheduler`?**
  _High betweenness centrality (0.090) - this node is a cross-community bridge._
- **Why does `main()` connect `Realtime Subband Flow` to `Touch Screen Check`?**
  _High betweenness centrality (0.079) - this node is a cross-community bridge._
- **Why does `Subband_Touch_Check_Example()` connect `Touch Screen Check` to `Realtime Subband Flow`?**
  _High betweenness centrality (0.072) - this node is a cross-community bridge._
- **Are the 7 inferred relationships involving `SubbandEval_RunSNRImprovementTest()` (e.g. with `SubbandDenoise_GetGainAvg()` and `SubbandDenoise_GetGainMax()`) actually correct?**
  _`SubbandEval_RunSNRImprovementTest()` has 7 INFERRED edges - model-reasoned connections that need verification._
- **Are the 12 inferred relationships involving `SubbandWOLA_Init()` (e.g. with `AudioCompare_Reset_Denoise_Mode()` and `AudioCompare_Reset_Wola_Only()`) actually correct?**
  _`SubbandWOLA_Init()` has 12 INFERRED edges - model-reasoned connections that need verification._
- **Are the 17 inferred relationships involving `SubbandWOLA_SetBypass()` (e.g. with `AudioCompare_Reset_Denoise_Mode()` and `AudioCompare_Reset_Wola_Only()`) actually correct?**
  _`SubbandWOLA_SetBypass()` has 17 INFERRED edges - model-reasoned connections that need verification._
- **What connects `SubbandCodecStats`, `McraEvalCase`, `FILE` to the rest of the system?**
  _13 weakly-connected nodes found - possible documentation gaps or missing edges._