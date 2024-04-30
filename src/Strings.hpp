/*
* Copyright (C) 2007-2024, GrammarSoft ApS
* Developed by Tino Didriksen <mail@tinodidriksen.com>
* Design by Eckhard Bick <eckhard.bick@mail.dk>, Tino Didriksen <mail@tinodidriksen.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this progam.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once
#ifndef c6d28b7452ec699b_STRINGS_H
#define c6d28b7452ec699b_STRINGS_H

#include <cstdint>

namespace CG3 {
// ToDo: Add ABORT
enum KEYWORDS : uint32_t {
	K_IGNORE,
	K_SETS,
	K_LIST,
	K_SET,
	K_DELIMITERS,
	K_SOFT_DELIMITERS,
	K_PREFERRED_TARGETS,
	K_MAPPING_PREFIX,
	K_MAPPINGS,
	K_CONSTRAINTS,
	K_CORRECTIONS,
	K_SECTION,
	K_BEFORE_SECTIONS,
	K_AFTER_SECTIONS,
	K_NULL_SECTION,
	K_ADD,
	K_MAP,
	K_REPLACE,
	K_SELECT,
	K_REMOVE,
	K_IFF,
	K_APPEND,
	K_SUBSTITUTE,
	K_START,
	K_END,
	K_ANCHOR,
	K_EXECUTE,
	K_JUMP,
	K_REMVARIABLE,
	K_SETVARIABLE,
	K_DELIMIT,
	K_MATCH,
	K_SETPARENT,
	K_SETCHILD,
	K_ADDRELATION,
	K_SETRELATION,
	K_REMRELATION,
	K_ADDRELATIONS,
	K_SETRELATIONS,
	K_REMRELATIONS,
	K_TEMPLATE,
	K_MOVE,
	K_MOVE_AFTER,
	K_MOVE_BEFORE,
	K_SWITCH,
	K_REMCOHORT,
	K_STATIC_SETS,
	K_UNMAP,
	K_COPY,
	K_ADDCOHORT,
	K_ADDCOHORT_AFTER,
	K_ADDCOHORT_BEFORE,
	K_EXTERNAL,
	K_EXTERNAL_ONCE,
	K_EXTERNAL_ALWAYS,
	K_OPTIONS,
	K_STRICT_TAGS,
	K_REOPEN_MAPPINGS,
	K_SUBREADINGS,
	K_SPLITCOHORT,
	K_PROTECT,
	K_UNPROTECT,
	K_MERGECOHORTS,
	K_RESTORE,
	K_WITH,
	K_OLIST,
	K_OSET,
	K_CMDARGS,
	K_CMDARGS_OVERRIDE,
	KEYWORD_COUNT,
};

enum : uint32_t {
	S_IGNORE,
	S_OR = 3,
	S_PLUS,
	S_MINUS,
	S_FAILFAST = 8,
	S_SET_DIFF,
	S_SET_ISECT_U,
	S_SET_SYMDIFF_U,
};

// This must be kept in lock-step with Rule.hpp's RULE_FLAGS
enum : uint32_t {
	FL_NEAREST,
	FL_ALLOWLOOP,
	FL_DELAYED,
	FL_IMMEDIATE,
	FL_LOOKDELETED,
	FL_LOOKDELAYED,
	FL_UNSAFE,
	FL_SAFE,
	FL_REMEMBERX,
	FL_RESETX,
	FL_KEEPORDER,
	FL_VARYORDER,
	FL_ENCL_INNER,
	FL_ENCL_OUTER,
	FL_ENCL_FINAL,
	FL_ENCL_ANY,
	FL_ALLOWCROSS,
	FL_WITHCHILD,
	FL_NOCHILD,
	FL_ITERATE,
	FL_NOITERATE,
	FL_UNMAPLAST,
	FL_REVERSE,
	FL_SUB,
	FL_OUTPUT,
	FL_CAPTURE_UNIF,
	FL_REPEAT,
	FL_BEFORE,
	FL_AFTER,
	FL_IGNORED,
	FL_LOOKIGNORED,
	FL_NOMAPPED,
	FL_NOPARENT,
	FLAGS_COUNT,
};
}

#include "stdafx.hpp"

namespace CG3 {
constexpr UStringView g_flags[FLAGS_COUNT] = {
	u"NEAREST",
	u"ALLOWLOOP",
	u"DELAYED",
	u"IMMEDIATE",
	u"LOOKDELETED",
	u"LOOKDELAYED",
	u"UNSAFE",
	u"SAFE",
	u"REMEMBERX",
	u"RESETX",
	u"KEEPORDER",
	u"VARYORDER",
	u"ENCL_INNER",
	u"ENCL_OUTER",
	u"ENCL_FINAL",
	u"ENCL_ANY",
	u"ALLOWCROSS",
	u"WITHCHILD",
	u"NOCHILD",
	u"ITERATE",
	u"NOITERATE",
	u"UNMAPLAST",
	u"REVERSE",
	u"SUB",
	u"OUTPUT",
	u"CAPTURE_UNIF",
	u"REPEAT",
	u"BEFORE",
	u"AFTER",
	u"IGNORED",
	u"LOOKIGNORED",
	u"NOMAPPED",
	u"NOPARENT",
};

constexpr UStringView keywords[KEYWORD_COUNT] = {
	u"__CG3_DUMMY_KEYWORD__",
	u"SETS",
	u"LIST",
	u"SET",
	u"DELIMITERS",
	u"SOFT-DELIMITERS",
	u"PREFERRED-TARGETS",
	u"MAPPING-PREFIX",
	u"MAPPINGS",
	u"CONSTRAINTS",
	u"CORRECTIONS",
	u"SECTION",
	u"BEFORE-SECTIONS",
	u"AFTER-SECTIONS",
	u"NULL-SECTION",
	u"ADD",
	u"MAP",
	u"REPLACE",
	u"SELECT",
	u"REMOVE",
	u"IFF",
	u"APPEND",
	u"SUBSTITUTE",
	u"START",
	u"END",
	u"ANCHOR",
	u"EXECUTE",
	u"JUMP",
	u"REMVARIABLE",
	u"SETVARIABLE",
	u"DELIMIT",
	u"MATCH",
	u"SETPARENT",
	u"SETCHILD",
	u"ADDRELATION",
	u"SETRELATION",
	u"REMRELATION",
	u"ADDRELATIONS",
	u"SETRELATIONS",
	u"REMRELATIONS",
	u"TEMPLATE",
	u"MOVE",
	u"MOVE-AFTER",
	u"MOVE-BEFORE",
	u"SWITCH",
	u"REMCOHORT",
	u"STATIC-SETS",
	u"UNMAP",
	u"COPY",
	u"ADDCOHORT",
	u"ADDCOHORT-AFTER",
	u"ADDCOHORT-BEFORE",
	u"EXTERNAL",
	u"EXTERNAL-ONCE",
	u"EXTERNAL-ALWAYS",
	u"OPTIONS",
	u"STRICT-TAGS",
	u"REOPEN-MAPPINGS",
	u"SUBREADINGS",
	u"SPLITCOHORT",
	u"PROTECT",
	u"UNPROTECT",
	u"MERGECOHORTS",
	u"RESTORE",
	u"WITH",
	u"OLIST",
	u"OSET",
	u"CMDARGS",
	u"CMDARGS-OVERRIDE",
};

constexpr UStringView stringbits[] = {
	u"",
	u"",
	u"",
	u"OR",
	u"+",
	u"-",
	u"",
	u"",
	u"^",
};

constexpr UStringView
	STR_TARGET{ u"TARGET" },
	STR_AND{ u"AND" },
	STR_IF{ u"IF" },
	STR_OR{ u"OR" },
	STR_TEXTNOT{ u"NOT" },
	STR_TEXTNEGATE{ u"NEGATE" },
	STR_ALL{ u"ALL" },
	STR_NONE{ u"NONE" },
	STR_LINK{ u"LINK" },
	STR_TO{ u"TO" },
	STR_FROM{ u"FROM" },
	STR_AFTER{ u"AFTER" },
	STR_BEFORE{ u"BEFORE" },
	STR_WITH{ u"WITH" },
	STR_ONCE{ u"ONCE" },
	STR_ALWAYS{ u"ALWAYS" },
	STR_EXCEPT{ u"EXCEPT" },
	STR_STATIC{ u"STATIC" },
	STR_ASTERIK{ u"*" },
	STR_BARRIER{ u"BARRIER" },
	STR_CBARRIER{ u"CBARRIER" },
	STR_CMD_FLUSH{ u"<STREAMCMD:FLUSH>" },
	STR_CMD_EXIT{ u"<STREAMCMD:EXIT>" },
	STR_CMD_IGNORE{ u"<STREAMCMD:IGNORE>" },
	STR_CMD_RESUME{ u"<STREAMCMD:RESUME>" },
	STR_CMD_SETVAR{ u"<STREAMCMD:SETVAR:" },
	STR_CMD_REMVAR{ u"<STREAMCMD:REMVAR:" },
	STR_DELIMITSET{ u"_S_DELIMITERS_" },
	STR_SOFTDELIMITSET{ u"_S_SOFT_DELIMITERS_" },
	STR_TEXTDELIMITSET{ u"_S_TEXT_DELIMITERS_" },
	STR_TEXTDELIM_DEFAULT{ u"/(^|\\n)</s/r" },
	STR_BEGINTAG{ u">>>" },
	STR_ENDTAG{ u"<<<" },
	STR_UU_LEFT{ u"_LEFT_" },
	STR_UU_RIGHT{ u"_RIGHT_" },
	STR_UU_PAREN{ u"_PAREN_" },
	STR_UU_TARGET{ u"_TARGET_" },
	STR_UU_MARK{ u"_MARK_" },
	STR_UU_ATTACHTO{ u"_ATTACHTO_" },
	STR_UU_ENCL{ u"_ENCL_" },
	STR_UU_SAME_BASIC{ u"_SAME_BASIC_" },
	STR_UU_C1{ u"_C1_" },
	STR_UU_C2{ u"_C2_" },
	STR_UU_C3{ u"_C3_" },
	STR_UU_C4{ u"_C4_" },
	STR_UU_C5{ u"_C5_" },
	STR_UU_C6{ u"_C6_" },
	STR_UU_C7{ u"_C7_" },
	STR_UU_C8{ u"_C8_" },
	STR_UU_C9{ u"_C9_" },
	STR_RXTEXT_ANY{ u"<.*>" },
	STR_RXBASE_ANY{ u"\".*\"" },
	STR_RXWORD_ANY{ u"\"<.*>\"" },
	STR_VS1{ u"$1" },
	STR_VS2{ u"$2" },
	STR_VS3{ u"$3" },
	STR_VS4{ u"$4" },
	STR_VS5{ u"$5" },
	STR_VS6{ u"$6" },
	STR_VS7{ u"$7" },
	STR_VS8{ u"$8" },
	STR_VS9{ u"$9" },
	STR_VSu_raw{ u"%u" },
	STR_VSU_raw{ u"%U" },
	STR_VSl_raw{ u"%l" },
	STR_VSL_raw{ u"%L" },
	STR_VSu{ u"\x01u" },
	STR_VSU{ u"\x01U" },
	STR_VSl{ u"\x01l" },
	STR_VSL{ u"\x01L" },
	STR_GPREFIX{ u"_G_" },
	STR_POSITIVE{ u"POSITIVE" },
	STR_NEGATIVE{ u"NEGATIVE" },
	STR_NO_ISETS{ u"no-inline-sets" },
	STR_NO_ITMPLS{ u"no-inline-templates" },
	STR_STRICT_WFORMS{ u"strict-wordforms" },
	STR_STRICT_BFORMS{ u"strict-baseforms" },
	STR_STRICT_SECOND{ u"strict-secondary" },
	STR_STRICT_REGEX{ u"strict-regex" },
	STR_STRICT_ICASE{ u"strict-icase" },
	STR_SELF_NO_BARRIER{ u"self-no-barrier" },
	STR_ORDERED{ u"ordered" },
	STR_ADDCOHORT_ATTACH{ u"addcohort-attach" },
	STR_SAFE_SETPARENT{ u"safe-setparent" },
	STR_DUMMY{ u"__CG3_DUMMY_STRINGBIT__" }
	;

constexpr size_t CG3_BUFFER_SIZE = 8192;
constexpr size_t NUM_GBUFFERS = 1;
CG3_IMPORTS extern std::vector<UString> gbuffers;
constexpr size_t NUM_CBUFFERS = 1;
CG3_IMPORTS extern std::vector<std::string> cbuffers;

constexpr UChar not_sign = u'\u00AC';
}

#endif
