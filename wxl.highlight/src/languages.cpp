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
    {comment, u";", u"", u"", 0, false, false, false, false, false},
    {string, u"'", u"'", u"", 0, true, false, false, false, false},
};

constexpr std::u16string_view kAssemblerWords0[] = {
    u"__sect__", u"aaa", u"aad", u"aam", u"aas", u"abs", u"absolute", u"adc", u"add", u"addps",
    u"addr", u"addss", u"ah", u"al", u"alias", u"and", u"andnps", u"andps", u"arg", u"arpl",
    u"assume", u"at", u"ax", u"basic", u"bh", u"bits", u"bl", u"bound", u"bp", u"bsf", u"bsr",
    u"bswap", u"bt", u"btc", u"btr", u"bts", u"bx", u"byte", u"c", u"call", u"casemap",
    u"catstr", u"cbw", u"cdq", u"ch", u"cl", u"clc", u"cld", u"cli", u"clts", u"cmc", u"cmova",
    u"cmovae", u"cmovb", u"cmovbe", u"cmovc", u"cmove", u"cmovg", u"cmovge", u"cmovl",
    u"cmovle", u"cmovna", u"cmovnae", u"cmovnb", u"cmovnbe", u"cmovnc", u"cmovne", u"cmovng",
    u"cmovnge", u"cmovnl", u"cmovnle", u"cmovno", u"cmovnp", u"cmovns", u"cmovnz", u"cmovo",
    u"cmovp", u"cmovpe", u"cmovpo", u"cmovs", u"cmovz", u"cmp", u"cmpeqps", u"cmpeqss",
    u"cmpleps", u"cmpless", u"cmpltps", u"cmpltss", u"cmpneqps", u"cmpneqss", u"cmpnleps",
    u"cmpnless", u"cmpnltps", u"cmpnltss", u"cmpordps", u"cmpordss", u"cmpps", u"cmpsb",
    u"cmpsd", u"cmpss", u"cmpsw", u"cmpunordps", u"cmpunordss", u"cmpxchg", u"cmpxchg8b",
    u"codeptr", u"codeseg", u"comiss", u"comm", u"comment", u"common", u"compact", u"cpp",
    u"cpuid", u"cs", u"cvtpi2ps", u"cvtps2pi", u"cvtsi2ss", u"cvtss2si", u"cvttps2pi",
    u"cvttss2si", u"cwd", u"cwde", u"cx", u"daa", u"das", u"dataptr", u"db", u"dd", u"dec",
    u"df", u"dh", u"di", u"display", u"div", u"divps", u"divss", u"dl", u"dq", u"ds", u"dt",
    u"dup", u"dw", u"dword", u"dx", u"eax", u"ebp", u"ebx", u"echo", u"ecx", u"edi", u"edx",
    u"elif", u"elseif1", u"elseif2", u"elseifb", u"elseifdef", u"elseifdif", u"elseifdifi",
    u"elseife", u"elseifidn", u"elseifidni", u"elseifnb", u"elseifndef", u"emms", u"emul",
    u"end", u"endm", u"endp", u"ends", u"endstruc", u"enter", u"enterd", u"enterw", u"enum",
    u"eq", u"equ", u"errif", u"errif1", u"errif2", u"errifb", u"errifdef", u"errifdif",
    u"errifdifi", u"errife", u"errifidn", u"errifidni", u"errifnb", u"errifndef", u"es", u"esi",
    u"esp", u"even", u"evendata", u"exitcode", u"exitm", u"export", u"extern", u"externdef",
    u"extrn", u"f2xm1", u"fabs", u"fadd", u"faddp", u"false", u"far", u"far16", u"far32",
    u"fastimul", u"fbld", u"fbstp", u"fchs", u"fclex", u"fcmovb", u"fcmovbe", u"fcmove",
    u"fcmovnb", u"fcmovnbe", u"fcmovne", u"fcmovnu", u"fcmovu", u"fcom", u"fcomi", u"fcomip",
    u"fcomp", u"fcompp", u"fcos", u"fdecstp", u"fdisi", u"fdiv", u"fdivp", u"fdivr", u"fdivrp",
    u"feni", u"ffree", u"fiadd", u"ficom", u"ficomp", u"fidiv", u"fidivr", u"fild", u"fimul",
    u"fincstp", u"finit", u"fist", u"fistp", u"fisub", u"fisubr", u"flat", u"fld", u"fld1",
    u"fldcw", u"fldenv", u"fldenvd", u"fldenvw", u"fldl2e", u"fldl2t", u"fldlg2", u"fldln2",
    u"fldpi", u"fldz", u"flipflag", u"fmul", u"fmulp", u"fnclex", u"fndisi", u"fneni",
    u"fninit", u"fnldenv", u"fnop", u"fnrstor", u"fnsave", u"fnsaved", u"fnsavew", u"fnstcw",
    u"fnstenv", u"fnstenvd", u"fnstenvw", u"fnstsw", u"for", u"forc", u"fortran", u"fpatan",
    u"fprem", u"fprem1", u"fptan", u"frndint", u"frstor", u"frstord", u"frstorw", u"fs",
    u"fsave", u"fsaved", u"fsavew", u"fscale", u"fsetpm", u"fsin", u"fsincos", u"fsqrt", u"fst",
    u"fstcw", u"fstenv", u"fstenvd", u"fstenvw", u"fstp", u"fstsw", u"fsub", u"fsubp", u"fsubr",
    u"fsubrp", u"ftst", u"fucom", u"fucomi", u"fucomip", u"fucomp", u"fucompp", u"fword",
    u"fxam", u"fxch", u"fxrstor", u"fxsave", u"fxtract", u"fyl2x", u"fyl2xp1", u"ge",
    u"getfield", u"global", u"goto", u"group", u"gs", u"high", u"hlt", u"huge", u"ideal",
    u"idiv", u"iend", u"if0", u"if1", u"if2", u"ifb", u"ifdef", u"ifdif", u"ifdifi", u"ifdifs",
    u"ife", u"ifeq", u"ifidn", u"ifidni", u"iflow", u"ifnb", u"ifndef", u"ifneq", u"ifnidn",
    u"import", u"imul", u"in", u"inc", u"incbin", u"include", u"includelib", u"insb", u"insd",
    u"instr", u"insw", u"int", u"int1", u"int3", u"into", u"invd", u"invlpg", u"invoke",
    u"iret", u"iretd", u"iretdf", u"iretf", u"iretw", u"irp", u"irpc", u"istruc", u"ja", u"jae",
    u"jb", u"jbe", u"jc", u"jcxz", u"je", u"jecxz", u"jg", u"jge", u"jl", u"jle", u"jmp",
    u"jna", u"jnae", u"jnb", u"jnbe", u"jnc", u"jne", u"jng", u"jnge", u"jnl", u"jnle", u"jno",
    u"jnp", u"jns", u"jnz", u"jo", u"jp", u"jpe", u"jpo", u"js", u"jz", u"label", u"lahf",
    u"lar", u"large", u"largestack", u"ldmxcsr", u"lds", u"le", u"lea", u"leave", u"leaved",
    u"leavew", u"length", u"les", u"lfs", u"lgdt", u"lgs", u"lidt", u"lldt", u"lmsw", u"local",
    u"locals", u"lodsb", u"lodsd", u"lodsw", u"loop", u"loope", u"looped", u"loopew", u"loopne",
    u"loopned", u"loopnew", u"loopnz", u"loopnzd", u"loopnzw", u"loopz", u"loopzd", u"loopzw",
    u"low", u"lsl", u"lss", u"ltr", u"macro", u"mask", u"maskflag", u"maskmovq", u"masm",
    u"masm51", u"maxps", u"maxss", u"medium", u"memory", u"method", u"minps", u"minss", u"mov",
    u"movaps", u"movd", u"movhlps", u"movhps", u"movlhps", u"movlps", u"movmskps", u"movntps",
    u"movntq", u"movq", u"movs", u"movsb", u"movsd", u"movss", u"movsw", u"movsx", u"movups",
    u"movzx", u"mul", u"mulps", u"mulss", u"multerrs", u"name", u"ne", u"near", u"near16",
    u"near32", u"neg", u"noemul", u"nojumps", u"nolanguage", u"nolocals", u"nomasm51",
    u"nomulterrs", u"none", u"nop", u"normal", u"nosmart", u"not", u"nothing", u"nowarn",
    u"oddfar", u"oddnear", u"offset", u"option", u"or", u"org", u"orps", u"out", u"outsb",
    u"outsd", u"outsw", u"overflow?", u"packssdw", u"packsswb", u"packuswb", u"paddb", u"paddd",
    u"paddsb", u"paddsw", u"paddusb", u"paddusw", u"paddw", u"page", u"pand", u"pandn", u"para",
    u"parity?", u"pascal", u"pavgb", u"pavgw", u"pcmpeqb", u"pcmpeqd", u"pcmpeqw", u"pcmpgtb",
    u"pcmpgtd", u"pcmpgtw", u"pextrw", u"pinsrw", u"pmaddwd", u"pmaxsw", u"pmaxub", u"pminsw",
    u"pminub", u"pmmx", u"pmovmskb", u"pmulhuw", u"pmulhw", u"pmullw", u"pnommx", u"pop",
    u"popa", u"popad", u"popaw", u"popf", u"popfd", u"popfw", u"popstate", u"por",
    u"prefetchnta", u"prefetcht0", u"prefetcht1", u"prefetcht2", u"private", u"proc",
    u"procdesc", u"proctype", u"prolog", u"proto", u"psadbw", u"pshufw", u"pslld", u"psllq",
    u"psllw", u"psrad", u"psraw", u"psrld", u"psrlq", u"psrlw", u"psubb", u"psubd", u"psubsb",
    u"psubsw", u"psubusb", u"psubusw", u"psubw", u"ptr", u"public", u"publicdll", u"punpckhbw",
    u"punpckhdq", u"punpckhwd", u"punpcklbw", u"punpckldq", u"punpcklwd", u"purge", u"push",
    u"pusha", u"pushad", u"pushaw", u"pushd", u"pushf", u"pushfd", u"pushfw", u"pushstate",
    u"pushw", u"pword", u"pxor", u"quirks", u"qword", u"rcl", u"rcpps", u"rcpss", u"rcr",
    u"rdmsr", u"rdpmc", u"rdtsc", u"real10", u"real4", u"real8", u"record", u"rep", u"repe",
    u"repeat", u"repne", u"repnz", u"rept", u"repz", u"resb", u"resd", u"resq", u"rest",
    u"resw", u"ret", u"retcode", u"retf", u"retn", u"returns", u"rol", u"ror", u"rsm",
    u"rsqrtps", u"rsqrtss", u"sahf", u"sar", u"sbb", u"sbyte", u"scasb", u"scasd", u"scasw",
    u"sdword", u"section", u"seg", u"segment", u"seta", u"setae", u"setb", u"setbe", u"setc",
    u"sete", u"setfield", u"setflag", u"setg", u"setge", u"setl", u"setle", u"setna", u"setnae",
    u"setnb", u"setnbe", u"setnc", u"setne", u"setng", u"setnge", u"setnl", u"setnle", u"setno",
    u"setnp", u"setns", u"setnz", u"seto", u"setp", u"setpe", u"setpo", u"sets", u"setz",
    u"sfence", u"sgdt", u"shl", u"shld", u"short", u"shr", u"shrd", u"shufps", u"si", u"sidt",
    u"sign", u"size", u"sizestr", u"sldt", u"small", u"smart", u"smsw", u"sp", u"sqrtps",
    u"sqrtss", u"ss", u"startupcode", u"stc", u"std", u"stdcall", u"sti", u"stmxcsr", u"stos",
    u"stosb", u"stosd", u"stosw", u"str", u"struc", u"struct", u"sub", u"subps", u"subss",
    u"substr", u"subtitle", u"subttl", u"sword", u"symtype", u"syscall", u"sysenter",
    u"sysexit", u"sysret", u"table", u"tblinit", u"tblptr", u"tbyte", u"tchuge", u"test",
    u"testflag", u"textequ", u"this", u"times", u"tiny", u"title", u"tpascal", u"true",
    u"tword", u"typedef", u"ucomiss", u"ud2", u"unicode", u"union", u"unknown", u"unpckhps",
    u"unpcklps", u"uppercase", u"use16", u"use32", u"uses", u"verr", u"verw", u"wait", u"warn",
    u"wbinvd", u"width", u"windows", u"with", u"word", u"wrmsr", u"xadd", u"xchg", u"xlatb",
    u"xor", u"xorps", u"zero?",
};

