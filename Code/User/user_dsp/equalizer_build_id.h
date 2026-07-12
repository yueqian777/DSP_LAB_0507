#ifndef _EQUALIZER_BUILD_ID_H_
#define _EQUALIZER_BUILD_ID_H_

#ifdef EQ_USE_GENERATED_BUILD_ID
#include "equalizer_build_id_generated.h"
#else
#define EQ_BUILD_VERSION "P33_FIX_V5"
#define EQ_BUILD_GIT_SHA "unavailable"
#define EQ_BUILD_DIRTY (-1)
#endif

#define EQ_BUILD_TIMESTAMP __DATE__ " " __TIME__

#endif /* _EQUALIZER_BUILD_ID_H_ */
