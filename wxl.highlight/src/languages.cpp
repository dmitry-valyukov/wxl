// Таблицы языков подсветки. СГЕНЕРИРОВАНО tools/import-syntax.ps1 из двух
// источников: определений RsdnFormatter (Format/CodeFormat/Syntax/*.json,
// MIT, © Russian Software Developer Network) и наших собственных описаний
// в tools/syntax — править не здесь, а в скрипте или в источнике.
//
// Ключевые слова каждого набора отсортированы по кодовым единицам (у
// языков без учёта регистра — в нижнем регистре): сканер ищет двоичным
// поиском. Комментарии и строки в источнике описаны регулярными
// выражениями; их перевод в правила «открыл — закрыл» — таблица
// $Delimiters в скрипте.

module wxl.highlight;

import std;

namespace wxl::highlight {

namespace {

using enum kind_t;
using enum boundary_t;
// ---- Assembler ----

constexpr delimited_rule kAssemblerDelimiters[] = {
    {comment, L";", L"", L"", 0, false, false, false, false, false},
    {string, L"'", L"'", L"", 0, true, false, false, false, false},
};

constexpr std::wstring_view kAssemblerWords0[] = {
    L"__sect__", L"aaa", L"aad", L"aam", L"aas", L"abs", L"absolute", L"adc", L"add", L"addps",
    L"addr", L"addss", L"ah", L"al", L"alias", L"and", L"andnps", L"andps", L"arg", L"arpl",
    L"assume", L"at", L"ax", L"basic", L"bh", L"bits", L"bl", L"bound", L"bp", L"bsf", L"bsr",
    L"bswap", L"bt", L"btc", L"btr", L"bts", L"bx", L"byte", L"c", L"call", L"casemap",
    L"catstr", L"cbw", L"cdq", L"ch", L"cl", L"clc", L"cld", L"cli", L"clts", L"cmc", L"cmova",
    L"cmovae", L"cmovb", L"cmovbe", L"cmovc", L"cmove", L"cmovg", L"cmovge", L"cmovl",
    L"cmovle", L"cmovna", L"cmovnae", L"cmovnb", L"cmovnbe", L"cmovnc", L"cmovne", L"cmovng",
    L"cmovnge", L"cmovnl", L"cmovnle", L"cmovno", L"cmovnp", L"cmovns", L"cmovnz", L"cmovo",
    L"cmovp", L"cmovpe", L"cmovpo", L"cmovs", L"cmovz", L"cmp", L"cmpeqps", L"cmpeqss",
    L"cmpleps", L"cmpless", L"cmpltps", L"cmpltss", L"cmpneqps", L"cmpneqss", L"cmpnleps",
    L"cmpnless", L"cmpnltps", L"cmpnltss", L"cmpordps", L"cmpordss", L"cmpps", L"cmpsb",
    L"cmpsd", L"cmpss", L"cmpsw", L"cmpunordps", L"cmpunordss", L"cmpxchg", L"cmpxchg8b",
    L"codeptr", L"codeseg", L"comiss", L"comm", L"comment", L"common", L"compact", L"cpp",
    L"cpuid", L"cs", L"cvtpi2ps", L"cvtps2pi", L"cvtsi2ss", L"cvtss2si", L"cvttps2pi",
    L"cvttss2si", L"cwd", L"cwde", L"cx", L"daa", L"das", L"dataptr", L"db", L"dd", L"dec",
    L"df", L"dh", L"di", L"display", L"div", L"divps", L"divss", L"dl", L"dq", L"ds", L"dt",
    L"dup", L"dw", L"dword", L"dx", L"eax", L"ebp", L"ebx", L"echo", L"ecx", L"edi", L"edx",
    L"elif", L"elseif1", L"elseif2", L"elseifb", L"elseifdef", L"elseifdif", L"elseifdifi",
    L"elseife", L"elseifidn", L"elseifidni", L"elseifnb", L"elseifndef", L"emms", L"emul",
    L"end", L"endm", L"endp", L"ends", L"endstruc", L"enter", L"enterd", L"enterw", L"enum",
    L"eq", L"equ", L"errif", L"errif1", L"errif2", L"errifb", L"errifdef", L"errifdif",
    L"errifdifi", L"errife", L"errifidn", L"errifidni", L"errifnb", L"errifndef", L"es", L"esi",
    L"esp", L"even", L"evendata", L"exitcode", L"exitm", L"export", L"extern", L"externdef",
    L"extrn", L"f2xm1", L"fabs", L"fadd", L"faddp", L"false", L"far", L"far16", L"far32",
    L"fastimul", L"fbld", L"fbstp", L"fchs", L"fclex", L"fcmovb", L"fcmovbe", L"fcmove",
    L"fcmovnb", L"fcmovnbe", L"fcmovne", L"fcmovnu", L"fcmovu", L"fcom", L"fcomi", L"fcomip",
    L"fcomp", L"fcompp", L"fcos", L"fdecstp", L"fdisi", L"fdiv", L"fdivp", L"fdivr", L"fdivrp",
    L"feni", L"ffree", L"fiadd", L"ficom", L"ficomp", L"fidiv", L"fidivr", L"fild", L"fimul",
    L"fincstp", L"finit", L"fist", L"fistp", L"fisub", L"fisubr", L"flat", L"fld", L"fld1",
    L"fldcw", L"fldenv", L"fldenvd", L"fldenvw", L"fldl2e", L"fldl2t", L"fldlg2", L"fldln2",
    L"fldpi", L"fldz", L"flipflag", L"fmul", L"fmulp", L"fnclex", L"fndisi", L"fneni",
    L"fninit", L"fnldenv", L"fnop", L"fnrstor", L"fnsave", L"fnsaved", L"fnsavew", L"fnstcw",
    L"fnstenv", L"fnstenvd", L"fnstenvw", L"fnstsw", L"for", L"forc", L"fortran", L"fpatan",
    L"fprem", L"fprem1", L"fptan", L"frndint", L"frstor", L"frstord", L"frstorw", L"fs",
    L"fsave", L"fsaved", L"fsavew", L"fscale", L"fsetpm", L"fsin", L"fsincos", L"fsqrt", L"fst",
    L"fstcw", L"fstenv", L"fstenvd", L"fstenvw", L"fstp", L"fstsw", L"fsub", L"fsubp", L"fsubr",
    L"fsubrp", L"ftst", L"fucom", L"fucomi", L"fucomip", L"fucomp", L"fucompp", L"fword",
    L"fxam", L"fxch", L"fxrstor", L"fxsave", L"fxtract", L"fyl2x", L"fyl2xp1", L"ge",
    L"getfield", L"global", L"goto", L"group", L"gs", L"high", L"hlt", L"huge", L"ideal",
    L"idiv", L"iend", L"if0", L"if1", L"if2", L"ifb", L"ifdef", L"ifdif", L"ifdifi", L"ifdifs",
    L"ife", L"ifeq", L"ifidn", L"ifidni", L"iflow", L"ifnb", L"ifndef", L"ifneq", L"ifnidn",
    L"import", L"imul", L"in", L"inc", L"incbin", L"include", L"includelib", L"insb", L"insd",
    L"instr", L"insw", L"int", L"int1", L"int3", L"into", L"invd", L"invlpg", L"invoke",
    L"iret", L"iretd", L"iretdf", L"iretf", L"iretw", L"irp", L"irpc", L"istruc", L"ja", L"jae",
    L"jb", L"jbe", L"jc", L"jcxz", L"je", L"jecxz", L"jg", L"jge", L"jl", L"jle", L"jmp",
    L"jna", L"jnae", L"jnb", L"jnbe", L"jnc", L"jne", L"jng", L"jnge", L"jnl", L"jnle", L"jno",
    L"jnp", L"jns", L"jnz", L"jo", L"jp", L"jpe", L"jpo", L"js", L"jz", L"label", L"lahf",
    L"lar", L"large", L"largestack", L"ldmxcsr", L"lds", L"le", L"lea", L"leave", L"leaved",
    L"leavew", L"length", L"les", L"lfs", L"lgdt", L"lgs", L"lidt", L"lldt", L"lmsw", L"local",
    L"locals", L"lodsb", L"lodsd", L"lodsw", L"loop", L"loope", L"looped", L"loopew", L"loopne",
    L"loopned", L"loopnew", L"loopnz", L"loopnzd", L"loopnzw", L"loopz", L"loopzd", L"loopzw",
    L"low", L"lsl", L"lss", L"ltr", L"macro", L"mask", L"maskflag", L"maskmovq", L"masm",
    L"masm51", L"maxps", L"maxss", L"medium", L"memory", L"method", L"minps", L"minss", L"mov",
    L"movaps", L"movd", L"movhlps", L"movhps", L"movlhps", L"movlps", L"movmskps", L"movntps",
    L"movntq", L"movq", L"movs", L"movsb", L"movsd", L"movss", L"movsw", L"movsx", L"movups",
    L"movzx", L"mul", L"mulps", L"mulss", L"multerrs", L"name", L"ne", L"near", L"near16",
    L"near32", L"neg", L"noemul", L"nojumps", L"nolanguage", L"nolocals", L"nomasm51",
    L"nomulterrs", L"none", L"nop", L"normal", L"nosmart", L"not", L"nothing", L"nowarn",
    L"oddfar", L"oddnear", L"offset", L"option", L"or", L"org", L"orps", L"out", L"outsb",
    L"outsd", L"outsw", L"overflow?", L"packssdw", L"packsswb", L"packuswb", L"paddb", L"paddd",
    L"paddsb", L"paddsw", L"paddusb", L"paddusw", L"paddw", L"page", L"pand", L"pandn", L"para",
    L"parity?", L"pascal", L"pavgb", L"pavgw", L"pcmpeqb", L"pcmpeqd", L"pcmpeqw", L"pcmpgtb",
    L"pcmpgtd", L"pcmpgtw", L"pextrw", L"pinsrw", L"pmaddwd", L"pmaxsw", L"pmaxub", L"pminsw",
    L"pminub", L"pmmx", L"pmovmskb", L"pmulhuw", L"pmulhw", L"pmullw", L"pnommx", L"pop",
    L"popa", L"popad", L"popaw", L"popf", L"popfd", L"popfw", L"popstate", L"por",
    L"prefetchnta", L"prefetcht0", L"prefetcht1", L"prefetcht2", L"private", L"proc",
    L"procdesc", L"proctype", L"prolog", L"proto", L"psadbw", L"pshufw", L"pslld", L"psllq",
    L"psllw", L"psrad", L"psraw", L"psrld", L"psrlq", L"psrlw", L"psubb", L"psubd", L"psubsb",
    L"psubsw", L"psubusb", L"psubusw", L"psubw", L"ptr", L"public", L"publicdll", L"punpckhbw",
    L"punpckhdq", L"punpckhwd", L"punpcklbw", L"punpckldq", L"punpcklwd", L"purge", L"push",
    L"pusha", L"pushad", L"pushaw", L"pushd", L"pushf", L"pushfd", L"pushfw", L"pushstate",
    L"pushw", L"pword", L"pxor", L"quirks", L"qword", L"rcl", L"rcpps", L"rcpss", L"rcr",
    L"rdmsr", L"rdpmc", L"rdtsc", L"real10", L"real4", L"real8", L"record", L"rep", L"repe",
    L"repeat", L"repne", L"repnz", L"rept", L"repz", L"resb", L"resd", L"resq", L"rest",
    L"resw", L"ret", L"retcode", L"retf", L"retn", L"returns", L"rol", L"ror", L"rsm",
    L"rsqrtps", L"rsqrtss", L"sahf", L"sar", L"sbb", L"sbyte", L"scasb", L"scasd", L"scasw",
    L"sdword", L"section", L"seg", L"segment", L"seta", L"setae", L"setb", L"setbe", L"setc",
    L"sete", L"setfield", L"setflag", L"setg", L"setge", L"setl", L"setle", L"setna", L"setnae",
    L"setnb", L"setnbe", L"setnc", L"setne", L"setng", L"setnge", L"setnl", L"setnle", L"setno",
    L"setnp", L"setns", L"setnz", L"seto", L"setp", L"setpe", L"setpo", L"sets", L"setz",
    L"sfence", L"sgdt", L"shl", L"shld", L"short", L"shr", L"shrd", L"shufps", L"si", L"sidt",
    L"sign", L"size", L"sizestr", L"sldt", L"small", L"smart", L"smsw", L"sp", L"sqrtps",
    L"sqrtss", L"ss", L"startupcode", L"stc", L"std", L"stdcall", L"sti", L"stmxcsr", L"stos",
    L"stosb", L"stosd", L"stosw", L"str", L"struc", L"struct", L"sub", L"subps", L"subss",
    L"substr", L"subtitle", L"subttl", L"sword", L"symtype", L"syscall", L"sysenter",
    L"sysexit", L"sysret", L"table", L"tblinit", L"tblptr", L"tbyte", L"tchuge", L"test",
    L"testflag", L"textequ", L"this", L"times", L"tiny", L"title", L"tpascal", L"true",
    L"tword", L"typedef", L"ucomiss", L"ud2", L"unicode", L"union", L"unknown", L"unpckhps",
    L"unpcklps", L"uppercase", L"use16", L"use32", L"uses", L"verr", L"verw", L"wait", L"warn",
    L"wbinvd", L"width", L"windows", L"with", L"word", L"wrmsr", L"xadd", L"xchg", L"xlatb",
    L"xor", L"xorps", L"zero?",
};

constexpr std::wstring_view kAssemblerWords1[] = {
    L"alpha", L"break", L"continue", L"cref", L"endw", L"err1", L"err2", L"errb", L"errdef",
    L"exit", L"lall", L"lfcond", L"list", L"listall", L"listif", L"listmacro", L"listmacroall",
    L"mmx", L"nocref", L"nolist", L"nolistif", L"nolistmacro", L"nommx", L"sall", L"seq",
    L"sfcond", L"tfcond", L"until", L"untilcxz", L"xall", L"xcref", L"xlist",
};

constexpr std::wstring_view kAssemblerWords2[] = {
    L"const", L"dosseg", L"else", L"elseif", L"endif", L"err", L"errdif", L"errdifi", L"erre",
    L"erridn", L"erridni", L"errnb", L"errndef", L"errnz", L"if", L"radix", L"stack", L"type",
    L"while",
};

constexpr std::wstring_view kAssemblerWords3[] = {
    L"??date", L"??filename", L"??time", L"@codesize", L"@cpu", L"@datasize", L"@filename",
    L"@wordsize",
};

constexpr std::wstring_view kAssemblerWords4[] = {
    L"curseg",
};

constexpr std::wstring_view kAssemblerWords5[] = {
    L"version",
};

constexpr std::wstring_view kAssemblerWords6[] = {
    L"code", L"data", L"fardata", L"model", L"startup",
};

constexpr std::wstring_view kAssemblerWords7[] = {
    L"carry?", L"data?", L"fardata?",
};

constexpr keyword_set kAssemblerKeywords[] = {
    {word, word, kAssemblerWords0, 11},
    {dot, word, kAssemblerWords1, 12},
    {dot_or_word, word, kAssemblerWords2, 7},
    {none, word, kAssemblerWords3, 10},
    {at_or_word, word, kAssemblerWords4, 6},
    {word_or_double_question, word, kAssemblerWords5, 7},
    {dot_or_at_or_word, word, kAssemblerWords6, 7},
    {word, none, kAssemblerWords7, 8},
};

// ---- C ----

constexpr delimited_rule kCDelimiters[] = {
    {comment, L"//", L"", L"", 0, false, false, false, false, false},
    {comment, L"/*", L"*/", L"", 0, false, true, false, false, false},
    {string, L"@\"", L"\"", L"", 0, true, true, false, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
    {string, L"'", L"'", L"", L'\\', false, false, false, true, false},
};

constexpr std::wstring_view kCWords0[] = {
    L"define", L"elif", L"else", L"endif", L"error", L"if", L"ifdef", L"ifndef", L"import",
    L"include", L"line", L"pragma", L"undef",
};

constexpr std::wstring_view kCWords1[] = {
    L"__abstract", L"__asm", L"__based", L"__box", L"__cdecl", L"__declspec", L"__delegate",
    L"__event", L"__except", L"__fastcall", L"__finally", L"__gc", L"__identifier", L"__inline",
    L"__int16", L"__int32", L"__int64", L"__int8", L"__interface", L"__leave",
    L"__multiple_inheritance", L"__nogc", L"__pin", L"__property", L"__sealed",
    L"__single_inheritance", L"__stdcall", L"__try", L"__try_cast", L"__typeof", L"__uuidof",
    L"__value", L"__virtual_inheritance", L"asm", L"auto", L"bad_cast", L"bad_typeid", L"bool",
    L"break", L"case", L"catch", L"char", L"class", L"const", L"const_cast", L"continue",
    L"default", L"delete", L"do", L"double", L"dynamic_cast", L"else", L"enum", L"except",
    L"explicit", L"extern", L"false", L"finally", L"float", L"for", L"friend", L"goto", L"if",
    L"inline", L"int", L"long", L"mutable", L"namespace", L"new", L"operator", L"private",
    L"protected", L"public", L"register", L"reinterpret_cast", L"return", L"short", L"signed",
    L"sizeof", L"static", L"static_cast", L"struct", L"switch", L"template", L"this", L"throw",
    L"true", L"try", L"type_info", L"typedef", L"typeid", L"typename", L"union", L"unsigned",
    L"using", L"virtual", L"void", L"volatile", L"wchar", L"wchar_t", L"while",
};

constexpr keyword_set kCKeywords[] = {
    {hash_with_space, word, kCWords0, 7},
    {word, word, kCWords1, 22},
};

// ---- CSharp ----

constexpr delimited_rule kCSharpDelimiters[] = {
    {comment, L"//", L"", L"", 0, false, false, false, false, false},
    {comment, L"/*", L"*/", L"", 0, false, true, false, false, false},
    {string, L"@\"", L"\"", L"", 0, true, true, false, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
    {string, L"'", L"'", L"", L'\\', false, false, false, true, false},
};

constexpr std::wstring_view kCSharpWords0[] = {
    L"define", L"elif", L"else", L"endif", L"endregion", L"error", L"if", L"line", L"pragma",
    L"region", L"undef", L"warning",
};

constexpr std::wstring_view kCSharpWords1[] = {
    L"__arglist", L"__makeref", L"__reftype", L"__refvalue", L"abstract", L"add", L"alias",
    L"as", L"ascending", L"assembly", L"base", L"bool", L"break", L"by", L"byte", L"case",
    L"catch", L"char", L"checked", L"class", L"const", L"continue", L"decimal", L"default",
    L"delegate", L"descending", L"do", L"double", L"dynamic", L"else", L"enum", L"equals",
    L"event", L"explicit", L"extern", L"false", L"field", L"finally", L"fixed", L"float",
    L"for", L"foreach", L"from", L"get", L"global", L"goto", L"group", L"if", L"implicit",
    L"in", L"int", L"interface", L"internal", L"into", L"is", L"join", L"let", L"lock", L"long",
    L"method", L"module", L"namespace", L"new", L"null", L"object", L"on", L"operator",
    L"orderby", L"out", L"override", L"param", L"params", L"partial", L"private", L"property",
    L"protected", L"public", L"readonly", L"ref", L"remove", L"return", L"sbyte", L"sealed",
    L"select", L"set", L"short", L"sizeof", L"stackalloc", L"static", L"string", L"struct",
    L"switch", L"this", L"throw", L"true", L"try", L"type", L"typeof", L"typevar", L"uint",
    L"ulong", L"unchecked", L"unsafe", L"ushort", L"using", L"value", L"var", L"virtual",
    L"void", L"volatile", L"where", L"while", L"yield",
};

constexpr keyword_set kCSharpKeywords[] = {
    {hash_with_space, word, kCSharpWords0, 9},
    {word, word, kCSharpWords1, 10},
};

// ---- Erlang ----

constexpr delimited_rule kErlangDelimiters[] = {
    {comment, L"%", L"", L"", 0, false, false, false, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
    {string, L"'", L"'", L"", L'\\', false, false, false, false, false},
};

constexpr std::wstring_view kErlangWords0[] = {
    L"after", L"begin", L"case", L"catch", L"cond", L"end", L"fun", L"if", L"let", L"of",
    L"query", L"receive", L"try", L"when",
};

constexpr keyword_set kErlangKeywords[] = {
    {word, word, kErlangWords0, 7},
};

// ---- Haskell ----

constexpr delimited_rule kHaskellDelimiters[] = {
    {comment, L"--", L"", L"", 0, false, false, false, false, false},
    {comment, L"{-", L"-}", L"", 0, false, true, false, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
    {string, L"'", L"'", L"", L'\\', false, false, false, false, false},
};

constexpr std::wstring_view kHaskellWords0[] = {
    L"_", L"anyclass", L"as", L"case", L"class", L"data", L"default", L"deriving", L"do",
    L"else", L"family", L"forall", L"foreign", L"hiding", L"if", L"import", L"in", L"infix",
    L"infixl", L"infixr", L"instance", L"let", L"mdo", L"module", L"newtype", L"of", L"pattern",
    L"qualified", L"role", L"static", L"stock", L"then", L"type", L"via", L"where",
};

constexpr keyword_set kHaskellKeywords[] = {
    {word, word, kHaskellWords0, 9},
};

// ---- IDL ----

constexpr delimited_rule kIDLDelimiters[] = {
    {comment, L"//", L"", L"", 0, false, false, false, false, false},
    {comment, L"/*", L"*/", L"", 0, false, true, false, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
    {string, L"'", L"'", L"", L'\\', false, false, false, false, false},
};

constexpr std::wstring_view kIDLWords0[] = {
    L"FALSE", L"Object", L"TRUE", L"any", L"attribute", L"boolean", L"case", L"char", L"const",
    L"context", L"default", L"double", L"enum", L"exception", L"fixed", L"float", L"in",
    L"inout", L"interface", L"long", L"module", L"native", L"octet", L"oneway", L"out",
    L"raises", L"readonly", L"sequence", L"short", L"string", L"struct", L"switch", L"typedef",
    L"union", L"unsigned", L"void", L"wchar", L"wstring",
};

constexpr keyword_set kIDLKeywords[] = {
    {word, word, kIDLWords0, 9},
};

// ---- Java ----

constexpr delimited_rule kJavaDelimiters[] = {
    {comment, L"//", L"", L"", 0, false, false, false, false, false},
    {comment, L"/*", L"*/", L"", 0, false, true, false, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
};

constexpr std::wstring_view kJavaWords0[] = {
    L"abstract", L"boolean", L"break", L"byte", L"case", L"catch", L"char", L"class", L"const",
    L"continue", L"debugger", L"default", L"delete", L"do", L"double", L"else", L"enum",
    L"export", L"extends", L"false", L"final", L"finally", L"float", L"for", L"function",
    L"goto", L"if", L"implements", L"import", L"in", L"instanceof", L"int", L"interface",
    L"long", L"native", L"new", L"null", L"package", L"private", L"protected", L"public",
    L"return", L"short", L"static", L"super", L"switch", L"synchronized", L"this", L"throw",
    L"throws", L"transient", L"true", L"try", L"typeof", L"var", L"void", L"volatile", L"while",
    L"with",
};

constexpr keyword_set kJavaKeywords[] = {
    {word, word, kJavaWords0, 12},
};

// ---- Lisp ----

constexpr delimited_rule kLispDelimiters[] = {
    {comment, L";", L"", L"", 0, false, false, false, false, false},
    {comment, L"#|", L"|#", L"", 0, false, true, false, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
};

constexpr std::wstring_view kLispWords0[] = {
    L"apply", L"block", L"case", L"catch", L"clear-output", L"close", L"compile", L"cond",
    L"count-if", L"count-if-not", L"declaim", L"declare", L"defconstant", L"defmacro",
    L"defpackage", L"defparameter", L"defun", L"defvar", L"delete-file", L"directory", L"do",
    L"dolist", L"dotimes", L"ensure-directories-exist", L"eval", L"export", L"file-exists-p",
    L"find-if", L"find-if-not", L"find-symbol", L"finish-output", L"force-output", L"format",
    L"fresh-line", L"funcall", L"function", L"go", L"handler-case", L"if", L"import",
    L"in-package", L"intern", L"lambda", L"let", L"let*", L"load", L"loop", L"make-package",
    L"mapc", L"mapcan", L"mapcar", L"mapcon", L"mapl", L"maplist", L"merge",
    L"multiple-value-bind", L"multiple-value-call", L"open", L"position-if", L"position-if-not",
    L"prin1", L"princ", L"print", L"probe-file", L"prog1", L"prog2", L"progn", L"provide",
    L"read", L"read-char", L"read-line", L"reduce", L"remove-if", L"remove-if-not",
    L"rename-file", L"require", L"restart-case", L"return", L"return-from", L"sort",
    L"stable-sort", L"tagbody", L"terpri", L"throw", L"unless", L"unwind-protect",
    L"use-package", L"values", L"when", L"with-input-from-string", L"with-open-file",
    L"with-output-to-string", L"write",
};

constexpr keyword_set kLispKeywords[] = {
    {open_paren, word, kLispWords0, 24},
};

// ---- MSIL ----

constexpr delimited_rule kMSILDelimiters[] = {
    {comment, L"//", L"", L"", 0, false, false, false, false, false},
    {comment, L"/*", L"*/", L"", 0, false, true, false, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
};

constexpr std::wstring_view kMSILWords0[] = {
    L".assembly", L".class", L".entrypoint", L".event", L".field", L".locals", L".maxstack",
    L".method", L".module", L".namespace", L".property", L".try", L"abstract", L"add",
    L"add.ovf", L"add.ovf.un", L"and", L"arglist", L"beq", L"beq.s", L"bge", L"bge.s",
    L"bge.un", L"bge.un.s", L"bgt", L"bgt.s", L"bgt.un", L"bgt.un.s", L"ble", L"ble.s",
    L"ble.un", L"ble.un.s", L"blt", L"blt.s", L"blt.un", L"blt.un.s", L"bne.un", L"bne.un.s",
    L"bool", L"box", L"br", L"br.s", L"break", L"brfalse", L"brfalse.s", L"brinst", L"brinst.s",
    L"brnull", L"brnull.s", L"brtrue", L"brtrue.s", L"brzero", L"brzero.s", L"call", L"calli",
    L"callvirt", L"castclass", L"cdecl", L"ceq", L"cgt", L"cgt.un", L"ckfinite", L"clt",
    L"clt.un", L"constrained", L"conv.i", L"conv.i1", L"conv.i2", L"conv.i4", L"conv.i8",
    L"conv.ovf.i", L"conv.ovf.i.un", L"conv.ovf.i1", L"conv.ovf.i1.un", L"conv.ovf.i2",
    L"conv.ovf.i2.un", L"conv.ovf.i4", L"conv.ovf.i4.un", L"conv.ovf.i8", L"conv.ovf.i8.un",
    L"conv.ovf.u", L"conv.ovf.u.un", L"conv.ovf.u1", L"conv.ovf.u1.un", L"conv.ovf.u2",
    L"conv.ovf.u2.un", L"conv.ovf.u4", L"conv.ovf.u4.un", L"conv.ovf.u8", L"conv.ovf.u8.un",
    L"conv.r.un", L"conv.r4", L"conv.r8", L"conv.u", L"conv.u1", L"conv.u2", L"conv.u4",
    L"conv.u8", L"cpblk", L"cpobj", L"div", L"div.un", L"dup", L"endfilter", L"endfinally",
    L"explicit", L"fastcall", L"initblk", L"initobj", L"instance", L"isinst", L"jmp", L"ldarg",
    L"ldarg.0", L"ldarg.1", L"ldarg.2", L"ldarg.3", L"ldarg.s", L"ldarga", L"ldarga.s",
    L"ldc.i4", L"ldc.i4.0", L"ldc.i4.1", L"ldc.i4.2", L"ldc.i4.3", L"ldc.i4.4", L"ldc.i4.5",
    L"ldc.i4.6", L"ldc.i4.7", L"ldc.i4.8", L"ldc.i4.m1", L"ldc.i4.s", L"ldc.i8", L"ldc.r4",
    L"ldc.r8", L"ldelem", L"ldelem.i", L"ldelem.i1", L"ldelem.i2", L"ldelem.i4", L"ldelem.i8",
    L"ldelem.r4", L"ldelem.r8", L"ldelem.ref", L"ldelem.u1", L"ldelem.u2", L"ldelem.u4",
    L"ldelem.u8", L"ldelema", L"ldftn", L"ldind.i", L"ldind.i1", L"ldind.i2", L"ldind.i4",
    L"ldind.i8", L"ldind.r4", L"ldind.r8", L"ldind.ref", L"ldind.u1", L"ldind.u2", L"ldind.u4",
    L"ldind.u8", L"ldlen", L"ldloc", L"ldloc.0", L"ldloc.1", L"ldloc.2", L"ldloc.3", L"ldloc.s",
    L"ldloca", L"ldloca.s", L"ldnull", L"ldobj", L"ldsfld", L"ldsflda", L"ldstr", L"ldtoken",
    L"ldvirtftn", L"leave", L"leave.s", L"localloc", L"mkrefany", L"mul", L"mul.ovf",
    L"mul.ovf.un", L"native float", L"native int", L"native unsigned int", L"neg", L"newarr",
    L"newobj", L"no", L"nop", L"not", L"object", L"or", L"override", L"pop", L"private",
    L"public", L"readonly", L"refanytype", L"refanyval", L"rem", L"rem.un", L"ret", L"rethrow",
    L"sealed", L"shl", L"shr", L"shr.un", L"sizeof", L"starg", L"starg.s", L"static",
    L"stdcall", L"stelem", L"stelem.i", L"stelem.i1", L"stelem.i2", L"stelem.i4", L"stelem.i8",
    L"stelem.r4", L"stelem.r8", L"stelem.ref", L"stelem.u1", L"stelem.u2", L"stelem.u4",
    L"stelem.u8", L"stind.i", L"stind.i1", L"stind.i2", L"stind.i4", L"stind.i8", L"stind.r4",
    L"stind.r8", L"stind.ref", L"stloc", L"stloc.0", L"stloc.1", L"stloc.2", L"stloc.3",
    L"stloc.s", L"stobj", L"string", L"stsfld", L"sub", L"sub.ovf", L"sub.ovf.un", L"switch",
    L"tail", L"thiscall", L"throw", L"unaligned", L"unbox", L"unbox.any", L"valuetype",
    L"vararg", L"virtual", L"void", L"volatile", L"xor",
};

constexpr keyword_set kMSILKeywords[] = {
    {word, word, kMSILWords0, 19},
};

// ---- Nemerle ----

constexpr delimited_rule kNemerleDelimiters[] = {
    {comment, L"//", L"", L"", 0, false, false, false, false, false},
    {comment, L"/*", L"*/", L"", 0, false, true, false, false, false},
    {string, L"@\"", L"\"", L"", 0, true, true, false, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
    {string, L"'", L"'", L"", L'\\', false, false, false, true, false},
};

constexpr std::wstring_view kNemerleWords0[] = {
    L"define", L"elif", L"else", L"endif", L"endregion", L"error", L"if", L"line", L"pragma",
    L"region", L"undef", L"warning",
};

constexpr std::wstring_view kNemerleWords1[] = {
    L"_", L"abstract", L"add", L"as", L"assembly", L"await", L"base", L"bool", L"break",
    L"byte", L"catch", L"char", L"checked", L"class", L"continue", L"decimal", L"def",
    L"default", L"delegate", L"do", L"double", L"else", L"enum", L"event", L"extern", L"false",
    L"field", L"finally", L"float", L"for", L"foreach", L"get", L"if", L"in", L"int",
    L"interface", L"internal", L"is", L"keyword", L"lock", L"long", L"macro", L"marker",
    L"match", L"method", L"module", L"mutable", L"namespace", L"new", L"null", L"object",
    L"out", L"override", L"param", L"params", L"partial", L"private", L"protected", L"public",
    L"ref", L"regex", L"remove", L"return", L"sbyte", L"sealed", L"set", L"short", L"span",
    L"static", L"string", L"struct", L"syntax", L"this", L"throw", L"token", L"true", L"try",
    L"type", L"typeof", L"typevar", L"uint", L"ulong", L"unchecked", L"unless", L"ushort",
    L"using", L"value", L"variant", L"virtual", L"void", L"volatile", L"when", L"where",
    L"while", L"with", L"yield",
};

constexpr keyword_set kNemerleKeywords[] = {
    {hash_with_space, word, kNemerleWords0, 9},
    {word, word, kNemerleWords1, 9},
};

// ---- Nitra ----

constexpr delimited_rule kNitraDelimiters[] = {
    {comment, L"//", L"", L"", 0, false, false, false, false, false},
    {comment, L"/*", L"*/", L"", 0, false, true, false, false, false},
    {string, L"@\"", L"\"", L"", 0, true, true, false, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
    {string, L"'", L"'", L"", L'\\', false, false, false, true, false},
};

constexpr std::wstring_view kNitraWords0[] = {
    L"define", L"elif", L"else", L"endif", L"endregion", L"error", L"if", L"line", L"pragma",
    L"region", L"undef", L"warning",
};

constexpr std::wstring_view kNitraWords1[] = {
    L"SpanClass", L"StartRule", L"_", L"abstract", L"add", L"alias", L"as", L"assembly",
    L"await", L"base", L"bool", L"break", L"byte", L"catch", L"char", L"checked", L"class",
    L"company", L"continue", L"decimal", L"declaration", L"declarations", L"def", L"default",
    L"delegate", L"do", L"double", L"else", L"enum", L"event", L"extend", L"extension",
    L"extern", L"false", L"field", L"finally", L"float", L"for", L"foreach", L"get", L"if",
    L"in", L"inout", L"int", L"interface", L"internal", L"is", L"keyword", L"language", L"lock",
    L"long", L"macro", L"map", L"marker", L"match", L"method", L"module", L"mutable",
    L"namespace", L"new", L"null", L"object", L"out", L"override", L"param", L"params",
    L"partial", L"precedence", L"private", L"protected", L"public", L"ref", L"regex", L"remove",
    L"return", L"right-associative", L"rule", L"sbyte", L"sealed", L"set", L"short", L"span",
    L"start", L"static", L"string", L"struct", L"style", L"syntax", L"this", L"throw", L"token",
    L"true", L"try", L"type", L"typeof", L"typevar", L"uint", L"ulong", L"unchecked", L"unless",
    L"ushort", L"using", L"value", L"variant", L"virtual", L"void", L"volatile", L"when",
    L"where", L"while", L"with", L"yield",
};

constexpr keyword_set kNitraKeywords[] = {
    {hash_with_space, word, kNitraWords0, 9},
    {word, word, kNitraWords1, 17},
};

// ---- ObjC ----

constexpr delimited_rule kObjCDelimiters[] = {
    {comment, L"//", L"", L"", 0, false, false, false, false, false},
    {comment, L"/*", L"*/", L"", 0, false, true, false, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
    {string, L"'", L"'", L"", L'\\', false, false, false, false, false},
};

constexpr std::wstring_view kObjCWords0[] = {
    L"@catch", L"@class", L"@defs", L"@dynamic", L"@encode", L"@end", L"@finally",
    L"@implementation", L"@import", L"@interface", L"@optional", L"@private", L"@property",
    L"@protected", L"@protocol", L"@public", L"@required", L"@selector", L"@synchronized",
    L"@synthesize", L"@throw", L"@try", L"BOOL", L"Class", L"IMP", L"NO", L"NULL", L"Nil",
    L"SEL", L"YES", L"_Bool", L"_Complex", L"_Imaginary", L"assign", L"atomic", L"auto",
    L"break", L"case", L"char", L"const", L"continue", L"copy", L"default", L"do", L"double",
    L"else", L"enum", L"extern", L"float", L"for", L"getter", L"goto", L"id", L"if", L"inline",
    L"int", L"long", L"nil", L"nonatomic", L"readonly", L"readwrite", L"register", L"restrict",
    L"retain", L"return", L"self", L"setter", L"short", L"signed", L"sizeof", L"static",
    L"strong", L"struct", L"super", L"switch", L"typedef", L"union", L"unsafe_unretained",
    L"unsigned", L"void", L"volatile", L"weak", L"while",
};

constexpr keyword_set kObjCKeywords[] = {
    {word, word, kObjCWords0, 17},
};

// ---- Ocaml ----

constexpr delimited_rule kOcamlDelimiters[] = {
    {comment, L"(*", L"*)", L"", 0, false, true, false, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
    {string, L"'", L"'", L"", 0, false, false, false, false, false},
};

constexpr std::wstring_view kOcamlWords0[] = {
    L"and", L"as", L"asr", L"assert", L"begin", L"class", L"constraints", L"do", L"done",
    L"downto", L"else", L"end", L"exception", L"external", L"false", L"for", L"fun",
    L"function", L"functor", L"if", L"in", L"include", L"inherit", L"initializer", L"land",
    L"lazy", L"let", L"lor", L"lsl", L"lsr", L"lxor", L"match", L"method", L"mod", L"module",
    L"mutable", L"new", L"nonrec", L"object", L"of", L"open", L"or", L"private", L"rec", L"sig",
    L"struct", L"then", L"to", L"true", L"try", L"type", L"val", L"virtual", L"when", L"while",
    L"with",
};

constexpr keyword_set kOcamlKeywords[] = {
    {word, word, kOcamlWords0, 11},
};

// ---- Pascal ----

constexpr delimited_rule kPascalDelimiters[] = {
    {comment, L"{", L"}", L"", 0, false, true, false, false, false},
    {comment, L"(*", L"*)", L"", 0, false, true, false, false, false},
    {comment, L"//", L"", L"", 0, false, false, false, false, false},
    {string, L"'", L"'", L"", 0, true, false, false, false, false},
};

constexpr std::wstring_view kPascalWords0[] = {
    L"absolute", L"abstract", L"and", L"array", L"as", L"asm", L"assembler", L"automated",
    L"begin", L"case", L"cdecl", L"class", L"const", L"constructor", L"contains", L"default",
    L"deprecated", L"destructor", L"dispid", L"dispinterface", L"div", L"do", L"downto",
    L"dynamic", L"else", L"end", L"except", L"export", L"exports", L"external", L"far", L"file",
    L"final", L"finalization", L"finally", L"for", L"forward", L"function", L"goto", L"helper",
    L"if", L"implementation", L"implements", L"in", L"index", L"inherited", L"initialization",
    L"inline", L"interface", L"is", L"label", L"library", L"local", L"message", L"mod", L"name",
    L"near", L"nil", L"nodefault", L"not", L"object", L"of", L"on", L"or", L"out", L"overload",
    L"override", L"package", L"packed", L"pascal", L"platform", L"private", L"procedure",
    L"program", L"property", L"protected", L"public", L"published", L"raise", L"record",
    L"register", L"reintroduce", L"repeat", L"requires", L"resident", L"resourcestring",
    L"safecall", L"sealed", L"set", L"shl", L"shr", L"static", L"stdcall", L"stored", L"strict",
    L"string", L"then", L"threadvar", L"to", L"try", L"type", L"unit", L"unsafe", L"until",
    L"uses", L"var", L"varargs", L"virtual", L"while", L"with", L"xor",
};

constexpr keyword_set kPascalKeywords[] = {
    {word, word, kPascalWords0, 14},
};

// ---- Perl ----

constexpr delimited_rule kPerlDelimiters[] = {
    {comment, L"#", L"", L"", 0, false, false, false, false, false},
    {comment, L"=", L"=cut", L"", 0, false, true, true, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
    {string, L"'", L"'", L"", 0, true, false, false, false, false},
};

constexpr std::wstring_view kPerlWords0[] = {
    L"END", L"bless", L"caller", L"continue", L"dbmclose", L"dbmopen", L"default", L"defined",
    L"delete", L"do", L"each", L"else", L"elsif", L"endgrent", L"endhostent", L"endnetent",
    L"endprotoent", L"endpwent", L"endservent", L"eof", L"eval", L"exec", L"exists", L"exit",
    L"exp", L"fcntl", L"fileno", L"flock", L"for", L"foreach", L"fork", L"format", L"formline",
    L"getc", L"getgrent", L"getgrgid", L"getgrnam", L"gethostbyaddr", L"gethostbyname",
    L"gethostent", L"getlogin", L"getnetbyaddr", L"getnetbyname", L"getnetent", L"getpeername",
    L"getpgrp", L"getppid", L"getpriority", L"getprotobyname", L"getprotobynumber",
    L"getprotoent", L"getpwent", L"getpwnam", L"getpwuid", L"getservbyname", L"getservbyport",
    L"getservent", L"getsockname", L"getsockopt", L"glob", L"gmtime", L"goto", L"grep", L"hex",
    L"import", L"index", L"int", L"ioctl", L"join", L"keys", L"kill", L"last", L"lc",
    L"lcfirst", L"length", L"link", L"listen", L"local", L"localtime", L"log", L"lstat", L"map",
    L"mkdir", L"msgctl", L"msgget", L"msgrcv", L"msgsnd", L"my", L"next", L"no", L"oct",
    L"open", L"opendir", L"ord", L"pack", L"package", L"pipe", L"pop", L"pos", L"print",
    L"printf", L"push", L"quotemeta", L"rand", L"read", L"readdir", L"readline", L"readlink",
    L"recv", L"redo", L"ref", L"rename", L"require", L"reset", L"return", L"reverse",
    L"rewinddir", L"rindex", L"rmdir", L"scalar", L"seek", L"seekdir", L"select", L"semctl",
    L"semget", L"semop", L"send", L"setgrent", L"sethostent", L"setnetent", L"setpgrp",
    L"setpriority", L"setprotoent", L"setpwent", L"setservent", L"setsockopt", L"shift",
    L"shmctl", L"shmget", L"shmread", L"shmwrite", L"shutdown", L"sin", L"sleep", L"socket",
    L"socketpair", L"sort", L"splice", L"split", L"sprintf", L"sqrt", L"srand", L"stat",
    L"study", L"sub", L"substr", L"symlink", L"syscall", L"sysread", L"system", L"syswrite",
    L"tell", L"telldir", L"tie", L"tied", L"time", L"times", L"tr", L"truncate", L"uc",
    L"ucfirst", L"umask", L"undef", L"unless", L"unlink", L"unpack", L"unshift", L"untie",
    L"until", L"use", L"utime", L"values", L"vec", L"wait", L"waitpid", L"wantarray", L"warn",
    L"while", L"write", L"xor",
};

constexpr keyword_set kPerlKeywords[] = {
    {word, word, kPerlWords0, 16},
};

// ---- PHP ----

constexpr delimited_rule kPHPDelimiters[] = {
    {comment, L"//", L"", L"", 0, false, false, false, false, false},
    {comment, L"#", L"", L"", 0, false, false, false, false, false},
    {comment, L"/*", L"*/", L"", 0, false, true, false, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
    {string, L"'", L"'", L"", L'\\', false, false, false, false, false},
};

constexpr std::wstring_view kPHPWords0[] = {
    L"abstract", L"and", L"array", L"as", L"break", L"case", L"catch", L"cfunction", L"class",
    L"clone", L"const", L"continue", L"declare", L"default", L"die", L"do", L"echo", L"else",
    L"elseif", L"empty", L"enddeclare", L"endfor", L"endforeach", L"endif", L"endswitch",
    L"endwhile", L"eval", L"exception", L"exit", L"extends", L"final", L"for", L"foreach",
    L"function", L"global", L"if", L"implements", L"include", L"include_once", L"interface",
    L"isset", L"list", L"new", L"old_function", L"or", L"php_user_filter", L"print", L"private",
    L"protected", L"public", L"require", L"require_once", L"return", L"static", L"switch",
    L"this", L"throw", L"try", L"unset", L"use", L"var", L"while", L"xor",
};

constexpr keyword_set kPHPKeywords[] = {
    {word, word, kPHPWords0, 15},
};

// ---- Prolog ----

constexpr delimited_rule kPrologDelimiters[] = {
    {comment, L"%", L"", L"", 0, false, false, false, false, false},
    {comment, L"/*", L"*/", L"", 0, false, true, false, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
};

constexpr std::wstring_view kPrologWords0[] = {
    L"abolish", L"abort", L"arg", L"asserta", L"assertz", L"atom", L"atomic", L"bagof",
    L"break", L"call", L"case", L"catch", L"clause", L"close", L"concat", L"consult",
    L"dynamic", L"else", L"end", L"fail", L"false", L"findall", L"float", L"flush_output",
    L"forall", L"functor", L"get", L"halt", L"if", L"integer", L"is", L"length", L"listing",
    L"mod", L"multifile", L"nl", L"nonvar", L"not", L"notrace", L"number", L"once", L"op",
    L"open", L"peek_char", L"peek_code", L"put", L"read", L"repeat", L"retract", L"retractall",
    L"see", L"seeing", L"seen", L"setof", L"skip", L"static", L"sub_atom", L"tell", L"telling",
    L"term", L"throw", L"trace", L"true", L"ttyflush", L"unify_with_occurs_check", L"unknown",
    L"var", L"write",
};

constexpr keyword_set kPrologKeywords[] = {
    {word, word, kPrologWords0, 23},
};

// ---- Python ----

constexpr delimited_rule kPythonDelimiters[] = {
    {comment, L"#", L"", L"", 0, false, false, false, false, false},
    {comment, L"\"\"\"", L"\"\"\"", L"uUrR", 0, false, true, false, false, false},
    {comment, L"'''", L"'''", L"uUrR", 0, false, true, false, false, false},
    {string, L"\"", L"\"", L"uUrR", L'\\', false, false, false, false, false},
    {string, L"'", L"'", L"uUrR", L'\\', false, false, false, false, false},
};

constexpr std::wstring_view kPythonWords0[] = {
    L"and", L"assert", L"break", L"class", L"continue", L"def", L"del", L"elif", L"else",
    L"except", L"exec", L"finally", L"for", L"from", L"global", L"if", L"import", L"in", L"is",
    L"not", L"or", L"pass", L"print", L"raise", L"return", L"try", L"while", L"yield",
};

constexpr keyword_set kPythonKeywords[] = {
    {word, word, kPythonWords0, 8},
};

// ---- Ruby ----

constexpr delimited_rule kRubyDelimiters[] = {
    {comment, L"#", L"", L"", 0, false, false, false, false, false},
    {comment, L"=begin", L"=end", L"", 0, false, true, true, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
    {string, L"'", L"'", L"", L'\\', false, false, false, false, false},
};

constexpr std::wstring_view kRubyWords0[] = {
    L"BEGIN", L"END", L"alias", L"and", L"begin", L"break", L"case", L"class", L"def",
    L"defined", L"do", L"else", L"elsif", L"end", L"ensure", L"false", L"for", L"if", L"in",
    L"module", L"next", L"nil", L"not", L"or", L"redo", L"rescue", L"retry", L"return", L"self",
    L"super", L"then", L"true", L"undef", L"unless", L"until", L"when", L"while", L"yield",
};

constexpr keyword_set kRubyKeywords[] = {
    {word, word, kRubyWords0, 7},
};

// ---- Rust ----

constexpr delimited_rule kRustDelimiters[] = {
    {comment, L"//", L"", L"", 0, false, false, false, false, false},
    {comment, L"/*", L"*/", L"", 0, false, true, false, false, false},
    {string, L"@\"", L"\"", L"", 0, true, true, false, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
    {string, L"'", L"'", L"", L'\\', false, false, false, true, false},
};

constexpr std::wstring_view kRustWords0[] = {
    L"assert", L"fail",
};

constexpr std::wstring_view kRustWords1[] = {
    L"Self", L"as", L"bool", L"break", L"char", L"const", L"continue", L"do", L"enum",
    L"extern", L"f32", L"f64", L"false", L"float", L"fn", L"for", L"i16", L"i32", L"i64", L"i8",
    L"if", L"impl", L"in", L"int", L"let", L"loop", L"mod", L"mut", L"once", L"priv", L"proc",
    L"pub", L"ref", L"return", L"static", L"str", L"struct", L"trait", L"true", L"type", L"u16",
    L"u32", L"u64", L"u8", L"uint", L"unsafe", L"use", L"while",
};

constexpr keyword_set kRustKeywords[] = {
    {word, exclamation_and_word, kRustWords0, 6},
    {word, word, kRustWords1, 8},
};

// ---- SQL ----

constexpr delimited_rule kSQLDelimiters[] = {
    {comment, L"--", L"", L"", 0, false, false, false, false, false},
    {comment, L"/*", L"*/", L"", 0, false, true, false, false, false},
    {string, L"'", L"'", L"", 0, true, false, false, false, false},
};

constexpr std::wstring_view kSQLWords0[] = {
    L"add", L"all", L"alter", L"and", L"any", L"as", L"asc", L"authorization", L"backup",
    L"begin", L"between", L"break", L"browse", L"bulk", L"by", L"cascade", L"case", L"check",
    L"checkpoint", L"close", L"clustered", L"coalesce", L"collate", L"column", L"commit",
    L"compute", L"constraint", L"contains", L"containstable", L"continue", L"convert", L"count",
    L"create", L"cross", L"current", L"current_date", L"current_time", L"current_timestamp",
    L"current_user", L"cursor", L"database", L"dbcc", L"deallocate", L"declare", L"default",
    L"delete", L"deny", L"desc", L"disk", L"distinct", L"distributed", L"double", L"drop",
    L"dummy", L"dump", L"else", L"end", L"errlvl", L"escape", L"except", L"exec", L"execute",
    L"exists", L"exit", L"fetch", L"file", L"fillfactor", L"for", L"foreign", L"freetext",
    L"freetexttable", L"from", L"full", L"function", L"goto", L"grant", L"group", L"having",
    L"holdlock", L"identity", L"identity_insert", L"identitycol", L"if", L"in", L"index",
    L"inner", L"insert", L"intersect", L"into", L"is", L"join", L"key", L"kill", L"left",
    L"like", L"lineno", L"load", L"local", L"national", L"nocheck", L"nonclustered", L"not",
    L"null", L"nullif", L"of", L"off", L"offsets", L"on", L"open", L"opendatasource",
    L"openquery", L"openrowset", L"openxml", L"option", L"or", L"order", L"outer", L"over",
    L"percent", L"plan", L"precision", L"primary", L"print", L"proc", L"procedure", L"public",
    L"raiserror", L"read", L"readtext", L"reconfigure", L"references", L"replication",
    L"restore", L"restrict", L"return", L"revoke", L"right", L"rollback", L"rowcount",
    L"rowguidcol", L"rule", L"save", L"schema", L"select", L"session_user", L"set", L"setuser",
    L"shutdown", L"some", L"statistics", L"system_user", L"table", L"textsize", L"then", L"to",
    L"top", L"tran", L"transaction", L"trigger", L"truncate", L"tsequal", L"union", L"unique",
    L"update", L"updatetext", L"use", L"user", L"values", L"varying", L"view", L"waitfor",
    L"when", L"where", L"while", L"with", L"writetext",
};

constexpr keyword_set kSQLKeywords[] = {
    {word, word, kSQLWords0, 17},
};

// ---- VisualBasic ----

constexpr delimited_rule kVisualBasicDelimiters[] = {
    {comment, L"'", L"", L"", 0, false, false, false, false, false},
    {string, L"\"", L"\"", L"", 0, true, false, false, false, false},
};

constexpr std::wstring_view kVisualBasicWords0[] = {
    L"addhandler", L"addressof", L"alias", L"and", L"andalso", L"ansi", L"as", L"assembly",
    L"auto", L"byref", L"byval", L"call", L"case", L"catch", L"class", L"const", L"continue",
    L"declare", L"default", L"delegate", L"dim", L"directcast", L"do", L"each", L"else",
    L"elseif", L"end", L"enum", L"erase", L"error", L"event", L"exit", L"finally", L"for",
    L"friend", L"function", L"get", L"gettype", L"gosub", L"goto", L"handles", L"if",
    L"implements", L"imports", L"in", L"inherits", L"interface", L"is", L"let", L"lib", L"like",
    L"loop", L"me", L"mod", L"mustinherit", L"mustoverride", L"mybase", L"myclass",
    L"namespace", L"new", L"next", L"not", L"nothing", L"notinheritable", L"notoverridable",
    L"object", L"on", L"option", L"optional", L"or", L"orelse", L"overloads", L"overridable",
    L"overrides", L"paramarray", L"preserve", L"private", L"property", L"protected", L"public",
    L"raiseevent", L"readonly", L"redim", L"rem", L"removehandler", L"resume", L"return",
    L"select", L"set", L"shadows", L"shared", L"single", L"static", L"step", L"stop", L"string",
    L"structure", L"sub", L"synclock", L"then", L"throw", L"to", L"try", L"typeof", L"unicode",
    L"until", L"variant", L"wend", L"when", L"while", L"with", L"withevents", L"writeonly",
    L"xor",
};

constexpr keyword_set kVisualBasicKeywords[] = {
    {word, word, kVisualBasicWords0, 14},
};

// ---- XSL ----

constexpr delimited_rule kXSLDelimiters[] = {
    {comment, L"<!--", L"-->", L"", 0, false, true, false, false, false},
    {string, L"\"", L"\"", L"", 0, false, false, false, false, false},
};

constexpr std::wstring_view kXSLWords0[] = {
    L"?xml",
};

constexpr std::wstring_view kXSLWords1[] = {
    L"case-order", L"current", L"data-type", L"disable-output-escaping", L"document",
    L"elements", L"encoding", L"format-number", L"generate-id", L"indent", L"key", L"lang",
    L"match", L"method", L"mode", L"name", L"namespace", L"order", L"priority",
    L"result-prefix", L"select", L"stylesheet-prefix", L"system-property", L"terminate",
    L"test", L"unparsed-entity-uri", L"use", L"use-attribute-sets", L"version", L"xmlns:xsl",
    L"xsl:analyze-string", L"xsl:apply-imports", L"xsl:apply-templates", L"xsl:attribute",
    L"xsl:attribute-set", L"xsl:call-template", L"xsl:character-map", L"xsl:choose",
    L"xsl:comment", L"xsl:copy", L"xsl:copy-of", L"xsl:decimal-format", L"xsl:document",
    L"xsl:element", L"xsl:fallback", L"xsl:for-each", L"xsl:for-each-group", L"xsl:function",
    L"xsl:if", L"xsl:import", L"xsl:import-schema", L"xsl:include", L"xsl:key",
    L"xsl:matching-substring", L"xsl:message", L"xsl:namespace", L"xsl:namespace-alias",
    L"xsl:next-match", L"xsl:non-matching-substring", L"xsl:number", L"xsl:otherwise",
    L"xsl:output", L"xsl:output-character", L"xsl:param", L"xsl:perform-sort",
    L"xsl:preserve-space", L"xsl:processing-instructions", L"xsl:result-document",
    L"xsl:script", L"xsl:sequence", L"xsl:sort", L"xsl:strip-space", L"xsl:stylesheet",
    L"xsl:template", L"xsl:text", L"xsl:transform", L"xsl:value-of", L"xsl:variable",
    L"xsl:when", L"xsl:with-param",
};

constexpr keyword_set kXSLKeywords[] = {
    {none, word, kXSLWords0, 4},
    {word, word, kXSLWords1, 27},
};

// ---- Bash ----

constexpr delimited_rule kBashDelimiters[] = {
    {comment, L"#", L"", L"", 0, false, false, false, false, true},
    {string, L"\"", L"\"", L"", L'\\', false, true, false, false, false},
    {string, L"'", L"'", L"", 0, false, true, false, false, false},
};

constexpr std::wstring_view kBashWords0[] = {
    L"alias", L"bg", L"bind", L"break", L"builtin", L"case", L"cd", L"command", L"continue",
    L"coproc", L"declare", L"do", L"done", L"echo", L"elif", L"else", L"esac", L"eval", L"exec",
    L"exit", L"export", L"false", L"fg", L"fi", L"for", L"function", L"getopts", L"hash", L"if",
    L"in", L"jobs", L"kill", L"let", L"local", L"mapfile", L"popd", L"printf", L"pushd", L"pwd",
    L"read", L"readarray", L"readonly", L"return", L"select", L"set", L"shift", L"shopt",
    L"source", L"test", L"then", L"time", L"trap", L"true", L"type", L"typeset", L"ulimit",
    L"umask", L"unalias", L"unset", L"until", L"wait", L"while",
};

constexpr keyword_set kBashKeywords[] = {
    {word, word, kBashWords0, 9},
};

// ---- CMake ----

constexpr delimited_rule kCMakeDelimiters[] = {
    {comment, L"#[[", L"]]", L"", 0, false, true, false, false, true},
    {comment, L"#", L"", L"", 0, false, false, false, false, true},
    {string, L"\"", L"\"", L"", L'\\', false, true, false, false, false},
};

constexpr std::wstring_view kCMakeWords0[] = {
    L"add_compile_definitions", L"add_compile_options", L"add_custom_command",
    L"add_custom_target", L"add_definitions", L"add_dependencies", L"add_executable",
    L"add_library", L"add_link_options", L"add_subdirectory", L"add_test", L"and",
    L"aux_source_directory", L"block", L"break", L"cmake_language", L"cmake_minimum_required",
    L"cmake_parse_arguments", L"cmake_path", L"cmake_policy", L"configure_file", L"continue",
    L"defined", L"else", L"elseif", L"enable_language", L"enable_testing", L"endblock",
    L"endforeach", L"endfunction", L"endif", L"endmacro", L"endwhile", L"equal",
    L"execute_process", L"exists", L"export", L"file", L"find_file", L"find_library",
    L"find_package", L"find_path", L"find_program", L"foreach", L"function",
    L"get_cmake_property", L"get_directory_property", L"get_filename_component",
    L"get_property", L"get_source_file_property", L"get_target_property", L"get_test_property",
    L"greater", L"greater_equal", L"if", L"in", L"include", L"include_directories",
    L"include_guard", L"install", L"is_directory", L"less", L"less_equal", L"link_directories",
    L"link_libraries", L"list", L"macro", L"mark_as_advanced", L"matches", L"math", L"message",
    L"not", L"option", L"or", L"project", L"remove_definitions", L"return",
    L"separate_arguments", L"set", L"set_directory_properties", L"set_property",
    L"set_source_files_properties", L"set_target_properties", L"set_tests_properties",
    L"site_name", L"source_group", L"strequal", L"string", L"target_compile_definitions",
    L"target_compile_features", L"target_compile_options", L"target_include_directories",
    L"target_link_directories", L"target_link_libraries", L"target_link_options",
    L"target_precompile_headers", L"target_sources", L"try_compile", L"try_run", L"unset",
    L"variable_watch", L"version_equal", L"version_greater", L"version_less", L"while",
    L"write_file",
};

constexpr keyword_set kCMakeKeywords[] = {
    {word, word, kCMakeWords0, 27},
};

// ---- CSS ----

constexpr delimited_rule kCSSDelimiters[] = {
    {comment, L"/*", L"*/", L"", 0, false, true, false, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
    {string, L"'", L"'", L"", L'\\', false, false, false, false, false},
};

constexpr std::wstring_view kCSSWords0[] = {
    L"@charset", L"@container", L"@font-face", L"@import", L"@keyframes", L"@layer", L"@media",
    L"@namespace", L"@page", L"@property", L"@supports",
};

constexpr std::wstring_view kCSSWords1[] = {
    L"align-content", L"align-items", L"align-self", L"animation", L"animation-delay",
    L"animation-direction", L"animation-duration", L"animation-fill-mode",
    L"animation-iteration-count", L"animation-name", L"animation-play-state",
    L"animation-timing-function", L"backdrop-filter", L"background", L"background-attachment",
    L"background-clip", L"background-color", L"background-image", L"background-origin",
    L"background-position", L"background-repeat", L"background-size", L"border",
    L"border-bottom", L"border-collapse", L"border-color", L"border-image", L"border-left",
    L"border-radius", L"border-right", L"border-spacing", L"border-style", L"border-top",
    L"border-width", L"bottom", L"box-shadow", L"box-sizing", L"caption-side", L"caret-color",
    L"clear", L"clip", L"clip-path", L"color", L"column-count", L"column-gap", L"columns",
    L"content", L"cursor", L"direction", L"display", L"empty-cells", L"filter", L"flex",
    L"flex-basis", L"flex-direction", L"flex-flow", L"flex-grow", L"flex-shrink", L"flex-wrap",
    L"float", L"font", L"font-family", L"font-feature-settings", L"font-size", L"font-style",
    L"font-variant", L"font-weight", L"gap", L"grid", L"grid-area", L"grid-auto-columns",
    L"grid-auto-flow", L"grid-auto-rows", L"grid-column", L"grid-gap", L"grid-row",
    L"grid-template", L"grid-template-areas", L"grid-template-columns", L"grid-template-rows",
    L"height", L"justify-content", L"justify-items", L"justify-self", L"left",
    L"letter-spacing", L"line-height", L"list-style", L"list-style-image",
    L"list-style-position", L"list-style-type", L"margin", L"margin-bottom", L"margin-left",
    L"margin-right", L"margin-top", L"max-height", L"max-width", L"min-height", L"min-width",
    L"mix-blend-mode", L"object-fit", L"object-position", L"opacity", L"order", L"outline",
    L"outline-color", L"outline-offset", L"outline-style", L"outline-width", L"overflow",
    L"overflow-x", L"overflow-y", L"padding", L"padding-bottom", L"padding-left",
    L"padding-right", L"padding-top", L"place-content", L"place-items", L"place-self",
    L"pointer-events", L"position", L"quotes", L"resize", L"right", L"row-gap",
    L"scroll-behavior", L"table-layout", L"text-align", L"text-decoration", L"text-indent",
    L"text-overflow", L"text-shadow", L"text-transform", L"top", L"transform",
    L"transform-origin", L"transition", L"transition-delay", L"transition-duration",
    L"transition-property", L"transition-timing-function", L"user-select", L"vertical-align",
    L"visibility", L"white-space", L"width", L"will-change", L"word-break", L"word-spacing",
    L"word-wrap", L"writing-mode", L"z-index",
};

constexpr keyword_set kCSSKeywords[] = {
    {none, word, kCSSWords0, 10},
    {word, word, kCSSWords1, 26},
};

// ---- Go ----

constexpr delimited_rule kGoDelimiters[] = {
    {comment, L"//", L"", L"", 0, false, false, false, false, false},
    {comment, L"/*", L"*/", L"", 0, false, true, false, false, false},
    {string, L"`", L"`", L"", 0, false, true, false, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
    {string, L"'", L"'", L"", L'\\', false, false, false, true, false},
};

constexpr std::wstring_view kGoWords0[] = {
    L"any", L"append", L"bool", L"break", L"byte", L"cap", L"case", L"chan", L"clear", L"close",
    L"complex", L"complex128", L"complex64", L"const", L"continue", L"copy", L"default",
    L"defer", L"delete", L"else", L"error", L"fallthrough", L"false", L"float32", L"float64",
    L"for", L"func", L"go", L"goto", L"if", L"imag", L"import", L"int", L"int16", L"int32",
    L"int64", L"int8", L"interface", L"iota", L"len", L"make", L"map", L"max", L"min", L"new",
    L"nil", L"package", L"panic", L"print", L"println", L"range", L"real", L"recover",
    L"return", L"rune", L"select", L"string", L"struct", L"switch", L"true", L"type", L"uint",
    L"uint16", L"uint32", L"uint64", L"uint8", L"uintptr", L"var",
};

constexpr keyword_set kGoKeywords[] = {
    {word, word, kGoWords0, 11},
};

// ---- JavaScript ----

constexpr delimited_rule kJavaScriptDelimiters[] = {
    {comment, L"//", L"", L"", 0, false, false, false, false, false},
    {comment, L"/*", L"*/", L"", 0, false, true, false, false, false},
    {string, L"`", L"`", L"", L'\\', false, true, false, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
    {string, L"'", L"'", L"", L'\\', false, false, false, false, false},
};

constexpr std::wstring_view kJavaScriptWords0[] = {
    L"Infinity", L"NaN", L"arguments", L"as", L"async", L"await", L"break", L"case", L"catch",
    L"class", L"const", L"constructor", L"continue", L"debugger", L"default", L"delete", L"do",
    L"else", L"enum", L"export", L"extends", L"false", L"finally", L"for", L"from", L"function",
    L"get", L"globalThis", L"if", L"implements", L"import", L"in", L"instanceof", L"interface",
    L"let", L"new", L"null", L"of", L"package", L"private", L"protected", L"public", L"return",
    L"set", L"static", L"super", L"switch", L"this", L"throw", L"true", L"try", L"typeof",
    L"undefined", L"var", L"void", L"while", L"with", L"yield",
};

constexpr keyword_set kJavaScriptKeywords[] = {
    {word, word, kJavaScriptWords0, 11},
};

// ---- JSON ----

constexpr delimited_rule kJSONDelimiters[] = {
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
};

constexpr std::wstring_view kJSONWords0[] = {
    L"false", L"null", L"true",
};

constexpr keyword_set kJSONKeywords[] = {
    {word, word, kJSONWords0, 5},
};

// ---- Lua ----

constexpr delimited_rule kLuaDelimiters[] = {
    {comment, L"--[[", L"]]", L"", 0, false, true, false, false, false},
    {comment, L"--", L"", L"", 0, false, false, false, false, false},
    {string, L"[[", L"]]", L"", 0, false, true, false, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
    {string, L"'", L"'", L"", L'\\', false, false, false, false, false},
};

constexpr std::wstring_view kLuaWords0[] = {
    L"_ENV", L"_G", L"and", L"assert", L"break", L"collectgarbage", L"coroutine", L"debug",
    L"do", L"dofile", L"else", L"elseif", L"end", L"error", L"false", L"for", L"function",
    L"getmetatable", L"goto", L"if", L"in", L"io", L"ipairs", L"load", L"local", L"math",
    L"next", L"nil", L"not", L"or", L"os", L"package", L"pairs", L"pcall", L"print",
    L"rawequal", L"rawget", L"rawlen", L"rawset", L"repeat", L"require", L"return", L"select",
    L"self", L"setmetatable", L"string", L"table", L"then", L"tonumber", L"tostring", L"true",
    L"type", L"until", L"while", L"xpcall",
};

constexpr keyword_set kLuaKeywords[] = {
    {word, word, kLuaWords0, 14},
};

// ---- TypeScript ----

constexpr delimited_rule kTypeScriptDelimiters[] = {
    {comment, L"//", L"", L"", 0, false, false, false, false, false},
    {comment, L"/*", L"*/", L"", 0, false, true, false, false, false},
    {string, L"`", L"`", L"", L'\\', false, true, false, false, false},
    {string, L"\"", L"\"", L"", L'\\', false, false, false, false, false},
    {string, L"'", L"'", L"", L'\\', false, false, false, false, false},
};

constexpr std::wstring_view kTypeScriptWords0[] = {
    L"Infinity", L"NaN", L"abstract", L"any", L"arguments", L"as", L"asserts", L"async",
    L"await", L"bigint", L"boolean", L"break", L"case", L"catch", L"class", L"const",
    L"constructor", L"continue", L"debugger", L"declare", L"default", L"delete", L"do", L"else",
    L"enum", L"export", L"extends", L"false", L"finally", L"for", L"from", L"function", L"get",
    L"globalThis", L"if", L"implements", L"import", L"in", L"infer", L"instanceof",
    L"interface", L"is", L"keyof", L"let", L"module", L"namespace", L"never", L"new", L"null",
    L"number", L"object", L"of", L"override", L"package", L"private", L"protected", L"public",
    L"readonly", L"require", L"return", L"satisfies", L"set", L"static", L"string", L"super",
    L"switch", L"symbol", L"this", L"throw", L"true", L"try", L"type", L"typeof", L"undefined",
    L"unique", L"unknown", L"var", L"void", L"while", L"with", L"yield",
};

constexpr keyword_set kTypeScriptKeywords[] = {
    {word, word, kTypeScriptWords0, 11},
};

// ---- все языки ----

constexpr language kLanguages[] = {
    {L"Assembler", true, kAssemblerDelimiters, kAssemblerKeywords},
    {L"C", false, kCDelimiters, kCKeywords},
    {L"CSharp", false, kCSharpDelimiters, kCSharpKeywords},
    {L"Erlang", false, kErlangDelimiters, kErlangKeywords},
    {L"Haskell", false, kHaskellDelimiters, kHaskellKeywords},
    {L"IDL", false, kIDLDelimiters, kIDLKeywords},
    {L"Java", false, kJavaDelimiters, kJavaKeywords},
    {L"Lisp", true, kLispDelimiters, kLispKeywords},
    {L"MSIL", true, kMSILDelimiters, kMSILKeywords},
    {L"Nemerle", false, kNemerleDelimiters, kNemerleKeywords},
    {L"Nitra", false, kNitraDelimiters, kNitraKeywords},
    {L"ObjC", false, kObjCDelimiters, kObjCKeywords},
    {L"Ocaml", false, kOcamlDelimiters, kOcamlKeywords},
    {L"Pascal", true, kPascalDelimiters, kPascalKeywords},
    {L"Perl", false, kPerlDelimiters, kPerlKeywords},
    {L"PHP", false, kPHPDelimiters, kPHPKeywords},
    {L"Prolog", true, kPrologDelimiters, kPrologKeywords},
    {L"Python", false, kPythonDelimiters, kPythonKeywords},
    {L"Ruby", false, kRubyDelimiters, kRubyKeywords},
    {L"Rust", false, kRustDelimiters, kRustKeywords},
    {L"SQL", true, kSQLDelimiters, kSQLKeywords},
    {L"VisualBasic", true, kVisualBasicDelimiters, kVisualBasicKeywords},
    {L"XSL", false, kXSLDelimiters, kXSLKeywords},
    {L"Bash", false, kBashDelimiters, kBashKeywords},
    {L"CMake", true, kCMakeDelimiters, kCMakeKeywords},
    {L"CSS", true, kCSSDelimiters, kCSSKeywords},
    {L"Go", false, kGoDelimiters, kGoKeywords},
    {L"JavaScript", false, kJavaScriptDelimiters, kJavaScriptKeywords},
    {L"JSON", false, kJSONDelimiters, kJSONKeywords},
    {L"Lua", false, kLuaDelimiters, kLuaKeywords},
    {L"TypeScript", false, kTypeScriptDelimiters, kTypeScriptKeywords},
};

struct alias_t {
    std::wstring_view tag;
    std::size_t language;  // индекс в kLanguages
};

// Имена тегов кода в нижнем регистре — какой язык.
constexpr alias_t kAliases[] = {
    {L"asm", 0},  // Assembler
    {L"assembly", 0},  // Assembler
    {L"c", 1},  // C
    {L"cpp", 1},  // C
    {L"c++", 1},  // C
    {L"ccode", 1},  // C
    {L"c#", 2},  // CSharp
    {L"cs", 2},  // CSharp
    {L"csharp", 2},  // CSharp
    {L"cscode", 2},  // CSharp
    {L"erlang", 3},  // Erlang
    {L"erl", 3},  // Erlang
    {L"haskell", 4},  // Haskell
    {L"hs", 4},  // Haskell
    {L"idl", 5},  // IDL
    {L"midl", 5},  // IDL
    {L"java", 6},  // Java
    {L"lisp", 7},  // Lisp
    {L"il", 8},  // MSIL
    {L"msil", 8},  // MSIL
    {L"nemerle", 9},  // Nemerle
    {L"nitra", 10},  // Nitra
    {L"objc", 11},  // ObjC
    {L"objectivec", 11},  // ObjC
    {L"ml", 12},  // Ocaml
    {L"ocaml", 12},  // Ocaml
    {L"pascal", 13},  // Pascal
    {L"delphi", 13},  // Pascal
    {L"perl", 14},  // Perl
    {L"php", 15},  // PHP
    {L"prolog", 16},  // Prolog
    {L"py", 17},  // Python
    {L"python", 17},  // Python
    {L"rb", 18},  // Ruby
    {L"ruby", 18},  // Ruby
    {L"rust", 19},  // Rust
    {L"sql", 20},  // SQL
    {L"vb", 21},  // VisualBasic
    {L"vbnet", 21},  // VisualBasic
    {L"vbcode", 21},  // VisualBasic
    {L"vbscript", 21},  // VisualBasic
    {L"vbs", 21},  // VisualBasic
    {L"xml", 22},  // XSL
    {L"xsl", 22},  // XSL
    {L"html", 22},  // XSL
    {L"bash", 23},  // Bash
    {L"sh", 23},  // Bash
    {L"shell", 23},  // Bash
    {L"cmake", 24},  // CMake
    {L"css", 25},  // CSS
    {L"go", 26},  // Go
    {L"golang", 26},  // Go
    {L"js", 27},  // JavaScript
    {L"javascript", 27},  // JavaScript
    {L"jscript", 27},  // JavaScript
    {L"json", 28},  // JSON
    {L"lua", 29},  // Lua
    {L"ts", 30},  // TypeScript
    {L"typescript", 30},  // TypeScript
};

}  // namespace

const language* find_language(std::wstring_view tag) noexcept {
    // Имя тега короткое; сравнение без учёта регистра ASCII, как у самих
    // тегов разметки ([C#] и [c#] — один язык).
    for (const alias_t& alias : kAliases) {
        if (alias.tag.size() != tag.size()) continue;
        bool same = true;
        for (std::size_t i = 0; i < tag.size() && same; ++i) {
            wchar_t c = tag[i];
            if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c + 32);
            same = c == alias.tag[i];
        }
        if (same) return &kLanguages[alias.language];
    }
    return nullptr;
}

std::span<const language> languages() noexcept { return kLanguages; }

}  // namespace wxl::highlight
