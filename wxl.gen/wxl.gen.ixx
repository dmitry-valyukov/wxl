// winui-srcgen -- the generator that reads WinUI3 metadata and writes wxl's
// wrappers. Three stages, one partition each:
//
//   :profile   input -- the JSON profiles saying what to generate
//   :crawl     the metadata walk producing the type closure
//   :generate  orchestration of the output, one writer per artefact
//
// :winmd is the only unit that includes winmd_reader.h, and re-exports the
// reader's names as `md`; :emit, :types, :metadata, :projection, :members
// and :writers are the output side's internals. All of them are exported
// here, which is also what lets every implementation unit of this module
// see them through the import of the primary interface it already has.

export module wxl.gen;

export import :md;

export import :profile;
export import :crawl;
export import :generate;
export import :run;

export import :emit;
export import :types;
export import :metadata;
export import :projection;
export import :members;
export import :writers;
