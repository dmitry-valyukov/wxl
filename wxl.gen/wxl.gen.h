#pragma once

// winui-srcgen -- the generator that reads WinUI3 metadata and writes wxl's
// wrappers, as an ordinary library under the executable. The profiles and the
// metadata walk it shares with the profile editor come from wxl.gen.common
// through md.h; the rest is the output side:
//
//   generate.h  orchestration of the output, one writer per artefact
//   run.h       a whole run behind one call
//
// gen/emit.h, types.h, metadata.h, projection.h, members.h and writers.h are
// the output side's internals. Every source of the library includes this file.

#include "md.h"

#include "generate.h"
#include "run.h"

#include "gen/emit.h"
#include "gen/types.h"
#include "gen/metadata.h"
#include "gen/projection.h"
#include "gen/members.h"
#include "gen/writers.h"
