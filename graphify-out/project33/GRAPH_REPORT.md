# Graph Report - C:\Users\zhangyueqian\lab8\DSP_LAB_0507  (2026-07-11)

## Corpus Check
- 10 files · ~13,196 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 172 nodes · 556 edges · 11 communities
- Extraction: 88% EXTRACTED · 12% INFERRED · 0% AMBIGUOUS · INFERRED: 65 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Equalizer Offline Evaluation|Equalizer Offline Evaluation]]
- [[_COMMUNITY_Equalizer LCD Display|Equalizer LCD Display]]
- [[_COMMUNITY_Realtime Equalizer Flow|Realtime Equalizer Flow]]
- [[_COMMUNITY_RBJ Processing Core|RBJ Processing Core]]
- [[_COMMUNITY_Legacy and Bypass State|Legacy and Bypass State]]
- [[_COMMUNITY_Frequency Response Analysis|Frequency Response Analysis]]
- [[_COMMUNITY_Biquad Design Validation|Biquad Design Validation]]
- [[_COMMUNITY_Gain Transition Control|Gain Transition Control]]
- [[_COMMUNITY_Initialization and Cache|Initialization and Cache]]
- [[_COMMUNITY_RBJ Filter Bank|RBJ Filter Bank]]

## God Nodes (most connected - your core abstractions)
1. `EQ_STATE` - 38 edges
2. `Equalizer_ProcessFrame()` - 17 edges
3. `EQ_EvalConfigure()` - 17 edges
4. `EqualizerEval_OfflineTest_All()` - 15 edges
5. `EQ_Abs()` - 14 edges
6. `Equalizer_SetCoreMode()` - 13 edges
7. `EQ_EvalWriteSystemResponse()` - 13 edges
8. `Equalizer_Flow_Example()` - 13 edges
9. `Equalizer_ApplyPreset()` - 12 edges
10. `Equalizer_ProcessFrameFloat()` - 12 edges

## Surprising Connections (you probably didn't know these)
- `main()` --calls--> `Equalizer_Flow_Example()`  [INFERRED]
  Code/main.c → Code/User/user_dsp/user_equalizer_flow.c
- `main()` --calls--> `Equalizer_Init()`  [INFERRED]
  Code/User/user_dsp/user_equalizer_display.c → Code/User/user_dsp/user_equalizer.c
- `EQ_EvalConfigure()` --calls--> `Equalizer_Init()`  [INFERRED]
  Code/User/user_dsp/user_equalizer_eval.c → Code/User/user_dsp/user_equalizer.c
- `EQ_EvalWritePresetCacheReport()` --calls--> `Equalizer_Init()`  [INFERRED]
  Code/User/user_dsp/user_equalizer_eval.c → Code/User/user_dsp/user_equalizer.c
- `EQ_EvalWriteTransparencyReport()` --calls--> `Equalizer_Init()`  [INFERRED]
  Code/User/user_dsp/user_equalizer_eval.c → Code/User/user_dsp/user_equalizer.c

## Import Cycles
- None detected.

## Communities (11 total, 0 thin omitted)

### Community 0 - "Equalizer Offline Evaluation"
Cohesion: 0.15
Nodes (39): EQ_STATE, Equalizer_ApplyPreset(), Equalizer_GetBandCenterHz(), Equalizer_HasPendingTransition(), Equalizer_ProcessFrameFloat(), EQ_EvalAbs(), EQ_EvalBinPower(), EQ_EvalConfigure() (+31 more)

### Community 1 - "Equalizer LCD Display"
Cohesion: 0.15
Nodes (37): EQ_STATE, EQ_BeginDraw(), EQ_ClampDbForDisplay(), EQ_ClampInt(), EQ_ClipRect(), EQ_DbToY(), EQ_DisplayAbs(), EQ_DisplayTestRequire() (+29 more)

### Community 2 - "Realtime Equalizer Flow"
Cohesion: 0.18
Nodes (15): main(), EQ_CaptureAdcFrame(), EQ_ClearDacBuffers(), EQ_FillDacInactiveBuffer(), EQ_FillDacPingBuffer(), EQ_FillDacPongBuffer(), EQ_KeepBuildFingerprint(), EQ_NormalizeDiagPath() (+7 more)

