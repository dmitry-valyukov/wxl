// winui-srcgen -- the generator that reads WinUI3 metadata and writes wxl's
// wrappers. The profiles and the metadata walk it shares with the profile
// editor come from wxl.gen.common's headers, which :winmd includes; this
// module is the output side:
//
//   :generate  orchestration of the output, one writer per artefact
//   :run       a whole run behind one call
//
// :winmd is the only unit that includes winmd_reader.h, and re-exports the
// reader's names as `md` together with wxl.gen.common's; :emit, :types,
// :metadata, :projection, :members and :writers are the output side's
// internals. All of them are exported here, which is also what lets every
// implementation unit of this module see them through the import of the
// primary interface it already has.

export module wxl.gen;

export import :md;

export import :generate;
export import :run;

export import :emit;
export import :types;
export import :metadata;
export import :projection;
export import :members;
export import :writers;
