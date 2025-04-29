/*
* Copyright (C) 2024, GrammarSoft ApS
* Developed by Tino Didriksen <mail@tinodidriksen.com>
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

#include "JsonlApplicator.hpp"
#include "Strings.hpp"
#include "Tag.hpp"
#include "Grammar.hpp"
#include "Window.hpp"
#include "SingleWindow.hpp"
#include "Reading.hpp"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"

#include <string>
#include "uextras.hpp"

namespace json = rapidjson;

namespace CG3 {

JsonlApplicator::JsonlApplicator(std::ostream& ux_err)
  : GrammarApplicator(ux_err)
{
}

// Add explicit destructor definition to potentially anchor the vtable
JsonlApplicator::~JsonlApplicator()
{
    // Empty destructor body
}

// Helper to safely get string from JSON, converting to UString
UString json_to_ustring(const json::Value& val) {
	if (val.IsString()) {
		std::string s = val.GetString();
		return UString(s.begin(), s.end());
	}
	return UString();
}

// Helper to convert UString to UTF-8 std::string for JSON serialization
std::string ustring_to_utf8(const UString& ustr) {
	std::string utf8_str;
	utf8_str.reserve(ustr.length() * 3); // Max 3 bytes per UChar in UTF-8
	UErrorCode status = U_ZERO_ERROR;
	int32_t length = 0;
	u_strToUTF8(nullptr, 0, &length, ustr.data(), ustr.length(), &status);
	if (status == U_BUFFER_OVERFLOW_ERROR || status == U_ZERO_ERROR) {
		utf8_str.resize(length);
		status = U_ZERO_ERROR;
		u_strToUTF8(&utf8_str[0], length, nullptr, ustr.data(), ustr.length(), &status);
	}
	if (U_FAILURE(status)) {
		return "";
	}
	utf8_str.resize(strlen(utf8_str.c_str()));
	return utf8_str;
}

// Helper function to parse a single reading (and its potential subreadings) from JSON
Reading* JsonlApplicator::parseJsonReading(const json::Value& reading_obj, Cohort* parentCohort) {
	if (!reading_obj.IsObject()) {
		u_fprintf(ux_stderr, "Error: Expected reading object, but got different type on line %u.\n", numLines);
		return nullptr;
	}
	Reading* cReading = alloc_reading(parentCohort);
	addTagToReading(*cReading, parentCohort->wordform);

	// Parse baseform ("l")
	if (reading_obj.HasMember("l")) {
		const json::Value& l_val = reading_obj["l"];
		UString base_str = json_to_ustring(l_val);
		if (!base_str.empty()) {
			UString base_tag;
			base_tag += '"';
			base_tag += base_str;
			base_tag += '"';
			addTagToReading(*cReading, addTag(base_tag));
		} else {
			u_fprintf(ux_stderr, "Warning: Empty 'l' (baseform) in reading on line %u.\n", numLines);
		}
	} else {
		u_fprintf(ux_stderr, "Warning: Reading missing 'l' (baseform) on line %u.\n", numLines);
	}

	// Parse tags ("ts")
	if (reading_obj.HasMember("ts") && reading_obj["ts"].IsArray()) {
		const json::Value& tags_arr = reading_obj["ts"];
		TagList mappings;
		for (const auto& tag_val : tags_arr.GetArray()) {
			UString tag_str = json_to_ustring(tag_val);
			if (!tag_str.empty()) {
				Tag* tag = addTag(tag_str);
				if (tag->type & T_MAPPING || (!tag_str.empty() && tag_str[0] == grammar->mapping_prefix)) {
					mappings.push_back(tag);
				} else {
					addTagToReading(*cReading, tag);
				}
			}
		}
		if (!mappings.empty()) {
			splitMappings(mappings, *parentCohort, *cReading, true);
		}
	}

	// Parse subreading ("s") recursively
	if (reading_obj.HasMember("s")) {
		const json::Value& sub_reading_val = reading_obj["s"];
		if (sub_reading_val.IsObject()) {
			Reading* subReading = parseJsonReading(sub_reading_val, parentCohort);
			if (subReading) {
				cReading->next = subReading;
			} else {
				u_fprintf(ux_stderr, "Error: Failed to parse subreading object on line %u.\n", numLines);
			}
		} else {
			u_fprintf(ux_stderr, "Warning: Value for 's' (sub_reading) is not an object on line %u. Skipping.\n", numLines);
		}
	}

	// Ensure baseform exists
	if (!cReading->baseform) {
		cReading->baseform = parentCohort->wordform->hash;
		u_fprintf(ux_stderr, "Warning: Reading on line %u ended up with no baseform. Using wordform.\n", numLines);
	}

	return cReading;
}

void JsonlApplicator::parseJsonCohort(const json::Value& obj, SingleWindow* cSWindow, Cohort*& cCohort) {
	cCohort = alloc_cohort(cSWindow);
	cCohort->global_number = gWindow->cohort_counter++;
	numCohorts++;

	UString wform_str;
	if (obj.HasMember("w")) {
		wform_str = json_to_ustring(obj["w"]);
	} else {
		u_fprintf(ux_stderr, "Warning: JSON cohort on line %u missing 'w' (wordform). Using empty.\n", numLines);
	}
	UString wform_tag;
	wform_tag.append(u"\"<");
	wform_tag += wform_str;
	wform_tag.append(u">\"");
	cCohort->wordform = addTag(wform_tag);

	cCohort->wblank.clear();
	if (obj.HasMember("z")) {
		cCohort->text = json_to_ustring(obj["z"]);
	}

	// handle static tags ("sts")
	if (obj.HasMember("sts") && obj["sts"].IsArray()) {
		if (!cCohort->wread) {
			cCohort->wread = alloc_reading(cCohort);
			addTagToReading(*cCohort->wread, cCohort->wordform);
			cCohort->wread->baseform = cCohort->wordform->hash;
		}
		for (const auto& tag_val : obj["sts"].GetArray()) {
			UString tag_str = json_to_ustring(tag_val);
			if (!tag_str.empty()) {
				Tag* tag = addTag(tag_str);
				cCohort->wread->tags_list.push_back(tag->hash);
			}
		}
	}

	if (obj.HasMember("rs") && obj["rs"].IsArray()) {
		const json::Value& readings_arr = obj["rs"];
		for (const auto& reading_val : readings_arr.GetArray()) {
			if (!reading_val.IsObject()) {
				u_fprintf(ux_stderr, "Warning: Non-object found in 'rs' (readings) array on line %u. Skipping.\n", numLines);
				continue;
			}
			Reading* cReading = parseJsonReading(reading_val, cCohort);
			if (cReading) {
				cCohort->appendReading(cReading);
				++numReadings; // Increment only if parsing succeeded
			} else {
				u_fprintf(ux_stderr, "Error: Failed to parse main reading on line %u.\n", numLines);
			}
		}
	}

	if (cCohort->readings.empty()) {
		initEmptyCohort(*cCohort);
	}
	insert_if_exists(cCohort->possible_sets, grammar->sets_any);

	if (obj.HasMember("ds") && obj["ds"].IsUint()) {
		cCohort->dep_self = obj["ds"].GetUint();
	}
	if (obj.HasMember("dp") && obj["dp"].IsUint()) {
		cCohort->dep_parent = obj["dp"].GetUint();
	}

	// parse deleted readings ("drs")
	if (obj.HasMember("drs") && obj["drs"].IsArray()) {
		for (const auto& dr_val : obj["drs"].GetArray()) {
			if (!dr_val.IsObject()) continue;
			Reading* delR = parseJsonReading(dr_val, cCohort);
			if (delR) {
				cCohort->deleted.push_back(delR);
			} else {
				u_fprintf(ux_stderr, "Error: Failed to parse deleted reading on line %u.\n", numLines);
			}
		}
	}
}

void JsonlApplicator::runGrammarOnText(std::istream& input, std::ostream& output) {
	ux_stdin = &input;
	ux_stdout = &output;

	if (!input.good()) {
		u_fprintf(ux_stderr, "Error: Input is null - nothing to parse!\n");
		CG3Quit(1);
	}
	if (input.eof()) {
		u_fprintf(ux_stderr, "Error: Input is empty - nothing to parse!\n");
		CG3Quit(1);
	}
	if (!output) {
		u_fprintf(ux_stderr, "Error: Output is null - cannot write to nothing!\n");
		CG3Quit(1);
	}
	if (!grammar) {
		u_fprintf(ux_stderr, "Error: No grammar provided - cannot continue! Hint: call setGrammar() first.\n");
		CG3Quit(1);
	}

	if (!grammar->delimiters || grammar->delimiters->empty()) {
		if (!grammar->soft_delimiters || grammar->soft_delimiters->empty()) {
			u_fprintf(ux_stderr, "Warning: No soft or hard delimiters defined in grammar. Hard limit of %u cohorts may break windows.\n", hard_limit);
		} else {
			u_fprintf(ux_stderr, "Warning: No hard delimiters defined in grammar. Soft limit of %u cohorts may break windows.\n", soft_limit);
		}
	}

	index();

	uint32_t resetAfter = ((num_windows + 4) * 2 + 1);
	uint32_t lines = 0;

	SingleWindow* cSWindow = nullptr;
	Cohort* cCohort = nullptr;

	gWindow->window_span = num_windows;

	ux_stripBOM(input);

	std::string line_str;
	while (std::getline(input, line_str)) {
		++lines;
		numLines++; // Keep track for warnings

		// Skip empty lines
		if (line_str.empty() || line_str.find_first_not_of(" \t\n\v\f\r") == std::string::npos) {
			continue;
		}

		json::Document doc;
		json::ParseResult ok = doc.Parse(line_str.c_str());

		if (!ok) {
			u_fprintf(ux_stderr, "Warning: Failed to parse JSON on line %u: %s (offset %zu). Skipping line.\n",
					  numLines, json::GetParseError_En(ok.Code()), ok.Offset());
			continue;
		}

		if (!doc.IsObject()) {
			u_fprintf(ux_stderr, "Warning: JSON on line %u is not an object. Skipping line.\n", numLines);
			continue;
		}

		if (!cSWindow) {
			cSWindow = gWindow->allocAppendSingleWindow();
			initEmptySingleWindow(cSWindow);
			++numWindows;
		}

		parseJsonCohort(doc, cSWindow, cCohort);

		if (!cCohort) {
		    u_fprintf(ux_stderr, "Error: Failed to create cohort from JSON on line %u.\n", numLines);
		    continue;
		}

		cSWindow->appendCohort(cCohort);

		bool did_delim = false;
		if (cSWindow->cohorts.size() >= soft_limit && grammar->soft_delimiters && doesSetMatchCohortNormal(*cCohort, grammar->soft_delimiters->number)) {
			if (verbosity_level > 0) {
				u_fprintf(ux_stderr, "Info: Soft limit of %u cohorts reached at line %u with soft delimiter.\n", soft_limit, numLines);
			}
			for (auto iter : cCohort->readings) {
				addTagToReading(*iter, endtag);
			}
			cSWindow = nullptr;
			cCohort = nullptr;
			did_delim = true;
		} else if (cSWindow->cohorts.size() >= hard_limit || (grammar->delimiters && doesSetMatchCohortNormal(*cCohort, grammar->delimiters->number))) {
			if (cSWindow->cohorts.size() >= hard_limit) {
				u_fprintf(ux_stderr, "Warning: Hard limit of %u cohorts reached at line %u - forcing break.\n", hard_limit, numLines);
			}
			for (auto iter : cCohort->readings) {
				addTagToReading(*iter, endtag);
			}
			cSWindow = nullptr;
			cCohort = nullptr;
			did_delim = true;
		}

		if (did_delim || gWindow->next.size() > num_windows) {
			gWindow->shuffleWindowsDown();
			runGrammarOnWindow();
			if (numWindows % resetAfter == 0) {
				resetIndexes();
			}
			if (verbosity_level > 0) {
				u_fprintf(ux_stderr, "Progress: L:%u, W:%u, C:%u, R:%u\r", lines, numWindows, numCohorts, numReadings);
				u_fflush(ux_stderr);
			}
		}
        cCohort = nullptr;
	}

	if (cSWindow && !cSWindow->cohorts.empty()) {
        Cohort* lastCohort = cSWindow->cohorts.back();
        for (auto iter : lastCohort->readings) {
            addTagToReading(*iter, endtag);
        }
    }

	while (!gWindow->next.empty()) {
		gWindow->shuffleWindowsDown();
		runGrammarOnWindow();
	}
    if(gWindow->current) {
        runGrammarOnWindow();
    }

	gWindow->shuffleWindowsDown();
	while (!gWindow->previous.empty()) {
		SingleWindow* tmp = gWindow->previous.front();
		printSingleWindow(tmp, output);
		free_swindow(tmp);
		gWindow->previous.erase(gWindow->previous.begin());
	}

	u_fflush(output);
	if (verbosity_level > 0) {
		u_fprintf(ux_stderr, "Progress: L:%u, W:%u, C:%u, R:%u - Done.\n", lines, numWindows, numCohorts, numReadings);
		u_fflush(ux_stderr);
	}
}

void JsonlApplicator::buildJsonTags(const Reading* reading, json::Value& tags_json, json::Document::AllocatorType& allocator) {
	assert(tags_json.IsArray());

	uint32SortedVector unique;
	for (auto tter : reading->tags_list) {
		if ((!show_end_tags && tter == endtag) || tter == begintag) {
			continue;
		}
		if (tter == reading->baseform || (reading->parent && tter == reading->parent->wordform->hash)) {
			continue;
		}

		if (unique_tags) {
			if (unique.find(tter) != unique.end()) {
				continue;
			}
			unique.insert(tter);
		}

		const Tag* tag = grammar->single_tags[tter];

		if (tag->type & T_DEPENDENCY && has_dep && !dep_original) {
			continue;
		}
		if (tag->type & T_RELATION && has_relations) {
			continue;
		}

		std::string utf8_tag = ustring_to_utf8(tag->tag);
		json::Value tag_val(utf8_tag.c_str(), allocator);
		tags_json.PushBack(tag_val, allocator);
	}
}

void JsonlApplicator::buildJsonReading(const Reading* reading, json::Value& reading_json, json::Document::AllocatorType& allocator) {
	assert(reading_json.IsObject());

	std::string baseform_utf8 = "";
	if (reading->baseform) {
		auto it = grammar->single_tags.find(reading->baseform);
		if (it != grammar->single_tags.end()) {
			auto& tag = it->second->tag;
			if (tag.size() >= 2 && tag.front() == '"' && tag.back() == '"') {
				baseform_utf8 = ustring_to_utf8(tag.substr(1, tag.size() - 2));
			} else {
				baseform_utf8 = ustring_to_utf8(tag);
			}
		}
	}
	json::Value l_val(baseform_utf8.c_str(), allocator);
	reading_json.AddMember("l", l_val, allocator);

	json::Value tags_json(json::kArrayType);
	buildJsonTags(reading, tags_json, allocator);
	if (!tags_json.Empty()) {
		reading_json.AddMember("ts", tags_json, allocator);
	}

	if (reading->next) {
		json::Value sub_reading_obj(json::kObjectType);
		buildJsonReading(reading->next, sub_reading_obj, allocator); // Recursively build next reading
		if (!sub_reading_obj.ObjectEmpty()) {
			reading_json.AddMember("s", sub_reading_obj, allocator);
		}
	}
}

void JsonlApplicator::printCohort(Cohort* cohort, std::ostream& output, bool profiling) {
	if (cohort->local_number == 0 || (cohort->type & CT_REMOVED)) {
		return;
	}

	if (!profiling) {
		cohort->unignoreAll();
	}

	json::Document doc;
	doc.SetObject();
	json::Document::AllocatorType& allocator = doc.GetAllocator();

	auto& wform_tag = cohort->wordform->tag;
	std::string wform_utf8;
	if (wform_tag.size() >= 4 && wform_tag.substr(0, 2) == u"\"<" && wform_tag.substr(wform_tag.size() - 2) == u">\"") {
		wform_utf8 = ustring_to_utf8(wform_tag.substr(2, wform_tag.size() - 4));
	} else {
		wform_utf8 = ustring_to_utf8(wform_tag);
	}
	json::Value w_val(wform_utf8.c_str(), allocator);
	doc.AddMember("w", w_val, allocator);

	if (cohort->wread && !cohort->wread->tags_list.empty()) {
		json::Value static_tags_json(json::kArrayType);
		uint32SortedVector unique_sts;
		for (const auto& tag_hash : cohort->wread->tags_list) {
			if (cohort->wordform && tag_hash == cohort->wordform->hash) {
				continue;
			}
			if (unique_tags) {
				if (unique_sts.find(tag_hash) != unique_sts.end()) {
					continue;
				}
				unique_sts.insert(tag_hash);
			}

			auto it = grammar->single_tags.find(tag_hash);
			if (it != grammar->single_tags.end()) {
				const Tag* tag_ptr = it->second;
				if (tag_ptr) {
					std::string sts_tag_utf8 = ustring_to_utf8(tag_ptr->tag);
					json::Value sts_tag_val(sts_tag_utf8.c_str(), allocator);
					static_tags_json.PushBack(sts_tag_val, allocator);
				}
			}
		}
		if (!static_tags_json.Empty()) {
			doc.AddMember("sts", static_tags_json, allocator);
		}
	}

	if (!cohort->text.empty()) {
		UString z_text = cohort->text;
		if (!z_text.empty() && z_text.back() == u'\n') {
			z_text.pop_back();
		}
        if (!z_text.empty()) {
            std::string z_utf8 = ustring_to_utf8(z_text);
            json::Value z_val(z_utf8.c_str(), allocator);
            doc.AddMember("z", z_val, allocator);
        }
	}

	if (has_dep && !(cohort->type & CT_REMOVED)) {
		uint32_t self_id = (cohort->dep_self == 0) ? cohort->global_number : cohort->dep_self;
		doc.AddMember("ds", self_id, allocator);
		if (cohort->dep_parent != DEP_NO_PARENT) {
			doc.AddMember("dp", cohort->dep_parent, allocator);
		}
	}

	ReadingList* readings_to_print = &cohort->readings;
	std::sort(readings_to_print->begin(), readings_to_print->end(), Reading::cmp_number);

	json::Value readings_json(json::kArrayType);
	for (const auto& reading : *readings_to_print) {
		if (reading->noprint) {
			continue;
		}
		json::Value reading_json(json::kObjectType);
		buildJsonReading(reading, reading_json, allocator);
		if (!reading_json.ObjectEmpty()) {
			readings_json.PushBack(reading_json, allocator);
		}

		if (!profiling) {
			// In non-profiling mode, typically only the first (best) reading is printed.
			// The schema allows multiple readings, so we keep this behavior for now.
			// If only the single best reading should be output, uncomment the break.
			// break;
		}
	}
	if (!readings_json.Empty()) {
		doc.AddMember("rs", readings_json, allocator);
	}

	if (!cohort->deleted.empty()) {
		json::Value deleted_readings_json(json::kArrayType);
		std::sort(cohort->deleted.begin(), cohort->deleted.end(), Reading::cmp_number);
		for (const auto& reading : cohort->deleted) {
			// TODO Assuming deleted readings should always be included if present, regardless of noprint flag?
			json::Value reading_json(json::kObjectType);
			buildJsonReading(reading, reading_json, allocator);
			if (!reading_json.ObjectEmpty()) {
				deleted_readings_json.PushBack(reading_json, allocator);
			}
		}
		if (!deleted_readings_json.Empty()) {
			doc.AddMember("drs", deleted_readings_json, allocator);
		}
	}

	json::StringBuffer buffer;
	json::Writer<json::StringBuffer> writer(buffer);
	doc.Accept(writer);

	u_fprintf(output, "%s\n", buffer.GetString());
}

void JsonlApplicator::printSingleWindow(SingleWindow* window, std::ostream& output, bool profiling) {
	if (!window->text.empty()) {
		u_fprintf(ux_stderr, "Warning: Window-level text found and ignored during JSONL output.\n");
	}

	for (auto& cohort : window->all_cohorts) {
		printCohort(cohort, output, profiling);
	}

	if (!window->text_post.empty()) {
		u_fprintf(ux_stderr, "Warning: Window-level post-text found and ignored during JSONL output.\n");
	}
}

} // namespace CG3