constexpr std::u16string_view kAssemblerWords1[] = {
    u"alpha", u"break", u"continue", u"cref", u"endw", u"err1", u"err2", u"errb", u"errdef",
    u"exit", u"lall", u"lfcond", u"list", u"listall", u"listif", u"listmacro", u"listmacroall",
    u"mmx", u"nocref", u"nolist", u"nolistif", u"nolistmacro", u"nommx", u"sall", u"seq",
    u"sfcond", u"tfcond", u"until", u"untilcxz", u"xall", u"xcref", u"xlist",
};

constexpr std::u16string_view kAssemblerWords2[] = {
    u"const", u"dosseg", u"else", u"elseif", u"endif", u"err", u"errdif", u"errdifi", u"erre",
    u"erridn", u"erridni", u"errnb", u"errndef", u"errnz", u"if", u"radix", u"stack", u"type",
    u"while",
};

constexpr std::u16string_view kAssemblerWords3[] = {
    u"??date", u"??filename", u"??time", u"@codesize", u"@cpu", u"@datasize", u"@filename",
    u"@wordsize",
};

constexpr std::u16string_view kAssemblerWords4[] = {
    u"curseg",
};

constexpr std::u16string_view kAssemblerWords5[] = {
    u"version",
};

constexpr std::u16string_view kAssemblerWords6[] = {
    u"code", u"data", u"fardata", u"model", u"startup",
};

constexpr std::u16string_view kAssemblerWords7[] = {
    u"carry?", u"data?", u"fardata?",
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
    {comment, u"//", u"", u"", 0, false, false, false, false, false},
    {comment, u"/*", u"*/", u"", 0, false, true, false, false, false},
    {string, u"@\"", u"\"", u"", 0, true, true, false, false, false},
    {string, u"\"", u"\"", u"uUL8", u'\\', false, false, false, false, false},
    {string, u"'", u"'", u"uUL8", u'\\', false, false, false, true, false},
};

constexpr std::u16string_view kCWords0[] = {
    u"define", u"elif", u"else", u"endif", u"error", u"if", u"ifdef", u"ifndef", u"import",
    u"include", u"line", u"pragma", u"undef",
};

constexpr std::u16string_view kCWords1[] = {
    u"__abstract", u"__asm", u"__based", u"__box", u"__cdecl", u"__declspec", u"__delegate",
    u"__event", u"__except", u"__fastcall", u"__finally", u"__gc", u"__identifier", u"__inline",
    u"__int16", u"__int32", u"__int64", u"__int8", u"__interface", u"__leave",
    u"__multiple_inheritance", u"__nogc", u"__pin", u"__property", u"__sealed",
    u"__single_inheritance", u"__stdcall", u"__try", u"__try_cast", u"__typeof", u"__uuidof",
    u"__value", u"__virtual_inheritance", u"asm", u"auto", u"bad_cast", u"bad_typeid", u"bool",
    u"break", u"case", u"catch", u"char", u"class", u"const", u"const_cast", u"continue",
    u"default", u"delete", u"do", u"double", u"dynamic_cast", u"else", u"enum", u"except",
    u"explicit", u"extern", u"false", u"finally", u"float", u"for", u"friend", u"goto", u"if",
    u"inline", u"int", u"long", u"mutable", u"namespace", u"new", u"operator", u"private",
    u"protected", u"public", u"register", u"reinterpret_cast", u"return", u"short", u"signed",
    u"sizeof", u"static", u"static_cast", u"struct", u"switch", u"template", u"this", u"throw",
    u"true", u"try", u"type_info", u"typedef", u"typeid", u"typename", u"union", u"unsigned",
    u"using", u"virtual", u"void", u"volatile", u"wchar", u"wchar_t", u"while",
};

constexpr keyword_set kCKeywords[] = {
    {hash_with_space, word, kCWords0, 7},
    {word, word, kCWords1, 22},
};

// ---- CSharp ----