### Community 3 - "RBJ Processing Core"
Cohesion: 0.30
Nodes (13): EQ_AllGainsFlat(), EQ_ApplyRbjPresetNow(), EQ_CopyTransparentShort(), EQ_ProcessFloatCopyShort(), EQ_ProcessRbjBank(), EQ_ProcessRbjSample(), EQ_SaturateToShort(), EQ_UpdateDebugPeaks() (+5 more)

### Community 4 - "Legacy and Bypass State"
Cohesion: 0.26
Nodes (12): EQ_STATE, EQ_CancelTransition(), EQ_NextLegacyGain(), EQ_ProcessLegacySample(), EQ_ResetLegacyState(), EQ_UpdateDebugHeadroom(), Equalizer_GetBypass(), Equalizer_GetCoreMode() (+4 more)

### Community 5 - "Frequency Response Analysis"
Cohesion: 0.29
Nodes (12): EQ_COMPLEX, EQ_BankResponse(), EQ_BiquadResponse(), EQ_ComplexAdd(), EQ_ComplexMagnitude(), EQ_ComplexMultiply(), EQ_ComplexPhase(), EQ_ComplexScale() (+4 more)

### Community 6 - "Biquad Design Validation"
Cohesion: 0.38
Nodes (11): EQ_BIQUAD, EQ_SECTION_INFO, EQ_BiquadIsFinite(), EQ_BiquadIsStable(), EQ_BiquadPoleRadii(), EQ_DesignRbjPeaking(), EQ_DesignRbjShelf(), EQ_IsFinite() (+3 more)

### Community 7 - "Gain Transition Control"
Cohesion: 0.36
Nodes (10): EQ_Abs(), EQ_ClampDb(), EQ_FadeSamples(), EQ_FindRbjCachedBank(), EQ_GainsMatch(), EQ_InstallRbjTarget(), EQ_SetLegacyTarget(), Equalizer_ApplySingleBand1kPlus3Db() (+2 more)

### Community 8 - "Initialization and Cache"
Cohesion: 0.43
Nodes (7): EQ_DbToGain(), EQ_DesignLegacyCoeffs(), EQ_EnsureRbjBankCache(), EQ_InstallRbjCachedTarget(), EQ_SetLegacyImmediate(), Equalizer_Init(), Equalizer_Reset()

### Community 9 - "RBJ Filter Bank"
Cohesion: 0.47
Nodes (6): EQ_FILTER_BANK, EQ_BankPeakDb(), EQ_BuildRbjBank(), EQ_ClearBankState(), EQ_InstallRbjCandidate(), EQ_TransitionSamples()

## Knowledge Gaps
- **1 isolated node(s):** `EQ_SECTION_INFO`
  These have ≤1 connection - possible missing edges or undocumented components.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Equalizer_Init()` connect `Initialization and Cache` to `Equalizer Offline Evaluation`, `Equalizer LCD Display`, `Realtime Equalizer Flow`, `RBJ Processing Core`, `Legacy and Bypass State`?**
  _High betweenness centrality (0.123) - this node is a cross-community bridge._
- **Why does `main()` connect `Equalizer LCD Display` to `Equalizer Offline Evaluation`, `Initialization and Cache`?**
  _High betweenness centrality (0.120) - this node is a cross-community bridge._
- **Why does `Equalizer_GetBandTargetGainDb()` connect `Equalizer LCD Display` to `Equalizer Offline Evaluation`, `Realtime Equalizer Flow`, `RBJ Processing Core`, `Legacy and Bypass State`?**
  _High betweenness centrality (0.104) - this node is a cross-community bridge._
- **Are the 7 inferred relationships involving `Equalizer_ProcessFrame()` (e.g. with `EQ_EvalWriteExpectedClipReport()` and `EQ_EvalWriteHeadroom()`) actually correct?**
  _`Equalizer_ProcessFrame()` has 7 INFERRED edges - model-reasoned connections that need verification._
- **Are the 4 inferred relationships involving `EQ_EvalConfigure()` (e.g. with `Equalizer_ApplyPreset()` and `Equalizer_Init()`) actually correct?**
  _`EQ_EvalConfigure()` has 4 INFERRED edges - model-reasoned connections that need verification._
- **What connects `EQ_SECTION_INFO` to the rest of the system?**
  _1 weakly-connected nodes found - possible documentation gaps or missing edges._