constexpr delimited_rule kCSharpDelimiters[] = {
    {comment, u"//", u"", u"", 0, false, false, false, false, false},
    {comment, u"/*", u"*/", u"", 0, false, true, false, false, false},
    {string, u"@\"", u"\"", u"", 0, true, true, false, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
    {string, u"'", u"'", u"", u'\\', false, false, false, true, false},
};

constexpr std::u16string_view kCSharpWords0[] = {
    u"define", u"elif", u"else", u"endif", u"endregion", u"error", u"if", u"line", u"pragma",
    u"region", u"undef", u"warning",
};

constexpr std::u16string_view kCSharpWords1[] = {
    u"__arglist", u"__makeref", u"__reftype", u"__refvalue", u"abstract", u"add", u"alias",
    u"as", u"ascending", u"assembly", u"base", u"bool", u"break", u"by", u"byte", u"case",
    u"catch", u"char", u"checked", u"class", u"const", u"continue", u"decimal", u"default",
    u"delegate", u"descending", u"do", u"double", u"dynamic", u"else", u"enum", u"equals",
    u"event", u"explicit", u"extern", u"false", u"field", u"finally", u"fixed", u"float",
    u"for", u"foreach", u"from", u"get", u"global", u"goto", u"group", u"if", u"implicit",
    u"in", u"int", u"interface", u"internal", u"into", u"is", u"join", u"let", u"lock", u"long",
    u"method", u"module", u"namespace", u"new", u"null", u"object", u"on", u"operator",
    u"orderby", u"out", u"override", u"param", u"params", u"partial", u"private", u"property",
    u"protected", u"public", u"readonly", u"ref", u"remove", u"return", u"sbyte", u"sealed",
    u"select", u"set", u"short", u"sizeof", u"stackalloc", u"static", u"string", u"struct",
    u"switch", u"this", u"throw", u"true", u"try", u"type", u"typeof", u"typevar", u"uint",
    u"ulong", u"unchecked", u"unsafe", u"ushort", u"using", u"value", u"var", u"virtual",
    u"void", u"volatile", u"where", u"while", u"yield",
};

constexpr keyword_set kCSharpKeywords[] = {
    {hash_with_space, word, kCSharpWords0, 9},
    {word, word, kCSharpWords1, 10},
};

// ---- Erlang ----

constexpr delimited_rule kErlangDelimiters[] = {
    {comment, u"%", u"", u"", 0, false, false, false, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
    {string, u"'", u"'", u"", u'\\', false, false, false, false, false},
};

constexpr std::u16string_view kErlangWords0[] = {
    u"after", u"begin", u"case", u"catch", u"cond", u"end", u"fun", u"if", u"let", u"of",
    u"query", u"receive", u"try", u"when",
};

constexpr keyword_set kErlangKeywords[] = {
    {word, word, kErlangWords0, 7},
};

// ---- Haskell ----

constexpr delimited_rule kHaskellDelimiters[] = {
    {comment, u"--", u"", u"", 0, false, false, false, false, false},
    {comment, u"{-", u"-}", u"", 0, false, true, false, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
    {string, u"'", u"'", u"", u'\\', false, false, false, false, false},
};

constexpr std::u16string_view kHaskellWords0[] = {
    u"_", u"anyclass", u"as", u"case", u"class", u"data", u"default", u"deriving", u"do",
    u"else", u"family", u"forall", u"foreign", u"hiding", u"if", u"import", u"in", u"infix",
    u"infixl", u"infixr", u"instance", u"let", u"mdo", u"module", u"newtype", u"of", u"pattern",
    u"qualified", u"role", u"static", u"stock", u"then", u"type", u"via", u"where",
};

constexpr keyword_set kHaskellKeywords[] = {
    {word, word, kHaskellWords0, 9},
};

// ---- IDL ----

constexpr delimited_rule kIDLDelimiters[] = {
    {comment, u"//", u"", u"", 0, false, false, false, false, false},
    {comment, u"/*", u"*/", u"", 0, false, true, false, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
    {string, u"'", u"'", u"", u'\\', false, false, false, false, false},
};

constexpr std::u16string_view kIDLWords0[] = {
    u"FALSE", u"Object", u"TRUE", u"any", u"attribute", u"boolean", u"case", u"char", u"const",
    u"context", u"default", u"double", u"enum", u"exception", u"fixed", u"float", u"in",
    u"inout", u"interface", u"long", u"module", u"native", u"octet", u"oneway", u"out",
    u"raises", u"readonly", u"sequence", u"short", u"string", u"struct", u"switch", u"typedef",
    u"union", u"unsigned", u"void", u"wchar", u"wstring",
};

constexpr keyword_set kIDLKeywords[] = {
    {word, word, kIDLWords0, 9},
};

// ---- Java ----

constexpr delimited_rule kJavaDelimiters[] = {
    {comment, u"//", u"", u"", 0, false, false, false, false, false},
    {comment, u"/*", u"*/", u"", 0, false, true, false, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
};

constexpr std::u16string_view kJavaWords0[] = {
    u"abstract", u"boolean", u"break", u"byte", u"case", u"catch", u"char", u"class", u"const",
    u"continue", u"debugger", u"default", u"delete", u"do", u"double", u"else", u"enum",
    u"export", u"extends", u"false", u"final", u"finally", u"float", u"for", u"function",
    u"goto", u"if", u"implements", u"import", u"in", u"instanceof", u"int", u"interface",
    u"long", u"native", u"new", u"null", u"package", u"private", u"protected", u"public",
    u"return", u"short", u"static", u"super", u"switch", u"synchronized", u"this", u"throw",
    u"throws", u"transient", u"true", u"try", u"typeof", u"var", u"void", u"volatile", u"while",
    u"with",
};

constexpr keyword_set kJavaKeywords[] = {
    {word, word, kJavaWords0, 12},
};

// ---- Lisp ----

constexpr delimited_rule kLispDelimiters[] = {
    {comment, u";", u"", u"", 0, false, false, false, false, false},
    {comment, u"#|", u"|#", u"", 0, false, true, false, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
};

constexpr std::u16string_view kLispWords0[] = {
    u"apply", u"block", u"case", u"catch", u"clear-output", u"close", u"compile", u"cond",
    u"count-if", u"count-if-not", u"declaim", u"declare", u"defconstant", u"defmacro",
    u"defpackage", u"defparameter", u"defun", u"defvar", u"delete-file", u"directory", u"do",
    u"dolist", u"dotimes", u"ensure-directories-exist", u"eval", u"export", u"file-exists-p",
    u"find-if", u"find-if-not", u"find-symbol", u"finish-output", u"force-output", u"format",
    u"fresh-line", u"funcall", u"function", u"go", u"handler-case", u"if", u"import",
    u"in-package", u"intern", u"lambda", u"let", u"let*", u"load", u"loop", u"make-package",
    u"mapc", u"mapcan", u"mapcar", u"mapcon", u"mapl", u"maplist", u"merge",
    u"multiple-value-bind", u"multiple-value-call", u"open", u"position-if", u"position-if-not",
    u"prin1", u"princ", u"print", u"probe-file", u"prog1", u"prog2", u"progn", u"provide",
    u"read", u"read-char", u"read-line", u"reduce", u"remove-if", u"remove-if-not",
    u"rename-file", u"require", u"restart-case", u"return", u"return-from", u"sort",
    u"stable-sort", u"tagbody", u"terpri", u"throw", u"unless", u"unwind-protect",
    u"use-package", u"values", u"when", u"with-input-from-string", u"with-open-file",
    u"with-output-to-string", u"write",
};

constexpr keyword_set kLispKeywords[] = {
    {open_paren, word, kLispWords0, 24},
};

// ---- MSIL ----

constexpr delimited_rule kMSILDelimiters[] = {
    {comment, u"//", u"", u"", 0, false, false, false, false, false},
    {comment, u"/*", u"*/", u"", 0, false, true, false, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
};

constexpr std::u16string_view kMSILWords0[] = {
    u".assembly", u".class", u".entrypoint", u".event", u".field", u".locals", u".maxstack",
    u".method", u".module", u".namespace", u".property", u".try", u"abstract", u"add",
    u"add.ovf", u"add.ovf.un", u"and", u"arglist", u"beq", u"beq.s", u"bge", u"bge.s",
    u"bge.un", u"bge.un.s", u"bgt", u"bgt.s", u"bgt.un", u"bgt.un.s", u"ble", u"ble.s",
    u"ble.un", u"ble.un.s", u"blt", u"blt.s", u"blt.un", u"blt.un.s", u"bne.un", u"bne.un.s",
    u"bool", u"box", u"br", u"br.s", u"break", u"brfalse", u"brfalse.s", u"brinst", u"brinst.s",
    u"brnull", u"brnull.s", u"brtrue", u"brtrue.s", u"brzero", u"brzero.s", u"call", u"calli",
    u"callvirt", u"castclass", u"cdecl", u"ceq", u"cgt", u"cgt.un", u"ckfinite", u"clt",
    u"clt.un", u"constrained", u"conv.i", u"conv.i1", u"conv.i2", u"conv.i4", u"conv.i8",
    u"conv.ovf.i", u"conv.ovf.i.un", u"conv.ovf.i1", u"conv.ovf.i1.un", u"conv.ovf.i2",
    u"conv.ovf.i2.un", u"conv.ovf.i4", u"conv.ovf.i4.un", u"conv.ovf.i8", u"conv.ovf.i8.un",
    u"conv.ovf.u", u"conv.ovf.u.un", u"conv.ovf.u1", u"conv.ovf.u1.un", u"conv.ovf.u2",
    u"conv.ovf.u2.un", u"conv.ovf.u4", u"conv.ovf.u4.un", u"conv.ovf.u8", u"conv.ovf.u8.un",
    u"conv.r.un", u"conv.r4", u"conv.r8", u"conv.u", u"conv.u1", u"conv.u2", u"conv.u4",
    u"conv.u8", u"cpblk", u"cpobj", u"div", u"div.un", u"dup", u"endfilter", u"endfinally",
    u"explicit", u"fastcall", u"initblk", u"initobj", u"instance", u"isinst", u"jmp", u"ldarg",
    u"ldarg.0", u"ldarg.1", u"ldarg.2", u"ldarg.3", u"ldarg.s", u"ldarga", u"ldarga.s",
    u"ldc.i4", u"ldc.i4.0", u"ldc.i4.1", u"ldc.i4.2", u"ldc.i4.3", u"ldc.i4.4", u"ldc.i4.5",
    u"ldc.i4.6", u"ldc.i4.7", u"ldc.i4.8", u"ldc.i4.m1", u"ldc.i4.s", u"ldc.i8", u"ldc.r4",
    u"ldc.r8", u"ldelem", u"ldelem.i", u"ldelem.i1", u"ldelem.i2", u"ldelem.i4", u"ldelem.i8",
    u"ldelem.r4", u"ldelem.r8", u"ldelem.ref", u"ldelem.u1", u"ldelem.u2", u"ldelem.u4",
    u"ldelem.u8", u"ldelema", u"ldftn", u"ldind.i", u"ldind.i1", u"ldind.i2", u"ldind.i4",
    u"ldind.i8", u"ldind.r4", u"ldind.r8", u"ldind.ref", u"ldind.u1", u"ldind.u2", u"ldind.u4",
    u"ldind.u8", u"ldlen", u"ldloc", u"ldloc.0", u"ldloc.1", u"ldloc.2", u"ldloc.3", u"ldloc.s",
    u"ldloca", u"ldloca.s", u"ldnull", u"ldobj", u"ldsfld", u"ldsflda", u"ldstr", u"ldtoken",
    u"ldvirtftn", u"leave", u"leave.s", u"localloc", u"mkrefany", u"mul", u"mul.ovf",
    u"mul.ovf.un", u"native float", u"native int", u"native unsigned int", u"neg", u"newarr",
    u"newobj", u"no", u"nop", u"not", u"object", u"or", u"override", u"pop", u"private",
    u"public", u"readonly", u"refanytype", u"refanyval", u"rem", u"rem.un", u"ret", u"rethrow",
    u"sealed", u"shl", u"shr", u"shr.un", u"sizeof", u"starg", u"starg.s", u"static",
    u"stdcall", u"stelem", u"stelem.i", u"stelem.i1", u"stelem.i2", u"stelem.i4", u"stelem.i8",
    u"stelem.r4", u"stelem.r8", u"stelem.ref", u"stelem.u1", u"stelem.u2", u"stelem.u4",
    u"stelem.u8", u"stind.i", u"stind.i1", u"stind.i2", u"stind.i4", u"stind.i8", u"stind.r4",
    u"stind.r8", u"stind.ref", u"stloc", u"stloc.0", u"stloc.1", u"stloc.2", u"stloc.3",
    u"stloc.s", u"stobj", u"string", u"stsfld", u"sub", u"sub.ovf", u"sub.ovf.un", u"switch",
    u"tail", u"thiscall", u"throw", u"unaligned", u"unbox", u"unbox.any", u"valuetype",
    u"vararg", u"virtual", u"void", u"volatile", u"xor",
};

constexpr keyword_set kMSILKeywords[] = {
    {word, word, kMSILWords0, 19},
};

// ---- Nemerle ----

constexpr delimited_rule kNemerleDelimiters[] = {
    {comment, u"//", u"", u"", 0, false, false, false, false, false},
    {comment, u"/*", u"*/", u"", 0, false, true, false, false, false},
    {string, u"@\"", u"\"", u"", 0, true, true, false, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
    {string, u"'", u"'", u"", u'\\', false, false, false, true, false},
};

constexpr std::u16string_view kNemerleWords0[] = {
    u"define", u"elif", u"else", u"endif", u"endregion", u"error", u"if", u"line", u"pragma",
    u"region", u"undef", u"warning",
};

constexpr std::u16string_view kNemerleWords1[] = {
    u"_", u"abstract", u"add", u"as", u"assembly", u"await", u"base", u"bool", u"break",
    u"byte", u"catch", u"char", u"checked", u"class", u"continue", u"decimal", u"def",
    u"default", u"delegate", u"do", u"double", u"else", u"enum", u"event", u"extern", u"false",
    u"field", u"finally", u"float", u"for", u"foreach", u"get", u"if", u"in", u"int",
    u"interface", u"internal", u"is", u"keyword", u"lock", u"long", u"macro", u"marker",
    u"match", u"method", u"module", u"mutable", u"namespace", u"new", u"null", u"object",
    u"out", u"override", u"param", u"params", u"partial", u"private", u"protected", u"public",
    u"ref", u"regex", u"remove", u"return", u"sbyte", u"sealed", u"set", u"short", u"span",
    u"static", u"string", u"struct", u"syntax", u"this", u"throw", u"token", u"true", u"try",
    u"type", u"typeof", u"typevar", u"uint", u"ulong", u"unchecked", u"unless", u"ushort",
    u"using", u"value", u"variant", u"virtual", u"void", u"volatile", u"when", u"where",
    u"while", u"with", u"yield",
};

constexpr keyword_set kNemerleKeywords[] = {
    {hash_with_space, word, kNemerleWords0, 9},
    {word, word, kNemerleWords1, 9},
};

// ---- Nitra ----

constexpr delimited_rule kNitraDelimiters[] = {
    {comment, u"//", u"", u"", 0, false, false, false, false, false},
    {comment, u"/*", u"*/", u"", 0, false, true, false, false, false},
    {string, u"@\"", u"\"", u"", 0, true, true, false, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
    {string, u"'", u"'", u"", u'\\', false, false, false, true, false},
};

constexpr std::u16string_view kNitraWords0[] = {
    u"define", u"elif", u"else", u"endif", u"endregion", u"error", u"if", u"line", u"pragma",
    u"region", u"undef", u"warning",
};

constexpr std::u16string_view kNitraWords1[] = {
    u"SpanClass", u"StartRule", u"_", u"abstract", u"add", u"alias", u"as", u"assembly",
    u"await", u"base", u"bool", u"break", u"byte", u"catch", u"char", u"checked", u"class",
    u"company", u"continue", u"decimal", u"declaration", u"declarations", u"def", u"default",
    u"delegate", u"do", u"double", u"else", u"enum", u"event", u"extend", u"extension",
    u"extern", u"false", u"field", u"finally", u"float", u"for", u"foreach", u"get", u"if",
    u"in", u"inout", u"int", u"interface", u"internal", u"is", u"keyword", u"language", u"lock",
    u"long", u"macro", u"map", u"marker", u"match", u"method", u"module", u"mutable",
    u"namespace", u"new", u"null", u"object", u"out", u"override", u"param", u"params",
    u"partial", u"precedence", u"private", u"protected", u"public", u"ref", u"regex", u"remove",
    u"return", u"right-associative", u"rule", u"sbyte", u"sealed", u"set", u"short", u"span",
    u"start", u"static", u"string", u"struct", u"style", u"syntax", u"this", u"throw", u"token",
    u"true", u"try", u"type", u"typeof", u"typevar", u"uint", u"ulong", u"unchecked", u"unless",
    u"ushort", u"using", u"value", u"variant", u"virtual", u"void", u"volatile", u"when",
    u"where", u"while", u"with", u"yield",
};

constexpr keyword_set kNitraKeywords[] = {
    {hash_with_space, word, kNitraWords0, 9},
    {word, word, kNitraWords1, 17},
};

// ---- ObjC ----

constexpr delimited_rule kObjCDelimiters[] = {
    {comment, u"//", u"", u"", 0, false, false, false, false, false},
    {comment, u"/*", u"*/", u"", 0, false, true, false, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
    {string, u"'", u"'", u"", u'\\', false, false, false, false, false},
};

constexpr std::u16string_view kObjCWords0[] = {
    u"@catch", u"@class", u"@defs", u"@dynamic", u"@encode", u"@end", u"@finally",
    u"@implementation", u"@import", u"@interface", u"@optional", u"@private", u"@property",
    u"@protected", u"@protocol", u"@public", u"@required", u"@selector", u"@synchronized",
    u"@synthesize", u"@throw", u"@try", u"BOOL", u"Class", u"IMP", u"NO", u"NULL", u"Nil",
    u"SEL", u"YES", u"_Bool", u"_Complex", u"_Imaginary", u"assign", u"atomic", u"auto",
    u"break", u"case", u"char", u"const", u"continue", u"copy", u"default", u"do", u"double",
    u"else", u"enum", u"extern", u"float", u"for", u"getter", u"goto", u"id", u"if", u"inline",
    u"int", u"long", u"nil", u"nonatomic", u"readonly", u"readwrite", u"register", u"restrict",
    u"retain", u"return", u"self", u"setter", u"short", u"signed", u"sizeof", u"static",
    u"strong", u"struct", u"super", u"switch", u"typedef", u"union", u"unsafe_unretained",
    u"unsigned", u"void", u"volatile", u"weak", u"while",
};

constexpr keyword_set kObjCKeywords[] = {
    {word, word, kObjCWords0, 17},
};

// ---- Ocaml ----

constexpr delimited_rule kOcamlDelimiters[] = {
    {comment, u"(*", u"*)", u"", 0, false, true, false, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
    {string, u"'", u"'", u"", 0, false, false, false, false, false},
};

constexpr std::u16string_view kOcamlWords0[] = {
    u"and", u"as", u"asr", u"assert", u"begin", u"class", u"constraints", u"do", u"done",
    u"downto", u"else", u"end", u"exception", u"external", u"false", u"for", u"fun",
    u"function", u"functor", u"if", u"in", u"include", u"inherit", u"initializer", u"land",
    u"lazy", u"let", u"lor", u"lsl", u"lsr", u"lxor", u"match", u"method", u"mod", u"module",
    u"mutable", u"new", u"nonrec", u"object", u"of", u"open", u"or", u"private", u"rec", u"sig",
    u"struct", u"then", u"to", u"true", u"try", u"type", u"val", u"virtual", u"when", u"while",
    u"with",
};

constexpr keyword_set kOcamlKeywords[] = {
    {word, word, kOcamlWords0, 11},
};

// ---- Pascal ----

constexpr delimited_rule kPascalDelimiters[] = {
    {comment, u"{", u"}", u"", 0, false, true, false, false, false},
    {comment, u"(*", u"*)", u"", 0, false, true, false, false, false},
    {comment, u"//", u"", u"", 0, false, false, false, false, false},
    {string, u"'", u"'", u"", 0, true, false, false, false, false},
};

constexpr std::u16string_view kPascalWords0[] = {
    u"absolute", u"abstract", u"and", u"array", u"as", u"asm", u"assembler", u"automated",
    u"begin", u"case", u"cdecl", u"class", u"const", u"constructor", u"contains", u"default",
    u"deprecated", u"destructor", u"dispid", u"dispinterface", u"div", u"do", u"downto",
    u"dynamic", u"else", u"end", u"except", u"export", u"exports", u"external", u"far", u"file",
    u"final", u"finalization", u"finally", u"for", u"forward", u"function", u"goto", u"helper",
    u"if", u"implementation", u"implements", u"in", u"index", u"inherited", u"initialization",
    u"inline", u"interface", u"is", u"label", u"library", u"local", u"message", u"mod", u"name",
    u"near", u"nil", u"nodefault", u"not", u"object", u"of", u"on", u"or", u"out", u"overload",
    u"override", u"package", u"packed", u"pascal", u"platform", u"private", u"procedure",
    u"program", u"property", u"protected", u"public", u"published", u"raise", u"record",
    u"register", u"reintroduce", u"repeat", u"requires", u"resident", u"resourcestring",
    u"safecall", u"sealed", u"set", u"shl", u"shr", u"static", u"stdcall", u"stored", u"strict",
    u"string", u"then", u"threadvar", u"to", u"try", u"type", u"unit", u"unsafe", u"until",
    u"uses", u"var", u"varargs", u"virtual", u"while", u"with", u"xor",
};

constexpr keyword_set kPascalKeywords[] = {
    {word, word, kPascalWords0, 14},
};

// ---- Perl ----

constexpr delimited_rule kPerlDelimiters[] = {
    {comment, u"#", u"", u"", 0, false, false, false, false, false},
    {comment, u"=", u"=cut", u"", 0, false, true, true, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
    {string, u"'", u"'", u"", 0, true, false, false, false, false},
};

constexpr std::u16string_view kPerlWords0[] = {
    u"END", u"bless", u"caller", u"continue", u"dbmclose", u"dbmopen", u"default", u"defined",
    u"delete", u"do", u"each", u"else", u"elsif", u"endgrent", u"endhostent", u"endnetent",
    u"endprotoent", u"endpwent", u"endservent", u"eof", u"eval", u"exec", u"exists", u"exit",
    u"exp", u"fcntl", u"fileno", u"flock", u"for", u"foreach", u"fork", u"format", u"formline",
    u"getc", u"getgrent", u"getgrgid", u"getgrnam", u"gethostbyaddr", u"gethostbyname",
    u"gethostent", u"getlogin", u"getnetbyaddr", u"getnetbyname", u"getnetent", u"getpeername",
    u"getpgrp", u"getppid", u"getpriority", u"getprotobyname", u"getprotobynumber",
    u"getprotoent", u"getpwent", u"getpwnam", u"getpwuid", u"getservbyname", u"getservbyport",
    u"getservent", u"getsockname", u"getsockopt", u"glob", u"gmtime", u"goto", u"grep", u"hex",
    u"import", u"index", u"int", u"ioctl", u"join", u"keys", u"kill", u"last", u"lc",
    u"lcfirst", u"length", u"link", u"listen", u"local", u"localtime", u"log", u"lstat", u"map",
    u"mkdir", u"msgctl", u"msgget", u"msgrcv", u"msgsnd", u"my", u"next", u"no", u"oct",
    u"open", u"opendir", u"ord", u"pack", u"package", u"pipe", u"pop", u"pos", u"print",
    u"printf", u"push", u"quotemeta", u"rand", u"read", u"readdir", u"readline", u"readlink",
    u"recv", u"redo", u"ref", u"rename", u"require", u"reset", u"return", u"reverse",
    u"rewinddir", u"rindex", u"rmdir", u"scalar", u"seek", u"seekdir", u"select", u"semctl",
    u"semget", u"semop", u"send", u"setgrent", u"sethostent", u"setnetent", u"setpgrp",
    u"setpriority", u"setprotoent", u"setpwent", u"setservent", u"setsockopt", u"shift",
    u"shmctl", u"shmget", u"shmread", u"shmwrite", u"shutdown", u"sin", u"sleep", u"socket",
    u"socketpair", u"sort", u"splice", u"split", u"sprintf", u"sqrt", u"srand", u"stat",
    u"study", u"sub", u"substr", u"symlink", u"syscall", u"sysread", u"system", u"syswrite",
    u"tell", u"telldir", u"tie", u"tied", u"time", u"times", u"tr", u"truncate", u"uc",
    u"ucfirst", u"umask", u"undef", u"unless", u"unlink", u"unpack", u"unshift", u"untie",
    u"until", u"use", u"utime", u"values", u"vec", u"wait", u"waitpid", u"wantarray", u"warn",
    u"while", u"write", u"xor",
};

constexpr keyword_set kPerlKeywords[] = {
    {word, word, kPerlWords0, 16},
};

// ---- PHP ----

constexpr delimited_rule kPHPDelimiters[] = {
    {comment, u"//", u"", u"", 0, false, false, false, false, false},
    {comment, u"#", u"", u"", 0, false, false, false, false, false},
    {comment, u"/*", u"*/", u"", 0, false, true, false, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
    {string, u"'", u"'", u"", u'\\', false, false, false, false, false},
};

constexpr std::u16string_view kPHPWords0[] = {
    u"abstract", u"and", u"array", u"as", u"break", u"case", u"catch", u"cfunction", u"class",
    u"clone", u"const", u"continue", u"declare", u"default", u"die", u"do", u"echo", u"else",
    u"elseif", u"empty", u"enddeclare", u"endfor", u"endforeach", u"endif", u"endswitch",
    u"endwhile", u"eval", u"exception", u"exit", u"extends", u"final", u"for", u"foreach",
    u"function", u"global", u"if", u"implements", u"include", u"include_once", u"interface",
    u"isset", u"list", u"new", u"old_function", u"or", u"php_user_filter", u"print", u"private",
    u"protected", u"public", u"require", u"require_once", u"return", u"static", u"switch",
    u"this", u"throw", u"try", u"unset", u"use", u"var", u"while", u"xor",
};

constexpr keyword_set kPHPKeywords[] = {
    {word, word, kPHPWords0, 15},
};

// ---- Prolog ----

constexpr delimited_rule kPrologDelimiters[] = {
    {comment, u"%", u"", u"", 0, false, false, false, false, false},
    {comment, u"/*", u"*/", u"", 0, false, true, false, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
};

constexpr std::u16string_view kPrologWords0[] = {
    u"abolish", u"abort", u"arg", u"asserta", u"assertz", u"atom", u"atomic", u"bagof",
    u"break", u"call", u"case", u"catch", u"clause", u"close", u"concat", u"consult",
    u"dynamic", u"else", u"end", u"fail", u"false", u"findall", u"float", u"flush_output",
    u"forall", u"functor", u"get", u"halt", u"if", u"integer", u"is", u"length", u"listing",
    u"mod", u"multifile", u"nl", u"nonvar", u"not", u"notrace", u"number", u"once", u"op",
    u"open", u"peek_char", u"peek_code", u"put", u"read", u"repeat", u"retract", u"retractall",
    u"see", u"seeing", u"seen", u"setof", u"skip", u"static", u"sub_atom", u"tell", u"telling",
    u"term", u"throw", u"trace", u"true", u"ttyflush", u"unify_with_occurs_check", u"unknown",
    u"var", u"write",
};

constexpr keyword_set kPrologKeywords[] = {
    {word, word, kPrologWords0, 23},
};

// ---- Python ----

constexpr delimited_rule kPythonDelimiters[] = {
    {comment, u"#", u"", u"", 0, false, false, false, false, false},
    {comment, u"\"\"\"", u"\"\"\"", u"uUrR", 0, false, true, false, false, false},
    {comment, u"'''", u"'''", u"uUrR", 0, false, true, false, false, false},
    {string, u"\"", u"\"", u"uUrR", u'\\', false, false, false, false, false},
    {string, u"'", u"'", u"uUrR", u'\\', false, false, false, false, false},
};

constexpr std::u16string_view kPythonWords0[] = {
    u"and", u"assert", u"break", u"class", u"continue", u"def", u"del", u"elif", u"else",
    u"except", u"exec", u"finally", u"for", u"from", u"global", u"if", u"import", u"in", u"is",
    u"not", u"or", u"pass", u"print", u"raise", u"return", u"try", u"while", u"yield",
};

constexpr keyword_set kPythonKeywords[] = {
    {word, word, kPythonWords0, 8},
};

// ---- Ruby ----

constexpr delimited_rule kRubyDelimiters[] = {
    {comment, u"#", u"", u"", 0, false, false, false, false, false},
    {comment, u"=begin", u"=end", u"", 0, false, true, true, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
    {string, u"'", u"'", u"", u'\\', false, false, false, false, false},
};

constexpr std::u16string_view kRubyWords0[] = {
    u"BEGIN", u"END", u"alias", u"and", u"begin", u"break", u"case", u"class", u"def",
    u"defined", u"do", u"else", u"elsif", u"end", u"ensure", u"false", u"for", u"if", u"in",
    u"module", u"next", u"nil", u"not", u"or", u"redo", u"rescue", u"retry", u"return", u"self",
    u"super", u"then", u"true", u"undef", u"unless", u"until", u"when", u"while", u"yield",
};

constexpr keyword_set kRubyKeywords[] = {
    {word, word, kRubyWords0, 7},
};

// ---- Rust ----

constexpr delimited_rule kRustDelimiters[] = {
    {comment, u"//", u"", u"", 0, false, false, false, false, false},
    {comment, u"/*", u"*/", u"", 0, false, true, false, false, false},
    {string, u"@\"", u"\"", u"", 0, true, true, false, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
    {string, u"'", u"'", u"", u'\\', false, false, false, true, false},
};

constexpr std::u16string_view kRustWords0[] = {
    u"assert", u"fail",
};

constexpr std::u16string_view kRustWords1[] = {
    u"Self", u"as", u"bool", u"break", u"char", u"const", u"continue", u"do", u"enum",
    u"extern", u"f32", u"f64", u"false", u"float", u"fn", u"for", u"i16", u"i32", u"i64", u"i8",
    u"if", u"impl", u"in", u"int", u"let", u"loop", u"mod", u"mut", u"once", u"priv", u"proc",
    u"pub", u"ref", u"return", u"static", u"str", u"struct", u"trait", u"true", u"type", u"u16",
    u"u32", u"u64", u"u8", u"uint", u"unsafe", u"use", u"while",
};

constexpr keyword_set kRustKeywords[] = {
    {word, exclamation_and_word, kRustWords0, 6},
    {word, word, kRustWords1, 8},
};

// ---- SQL ----

constexpr delimited_rule kSQLDelimiters[] = {
    {comment, u"--", u"", u"", 0, false, false, false, false, false},
    {comment, u"/*", u"*/", u"", 0, false, true, false, false, false},
    {string, u"'", u"'", u"", 0, true, false, false, false, false},
};

constexpr std::u16string_view kSQLWords0[] = {
    u"add", u"all", u"alter", u"and", u"any", u"as", u"asc", u"authorization", u"backup",
    u"begin", u"between", u"break", u"browse", u"bulk", u"by", u"cascade", u"case", u"check",
    u"checkpoint", u"close", u"clustered", u"coalesce", u"collate", u"column", u"commit",
    u"compute", u"constraint", u"contains", u"containstable", u"continue", u"convert", u"count",
    u"create", u"cross", u"current", u"current_date", u"current_time", u"current_timestamp",
    u"current_user", u"cursor", u"database", u"dbcc", u"deallocate", u"declare", u"default",
    u"delete", u"deny", u"desc", u"disk", u"distinct", u"distributed", u"double", u"drop",
    u"dummy", u"dump", u"else", u"end", u"errlvl", u"escape", u"except", u"exec", u"execute",
    u"exists", u"exit", u"fetch", u"file", u"fillfactor", u"for", u"foreign", u"freetext",
    u"freetexttable", u"from", u"full", u"function", u"goto", u"grant", u"group", u"having",
    u"holdlock", u"identity", u"identity_insert", u"identitycol", u"if", u"in", u"index",
    u"inner", u"insert", u"intersect", u"into", u"is", u"join", u"key", u"kill", u"left",
    u"like", u"lineno", u"load", u"local", u"national", u"nocheck", u"nonclustered", u"not",
    u"null", u"nullif", u"of", u"off", u"offsets", u"on", u"open", u"opendatasource",
    u"openquery", u"openrowset", u"openxml", u"option", u"or", u"order", u"outer", u"over",
    u"percent", u"plan", u"precision", u"primary", u"print", u"proc", u"procedure", u"public",
    u"raiserror", u"read", u"readtext", u"reconfigure", u"references", u"replication",
    u"restore", u"restrict", u"return", u"revoke", u"right", u"rollback", u"rowcount",
    u"rowguidcol", u"rule", u"save", u"schema", u"select", u"session_user", u"set", u"setuser",
    u"shutdown", u"some", u"statistics", u"system_user", u"table", u"textsize", u"then", u"to",
    u"top", u"tran", u"transaction", u"trigger", u"truncate", u"tsequal", u"union", u"unique",
    u"update", u"updatetext", u"use", u"user", u"values", u"varying", u"view", u"waitfor",
    u"when", u"where", u"while", u"with", u"writetext",
};

constexpr keyword_set kSQLKeywords[] = {
    {word, word, kSQLWords0, 17},
};

// ---- VisualBasic ----

constexpr delimited_rule kVisualBasicDelimiters[] = {
    {comment, u"'", u"", u"", 0, false, false, false, false, false},
    {string, u"\"", u"\"", u"", 0, true, false, false, false, false},
};

constexpr std::u16string_view kVisualBasicWords0[] = {
    u"addhandler", u"addressof", u"alias", u"and", u"andalso", u"ansi", u"as", u"assembly",
    u"auto", u"byref", u"byval", u"call", u"case", u"catch", u"class", u"const", u"continue",
    u"declare", u"default", u"delegate", u"dim", u"directcast", u"do", u"each", u"else",
    u"elseif", u"end", u"enum", u"erase", u"error", u"event", u"exit", u"finally", u"for",
    u"friend", u"function", u"get", u"gettype", u"gosub", u"goto", u"handles", u"if",
    u"implements", u"imports", u"in", u"inherits", u"interface", u"is", u"let", u"lib", u"like",
    u"loop", u"me", u"mod", u"mustinherit", u"mustoverride", u"mybase", u"myclass",
    u"namespace", u"new", u"next", u"not", u"nothing", u"notinheritable", u"notoverridable",
    u"object", u"on", u"option", u"optional", u"or", u"orelse", u"overloads", u"overridable",
    u"overrides", u"paramarray", u"preserve", u"private", u"property", u"protected", u"public",
    u"raiseevent", u"readonly", u"redim", u"rem", u"removehandler", u"resume", u"return",
    u"select", u"set", u"shadows", u"shared", u"single", u"static", u"step", u"stop", u"string",
    u"structure", u"sub", u"synclock", u"then", u"throw", u"to", u"try", u"typeof", u"unicode",
    u"until", u"variant", u"wend", u"when", u"while", u"with", u"withevents", u"writeonly",
    u"xor",
};

constexpr keyword_set kVisualBasicKeywords[] = {
    {word, word, kVisualBasicWords0, 14},
};

// ---- XSL ----

constexpr delimited_rule kXSLDelimiters[] = {
    {comment, u"<!--", u"-->", u"", 0, false, true, false, false, false},
    {string, u"\"", u"\"", u"", 0, false, false, false, false, false},
};

constexpr std::u16string_view kXSLWords0[] = {
    u"?xml",
};

constexpr std::u16string_view kXSLWords1[] = {
    u"case-order", u"current", u"data-type", u"disable-output-escaping", u"document",
    u"elements", u"encoding", u"format-number", u"generate-id", u"indent", u"key", u"lang",
    u"match", u"method", u"mode", u"name", u"namespace", u"order", u"priority",
    u"result-prefix", u"select", u"stylesheet-prefix", u"system-property", u"terminate",
    u"test", u"unparsed-entity-uri", u"use", u"use-attribute-sets", u"version", u"xmlns:xsl",
    u"xsl:analyze-string", u"xsl:apply-imports", u"xsl:apply-templates", u"xsl:attribute",
    u"xsl:attribute-set", u"xsl:call-template", u"xsl:character-map", u"xsl:choose",
    u"xsl:comment", u"xsl:copy", u"xsl:copy-of", u"xsl:decimal-format", u"xsl:document",
    u"xsl:element", u"xsl:fallback", u"xsl:for-each", u"xsl:for-each-group", u"xsl:function",
    u"xsl:if", u"xsl:import", u"xsl:import-schema", u"xsl:include", u"xsl:key",
    u"xsl:matching-substring", u"xsl:message", u"xsl:namespace", u"xsl:namespace-alias",
    u"xsl:next-match", u"xsl:non-matching-substring", u"xsl:number", u"xsl:otherwise",
    u"xsl:output", u"xsl:output-character", u"xsl:param", u"xsl:perform-sort",
    u"xsl:preserve-space", u"xsl:processing-instructions", u"xsl:result-document",
    u"xsl:script", u"xsl:sequence", u"xsl:sort", u"xsl:strip-space", u"xsl:stylesheet",
    u"xsl:template", u"xsl:text", u"xsl:transform", u"xsl:value-of", u"xsl:variable",
    u"xsl:when", u"xsl:with-param",
};

constexpr keyword_set kXSLKeywords[] = {
    {none, word, kXSLWords0, 4},
    {word, word, kXSLWords1, 27},
};

// ---- Bash ----

constexpr delimited_rule kBashDelimiters[] = {
    {comment, u"#", u"", u"", 0, false, false, false, false, true},
    {string, u"\"", u"\"", u"", u'\\', false, true, false, false, false},
    {string, u"'", u"'", u"", 0, false, true, false, false, false},
};

constexpr std::u16string_view kBashWords0[] = {
    u"alias", u"bg", u"bind", u"break", u"builtin", u"case", u"cd", u"command", u"continue",
    u"coproc", u"declare", u"do", u"done", u"echo", u"elif", u"else", u"esac", u"eval", u"exec",
    u"exit", u"export", u"false", u"fg", u"fi", u"for", u"function", u"getopts", u"hash", u"if",
    u"in", u"jobs", u"kill", u"let", u"local", u"mapfile", u"popd", u"printf", u"pushd", u"pwd",
    u"read", u"readarray", u"readonly", u"return", u"select", u"set", u"shift", u"shopt",
    u"source", u"test", u"then", u"time", u"trap", u"true", u"type", u"typeset", u"ulimit",
    u"umask", u"unalias", u"unset", u"until", u"wait", u"while",
};

constexpr keyword_set kBashKeywords[] = {
    {word, word, kBashWords0, 9},
};

// ---- CMake ----

constexpr delimited_rule kCMakeDelimiters[] = {
    {comment, u"#[[", u"]]", u"", 0, false, true, false, false, true},
    {comment, u"#", u"", u"", 0, false, false, false, false, true},
    {string, u"\"", u"\"", u"", u'\\', false, true, false, false, false},
};

constexpr std::u16string_view kCMakeWords0[] = {
    u"add_compile_definitions", u"add_compile_options", u"add_custom_command",
    u"add_custom_target", u"add_definitions", u"add_dependencies", u"add_executable",
    u"add_library", u"add_link_options", u"add_subdirectory", u"add_test", u"and",
    u"aux_source_directory", u"block", u"break", u"cmake_language", u"cmake_minimum_required",
    u"cmake_parse_arguments", u"cmake_path", u"cmake_policy", u"configure_file", u"continue",
    u"defined", u"else", u"elseif", u"enable_language", u"enable_testing", u"endblock",
    u"endforeach", u"endfunction", u"endif", u"endmacro", u"endwhile", u"equal",
    u"execute_process", u"exists", u"export", u"file", u"find_file", u"find_library",
    u"find_package", u"find_path", u"find_program", u"foreach", u"function",
    u"get_cmake_property", u"get_directory_property", u"get_filename_component",
    u"get_property", u"get_source_file_property", u"get_target_property", u"get_test_property",
    u"greater", u"greater_equal", u"if", u"in", u"include", u"include_directories",
    u"include_guard", u"install", u"is_directory", u"less", u"less_equal", u"link_directories",
    u"link_libraries", u"list", u"macro", u"mark_as_advanced", u"matches", u"math", u"message",
    u"not", u"option", u"or", u"project", u"remove_definitions", u"return",
    u"separate_arguments", u"set", u"set_directory_properties", u"set_property",
    u"set_source_files_properties", u"set_target_properties", u"set_tests_properties",
    u"site_name", u"source_group", u"strequal", u"string", u"target_compile_definitions",
    u"target_compile_features", u"target_compile_options", u"target_include_directories",
    u"target_link_directories", u"target_link_libraries", u"target_link_options",
    u"target_precompile_headers", u"target_sources", u"try_compile", u"try_run", u"unset",
    u"variable_watch", u"version_equal", u"version_greater", u"version_less", u"while",
    u"write_file",
};

constexpr keyword_set kCMakeKeywords[] = {
    {word, word, kCMakeWords0, 27},
};

// ---- CSS ----

constexpr delimited_rule kCSSDelimiters[] = {
    {comment, u"/*", u"*/", u"", 0, false, true, false, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
    {string, u"'", u"'", u"", u'\\', false, false, false, false, false},
};

constexpr std::u16string_view kCSSWords0[] = {
    u"@charset", u"@container", u"@font-face", u"@import", u"@keyframes", u"@layer", u"@media",
    u"@namespace", u"@page", u"@property", u"@supports",
};

constexpr std::u16string_view kCSSWords1[] = {
    u"align-content", u"align-items", u"align-self", u"animation", u"animation-delay",
    u"animation-direction", u"animation-duration", u"animation-fill-mode",
    u"animation-iteration-count", u"animation-name", u"animation-play-state",
    u"animation-timing-function", u"backdrop-filter", u"background", u"background-attachment",
    u"background-clip", u"background-color", u"background-image", u"background-origin",
    u"background-position", u"background-repeat", u"background-size", u"border",
    u"border-bottom", u"border-collapse", u"border-color", u"border-image", u"border-left",
    u"border-radius", u"border-right", u"border-spacing", u"border-style", u"border-top",
    u"border-width", u"bottom", u"box-shadow", u"box-sizing", u"caption-side", u"caret-color",
    u"clear", u"clip", u"clip-path", u"color", u"column-count", u"column-gap", u"columns",
    u"content", u"cursor", u"direction", u"display", u"empty-cells", u"filter", u"flex",
    u"flex-basis", u"flex-direction", u"flex-flow", u"flex-grow", u"flex-shrink", u"flex-wrap",
    u"float", u"font", u"font-family", u"font-feature-settings", u"font-size", u"font-style",
    u"font-variant", u"font-weight", u"gap", u"grid", u"grid-area", u"grid-auto-columns",
    u"grid-auto-flow", u"grid-auto-rows", u"grid-column", u"grid-gap", u"grid-row",
    u"grid-template", u"grid-template-areas", u"grid-template-columns", u"grid-template-rows",
    u"height", u"justify-content", u"justify-items", u"justify-self", u"left",
    u"letter-spacing", u"line-height", u"list-style", u"list-style-image",
    u"list-style-position", u"list-style-type", u"margin", u"margin-bottom", u"margin-left",
    u"margin-right", u"margin-top", u"max-height", u"max-width", u"min-height", u"min-width",
    u"mix-blend-mode", u"object-fit", u"object-position", u"opacity", u"order", u"outline",
    u"outline-color", u"outline-offset", u"outline-style", u"outline-width", u"overflow",
    u"overflow-x", u"overflow-y", u"padding", u"padding-bottom", u"padding-left",
    u"padding-right", u"padding-top", u"place-content", u"place-items", u"place-self",
    u"pointer-events", u"position", u"quotes", u"resize", u"right", u"row-gap",
    u"scroll-behavior", u"table-layout", u"text-align", u"text-decoration", u"text-indent",
    u"text-overflow", u"text-shadow", u"text-transform", u"top", u"transform",
    u"transform-origin", u"transition", u"transition-delay", u"transition-duration",
    u"transition-property", u"transition-timing-function", u"user-select", u"vertical-align",
    u"visibility", u"white-space", u"width", u"will-change", u"word-break", u"word-spacing",
    u"word-wrap", u"writing-mode", u"z-index",
};

constexpr keyword_set kCSSKeywords[] = {
    {none, word, kCSSWords0, 10},
    {word, word, kCSSWords1, 26},
};

// ---- Go ----

constexpr delimited_rule kGoDelimiters[] = {
    {comment, u"//", u"", u"", 0, false, false, false, false, false},
    {comment, u"/*", u"*/", u"", 0, false, true, false, false, false},
    {string, u"`", u"`", u"", 0, false, true, false, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
    {string, u"'", u"'", u"", u'\\', false, false, false, true, false},
};

constexpr std::u16string_view kGoWords0[] = {
    u"any", u"append", u"bool", u"break", u"byte", u"cap", u"case", u"chan", u"clear", u"close",
    u"complex", u"complex128", u"complex64", u"const", u"continue", u"copy", u"default",
    u"defer", u"delete", u"else", u"error", u"fallthrough", u"false", u"float32", u"float64",
    u"for", u"func", u"go", u"goto", u"if", u"imag", u"import", u"int", u"int16", u"int32",
    u"int64", u"int8", u"interface", u"iota", u"len", u"make", u"map", u"max", u"min", u"new",
    u"nil", u"package", u"panic", u"print", u"println", u"range", u"real", u"recover",
    u"return", u"rune", u"select", u"string", u"struct", u"switch", u"true", u"type", u"uint",
    u"uint16", u"uint32", u"uint64", u"uint8", u"uintptr", u"var",
};

constexpr keyword_set kGoKeywords[] = {
    {word, word, kGoWords0, 11},
};

// ---- JavaScript ----

constexpr delimited_rule kJavaScriptDelimiters[] = {
    {comment, u"//", u"", u"", 0, false, false, false, false, false},
    {comment, u"/*", u"*/", u"", 0, false, true, false, false, false},
    {string, u"`", u"`", u"", u'\\', false, true, false, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
    {string, u"'", u"'", u"", u'\\', false, false, false, false, false},
};

constexpr std::u16string_view kJavaScriptWords0[] = {
    u"Infinity", u"NaN", u"arguments", u"as", u"async", u"await", u"break", u"case", u"catch",
    u"class", u"const", u"constructor", u"continue", u"debugger", u"default", u"delete", u"do",
    u"else", u"enum", u"export", u"extends", u"false", u"finally", u"for", u"from", u"function",
    u"get", u"globalThis", u"if", u"implements", u"import", u"in", u"instanceof", u"interface",
    u"let", u"new", u"null", u"of", u"package", u"private", u"protected", u"public", u"return",
    u"set", u"static", u"super", u"switch", u"this", u"throw", u"true", u"try", u"typeof",
    u"undefined", u"var", u"void", u"while", u"with", u"yield",
};

constexpr keyword_set kJavaScriptKeywords[] = {
    {word, word, kJavaScriptWords0, 11},
};

// ---- JSON ----

constexpr delimited_rule kJSONDelimiters[] = {
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
};

constexpr std::u16string_view kJSONWords0[] = {
    u"false", u"null", u"true",
};

constexpr keyword_set kJSONKeywords[] = {
    {word, word, kJSONWords0, 5},
};

// ---- Lua ----

constexpr delimited_rule kLuaDelimiters[] = {
    {comment, u"--[[", u"]]", u"", 0, false, true, false, false, false},
    {comment, u"--", u"", u"", 0, false, false, false, false, false},
    {string, u"[[", u"]]", u"", 0, false, true, false, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
    {string, u"'", u"'", u"", u'\\', false, false, false, false, false},
};

constexpr std::u16string_view kLuaWords0[] = {
    u"_ENV", u"_G", u"and", u"assert", u"break", u"collectgarbage", u"coroutine", u"debug",
    u"do", u"dofile", u"else", u"elseif", u"end", u"error", u"false", u"for", u"function",
    u"getmetatable", u"goto", u"if", u"in", u"io", u"ipairs", u"load", u"local", u"math",
    u"next", u"nil", u"not", u"or", u"os", u"package", u"pairs", u"pcall", u"print",
    u"rawequal", u"rawget", u"rawlen", u"rawset", u"repeat", u"require", u"return", u"select",
    u"self", u"setmetatable", u"string", u"table", u"then", u"tonumber", u"tostring", u"true",
    u"type", u"until", u"while", u"xpcall",
};

constexpr keyword_set kLuaKeywords[] = {
    {word, word, kLuaWords0, 14},
};

// ---- TypeScript ----

constexpr delimited_rule kTypeScriptDelimiters[] = {
    {comment, u"//", u"", u"", 0, false, false, false, false, false},
    {comment, u"/*", u"*/", u"", 0, false, true, false, false, false},
    {string, u"`", u"`", u"", u'\\', false, true, false, false, false},
    {string, u"\"", u"\"", u"", u'\\', false, false, false, false, false},
    {string, u"'", u"'", u"", u'\\', false, false, false, false, false},
};

constexpr std::u16string_view kTypeScriptWords0[] = {
    u"Infinity", u"NaN", u"abstract", u"any", u"arguments", u"as", u"asserts", u"async",
    u"await", u"bigint", u"boolean", u"break", u"case", u"catch", u"class", u"const",
    u"constructor", u"continue", u"debugger", u"declare", u"default", u"delete", u"do", u"else",
    u"enum", u"export", u"extends", u"false", u"finally", u"for", u"from", u"function", u"get",
    u"globalThis", u"if", u"implements", u"import", u"in", u"infer", u"instanceof",
    u"interface", u"is", u"keyof", u"let", u"module", u"namespace", u"never", u"new", u"null",
    u"number", u"object", u"of", u"override", u"package", u"private", u"protected", u"public",
    u"readonly", u"require", u"return", u"satisfies", u"set", u"static", u"string", u"super",
    u"switch", u"symbol", u"this", u"throw", u"true", u"try", u"type", u"typeof", u"undefined",
    u"unique", u"unknown", u"var", u"void", u"while", u"with", u"yield",
};

constexpr keyword_set kTypeScriptKeywords[] = {
    {word, word, kTypeScriptWords0, 11},
};

// ---- все языки ----

constexpr language kLanguages[] = {
    {u"Assembler", true, kAssemblerDelimiters, kAssemblerKeywords},
    {u"C", false, kCDelimiters, kCKeywords},
    {u"CSharp", false, kCSharpDelimiters, kCSharpKeywords},
    {u"Erlang", false, kErlangDelimiters, kErlangKeywords},
    {u"Haskell", false, kHaskellDelimiters, kHaskellKeywords},
    {u"IDL", false, kIDLDelimiters, kIDLKeywords},
    {u"Java", false, kJavaDelimiters, kJavaKeywords},
    {u"Lisp", true, kLispDelimiters, kLispKeywords},
    {u"MSIL", true, kMSILDelimiters, kMSILKeywords},
    {u"Nemerle", false, kNemerleDelimiters, kNemerleKeywords},
    {u"Nitra", false, kNitraDelimiters, kNitraKeywords},
    {u"ObjC", false, kObjCDelimiters, kObjCKeywords},
    {u"Ocaml", false, kOcamlDelimiters, kOcamlKeywords},
    {u"Pascal", true, kPascalDelimiters, kPascalKeywords},
    {u"Perl", false, kPerlDelimiters, kPerlKeywords},
    {u"PHP", false, kPHPDelimiters, kPHPKeywords},
    {u"Prolog", true, kPrologDelimiters, kPrologKeywords},
    {u"Python", false, kPythonDelimiters, kPythonKeywords},
    {u"Ruby", false, kRubyDelimiters, kRubyKeywords},
    {u"Rust", false, kRustDelimiters, kRustKeywords},
    {u"SQL", true, kSQLDelimiters, kSQLKeywords},
    {u"VisualBasic", true, kVisualBasicDelimiters, kVisualBasicKeywords},
    {u"XSL", false, kXSLDelimiters, kXSLKeywords},
    {u"Bash", false, kBashDelimiters, kBashKeywords},
    {u"CMake", true, kCMakeDelimiters, kCMakeKeywords},
    {u"CSS", true, kCSSDelimiters, kCSSKeywords},
    {u"Go", false, kGoDelimiters, kGoKeywords},
    {u"JavaScript", false, kJavaScriptDelimiters, kJavaScriptKeywords},
    {u"JSON", false, kJSONDelimiters, kJSONKeywords},
    {u"Lua", false, kLuaDelimiters, kLuaKeywords},
    {u"TypeScript", false, kTypeScriptDelimiters, kTypeScriptKeywords},
};

struct alias_t {
    std::u16string_view tag;
    std::size_t language;  // индекс в kLanguages
};

// Имена тегов кода в нижнем регистре — какой язык.
constexpr alias_t kAliases[] = {
    {u"asm", 0},  // Assembler
    {u"assembly", 0},  // Assembler
    {u"c", 1},  // C
    {u"cpp", 1},  // C
    {u"c++", 1},  // C
    {u"ccode", 1},  // C
    {u"c#", 2},  // CSharp
    {u"cs", 2},  // CSharp
    {u"csharp", 2},  // CSharp
    {u"cscode", 2},  // CSharp
    {u"erlang", 3},  // Erlang
    {u"erl", 3},  // Erlang
    {u"haskell", 4},  // Haskell
    {u"hs", 4},  // Haskell
    {u"idl", 5},  // IDL
    {u"midl", 5},  // IDL
    {u"java", 6},  // Java
    {u"lisp", 7},  // Lisp
    {u"il", 8},  // MSIL
    {u"msil", 8},  // MSIL
    {u"nemerle", 9},  // Nemerle
    {u"nitra", 10},  // Nitra
    {u"objc", 11},  // ObjC
    {u"objectivec", 11},  // ObjC
    {u"ml", 12},  // Ocaml
    {u"ocaml", 12},  // Ocaml
    {u"pascal", 13},  // Pascal
    {u"delphi", 13},  // Pascal
    {u"perl", 14},  // Perl
    {u"php", 15},  // PHP
    {u"prolog", 16},  // Prolog
    {u"py", 17},  // Python
    {u"python", 17},  // Python
    {u"rb", 18},  // Ruby
    {u"ruby", 18},  // Ruby
    {u"rust", 19},  // Rust
    {u"sql", 20},  // SQL
    {u"vb", 21},  // VisualBasic
    {u"vbnet", 21},  // VisualBasic
    {u"vbcode", 21},  // VisualBasic
    {u"vbscript", 21},  // VisualBasic
    {u"vbs", 21},  // VisualBasic
    {u"xml", 22},  // XSL
    {u"xsl", 22},  // XSL
    {u"html", 22},  // XSL
    {u"bash", 23},  // Bash
    {u"sh", 23},  // Bash
    {u"shell", 23},  // Bash
    {u"cmake", 24},  // CMake
    {u"css", 25},  // CSS
    {u"go", 26},  // Go
    {u"golang", 26},  // Go
    {u"js", 27},  // JavaScript
    {u"javascript", 27},  // JavaScript
    {u"jscript", 27},  // JavaScript
    {u"json", 28},  // JSON
    {u"lua", 29},  // Lua
    {u"ts", 30},  // TypeScript
    {u"typescript", 30},  // TypeScript
};

}  // namespace

const language* find_language(std::u16string_view tag) noexcept {
    // Имя тега короткое; сравнение без учёта регистра ASCII, как у самих
    // тегов разметки ([C#] и [c#] — один язык).
    for (const alias_t& alias : kAliases) {
        if (alias.tag.size() != tag.size()) continue;
        bool same = true;
        for (std::size_t i = 0; i < tag.size() && same; ++i) {
            char16_t c = tag[i];
            if (c >= u'A' && c <= u'Z') c = static_cast<char16_t>(c + 32);
            same = c == alias.tag[i];
        }
        if (same) return &kLanguages[alias.language];
    }
    return nullptr;
}

std::span<const language> languages() noexcept { return kLanguages; }

}  // namespace wxl::highlight
