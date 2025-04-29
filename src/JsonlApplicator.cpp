/*
* Copyright (C) 2024, GrammarSoft ApS
* Developed by Tino Didriksen <mail@tinodidriksen.com>
* Based on contributions from GitHub Copilot
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

#include <boost/json.hpp>
#include <boost/json/src.hpp>
#include <string>
#include "uextras.hpp"

namespace json = boost::json;

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
UString json_to_ustring(const json::value& val) {
	if (val.is_string()) {
		std::string s = val.get_string().c_str();
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
Reading* JsonlApplicator::parseJsonReading(const json::object& reading_obj, Cohort* parentCohort, Reading* parentReading /*= nullptr*/) {
	Reading* cReading = alloc_reading(parentCohort);
	if (parentReading) {
		parentReading->next = cReading; // Link subreading
	}
	addTagToReading(*cReading, parentCohort->wordform); // Add wordform tag by default

	// Parse baseform ("l")
	if (reading_obj.contains("l")) {
		UString base_str = json_to_ustring(reading_obj.at("l"));
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
	if (reading_obj.contains("ts") && reading_obj.at("ts").is_array()) {
		const json::array& tags_arr = reading_obj.at("ts").get_array();
		TagList mappings;
		for (const auto& tag_val : tags_arr) {
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

	// Parse subreadings ("s") recursively
	if (reading_obj.contains("s") && reading_obj.at("s").is_array()) {
		const json::array& sub_readings_arr = reading_obj.at("s").get_array();
		Reading* currentSubReading = cReading; // Start linking from the parent reading
		for (const auto& sub_reading_val : sub_readings_arr) {
			if (sub_reading_val.is_object()) {
				// Recursively parse the subreading and link it
				currentSubReading = parseJsonReading(sub_reading_val.get_object(), parentCohort, currentSubReading);
				if (!currentSubReading) { // Handle potential error from recursive call
					u_fprintf(ux_stderr, "Error: Failed to parse subreading on line %u.\n", numLines);
					// Decide how to proceed: skip this subreading, stop parsing this branch, etc.
					// For now, we just break the subreading chain here.
					break;
				}
			} else {
				u_fprintf(ux_stderr, "Warning: Non-object found in 's' (sub_readings) array on line %u. Skipping.\n", numLines);
			}
		}
	}

	// Ensure baseform exists
	if (!cReading->baseform) {
		cReading->baseform = parentCohort->wordform->hash;
		u_fprintf(ux_stderr, "Warning: Reading on line %u ended up with no baseform. Using wordform.\n", numLines);
	}

	return cReading;
}

void JsonlApplicator::parseJsonCohort(const json::object& obj, SingleWindow* cSWindow, Cohort*& cCohort) {
	cCohort = alloc_cohort(cSWindow);
	cCohort->global_number = gWindow->cohort_counter++;
	numCohorts++;

	UString wform_str;
	if (obj.contains("w")) {
		wform_str = json_to_ustring(obj.at("w"));
	} else {
		u_fprintf(ux_stderr, "Warning: JSON cohort on line %u missing 'w' (wordform). Using empty.\n", numLines);
	}
	UString wform_tag;
	wform_tag.append(u"\"<");
	wform_tag += wform_str;
	wform_tag.append(u">\"");
	cCohort->wordform = addTag(wform_tag);

	cCohort->wblank.clear();
	if (obj.contains("z")) {
		cCohort->text = json_to_ustring(obj.at("z"));
	}

	// handle static tags ("sts")
	if (obj.contains("sts") && obj.at("sts").is_array()) {
		// Ensure wread exists if we have static tags
		if (!cCohort->wread) {
			cCohort->wread = alloc_reading(cCohort);
			// Add wordform tag to wread as well
			addTagToReading(*cCohort->wread, cCohort->wordform);
			// Set baseform for wread, typically the wordform itself
			cCohort->wread->baseform = cCohort->wordform->hash;
		}
		for (const auto& tag_val : obj.at("sts").get_array()) {
			UString tag_str = json_to_ustring(tag_val);
			if (!tag_str.empty()) {
				Tag* tag = addTag(tag_str);
				// Add the static tag hash to the wread's tag list
				cCohort->wread->tags_list.push_back(tag->hash);
			}
		}
	}

	if (obj.contains("rs") && obj.at("rs").is_array()) {
		const json::array& readings_arr = obj.at("rs").get_array();
		for (const auto& reading_val : readings_arr) {
			if (!reading_val.is_object()) {
				u_fprintf(ux_stderr, "Warning: Non-object found in 'rs' (readings) array on line %u. Skipping.\n", numLines);
				continue;
			}
			const json::object& reading_obj = reading_val.get_object();
			// Use the new helper function to parse the reading and its subreadings
			Reading* cReading = parseJsonReading(reading_obj, cCohort);
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

	// restore dependency fields ("ds","dp")
	if (obj.contains("ds")) {
		cCohort->dep_self = obj.at("ds").to_number<uint32_t>();
	}
	if (obj.contains("dp")) {
		cCohort->dep_parent = obj.at("dp").to_number<uint32_t>();
	}

	// parse deleted readings ("drs")
	if (obj.contains("drs") && obj.at("drs").is_array()) {
		for (const auto& dr_val : obj.at("drs").get_array()) {
			if (!dr_val.is_object()) continue;
			const auto& dr_obj = dr_val.get_object();
			// Use the helper function for deleted readings too
			Reading* delR = parseJsonReading(dr_obj, cCohort);
			if (delR) {
				cCohort->deleted.push_back(delR);
			} else {
				u_fprintf(ux_stderr, "Error: Failed to parse deleted reading on line %u.\n", numLines);
			}
		}
	}
}

// Add the missing definition for runGrammarOnText
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

	// Delimiter warnings (less critical for JSONL as structure is explicit per line)
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

		// Parse JSON line
		json::value jv;
		json::error_code ec;
		jv = json::parse(line_str, ec); // Correct call to json::parse

		if (ec) {
			u_fprintf(ux_stderr, "Warning: Failed to parse JSON on line %u: %s. Skipping line.\n", numLines, ec.message().c_str());
			// Output the invalid line as-is? Or just skip? Skipping for now.
			// u_fprintf(output, "%s\n", line_str.c_str());
			continue;
		}

		if (!jv.is_object()) {
			u_fprintf(ux_stderr, "Warning: JSON on line %u is not an object. Skipping line.\n", numLines);
			// u_fprintf(output, "%s\n", line_str.c_str());
			continue;
		}

		const json::object& obj = jv.get_object();

		// --- Window Management ---
		// Create a new window for each JSON line/cohort? Or group them?
		// Assuming each JSON line represents one cohort and we group them into windows based on limits.
		if (!cSWindow) {
			cSWindow = gWindow->allocAppendSingleWindow();
			initEmptySingleWindow(cSWindow);
			++numWindows;
		}

		// Parse the JSON object into a Cohort
		parseJsonCohort(obj, cSWindow, cCohort); // cCohort is updated by reference

		if (!cCohort) { // Should not happen if parseJsonCohort succeeds
		    u_fprintf(ux_stderr, "Error: Failed to create cohort from JSON on line %u.\n", numLines);
		    continue;
		}

		cSWindow->appendCohort(cCohort);

		// --- Delimiter Logic ---
		bool did_delim = false;
		// Soft Delimiter Check
		if (cSWindow->cohorts.size() >= soft_limit && grammar->soft_delimiters && doesSetMatchCohortNormal(*cCohort, grammar->soft_delimiters->number)) {
			if (verbosity_level > 0) {
				u_fprintf(ux_stderr, "Info: Soft limit of %u cohorts reached at line %u with soft delimiter.\n", soft_limit, numLines);
			}
			for (auto iter : cCohort->readings) {
				addTagToReading(*iter, endtag);
			}
			cSWindow = nullptr; // Force creation of a new window next iteration
			cCohort = nullptr;
			did_delim = true;
		}
		// Hard Delimiter Check
		else if (cSWindow->cohorts.size() >= hard_limit || (grammar->delimiters && doesSetMatchCohortNormal(*cCohort, grammar->delimiters->number))) {
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

		// --- Process Windows ---
		if (did_delim || gWindow->next.size() > num_windows) {
			// If we delimited, the current cSWindow is finished (set to nullptr).
            // If the buffer is full (> num_windows), process the oldest one.
			gWindow->shuffleWindowsDown();
			runGrammarOnWindow(); // Processes the window now in gWindow->current
			if (numWindows % resetAfter == 0) {
				resetIndexes();
			}
			if (verbosity_level > 0) {
				u_fprintf(ux_stderr, "Progress: L:%u, W:%u, C:%u, R:%u\r", lines, numWindows, numCohorts, numReadings);
				u_fflush(ux_stderr);
			}
		}
        cCohort = nullptr; // Reset cCohort after it's added or window is processed
	} // End while getline

	// Process remaining windows
	if (cSWindow && !cSWindow->cohorts.empty()) {
        // Add endtag to the last cohort of the last window
        Cohort* lastCohort = cSWindow->cohorts.back();
        for (auto iter : lastCohort->readings) {
            addTagToReading(*iter, endtag);
        }
    }

	while (!gWindow->next.empty()) {
		gWindow->shuffleWindowsDown();
		runGrammarOnWindow();
	}
    // Process the final 'current' window if it exists
    if(gWindow->current) {
        runGrammarOnWindow();
    }

	// Print all processed windows
	gWindow->shuffleWindowsDown(); // Move final window to previous if needed
	while (!gWindow->previous.empty()) {
		SingleWindow* tmp = gWindow->previous.front();
		printSingleWindow(tmp, output);
		free_swindow(tmp);
		gWindow->previous.erase(gWindow->previous.begin());
	}

	u_fflush(output);
	if (verbosity_level > 0) { // Final progress update
		u_fprintf(ux_stderr, "Progress: L:%u, W:%u, C:%u, R:%u - Done.\n", lines, numWindows, numCohorts, numReadings);
		u_fflush(ux_stderr);
	}
}

void JsonlApplicator::buildJsonTags(const Reading* reading, json::array& tags_json) {
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

		tags_json.emplace_back(ustring_to_utf8(tag->tag));
	}
}

void JsonlApplicator::buildJsonReading(const Reading* reading, json::object& reading_json) {
	if (reading->baseform) {
		auto it = grammar->single_tags.find(reading->baseform);
		if (it != grammar->single_tags.end()) {
			auto& tag = it->second->tag;
			if (tag.size() >= 2 && tag.front() == '"' && tag.back() == '"') {
				reading_json["l"] = ustring_to_utf8(tag.substr(1, tag.size() - 2));
			} else {
				reading_json["l"] = ustring_to_utf8(tag);
			}
		} else {
			reading_json["l"] = "";
		}
	} else {
		reading_json["l"] = "";
	}

	json::array tags_json;
	buildJsonTags(reading, tags_json);
	reading_json["ts"] = std::move(tags_json);

	if (reading->next) {
		json::array sub_readings_json;
		const Reading* sub = reading->next;
		while (sub) {
			json::object sub_reading_obj;
			buildJsonReading(sub, sub_reading_obj);
			sub_readings_json.emplace_back(std::move(sub_reading_obj));
			sub = sub->next;
		}
		if (!sub_readings_json.empty()) {
			reading_json["s"] = std::move(sub_readings_json);
		}
	}
}

void JsonlApplicator::printCohort(Cohort* cohort, std::ostream& output, bool profiling) {
	if (cohort->local_number == 0 || (cohort->type & CT_REMOVED)) {
		// Removed cohorts are not printed in JSONL format.
		// Consider how to handle cohort->text if it contains relevant whitespace.
		return;
	}

	if (!profiling) {
		cohort->unignoreAll();
		// Merging mappings might affect tag output; assume handled before print.
	}

	json::object cohort_json;

	// Wordform ("w")
	auto& wform_tag = cohort->wordform->tag;
	if (wform_tag.size() >= 4 && wform_tag.substr(0, 2) == u"\"<" && wform_tag.substr(wform_tag.size() - 2) == u">\"") {
		cohort_json["w"] = ustring_to_utf8(wform_tag.substr(2, wform_tag.size() - 4));
	} else {
		cohort_json["w"] = ustring_to_utf8(wform_tag); // Fallback
	}

	// Static Tags ("sts") - Optional, from cohort->wread
	if (cohort->wread && !cohort->wread->tags_list.empty()) {
		json::array static_tags_json;
		uint32SortedVector unique_sts; // Use unique set for static tags too
		for (const auto& tag_hash : cohort->wread->tags_list) {
			// Skip the wordform itself if it was added to wread
			if (cohort->wordform && tag_hash == cohort->wordform->hash) {
				continue;
			}
			// Ensure uniqueness if required
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
					static_tags_json.emplace_back(ustring_to_utf8(tag_ptr->tag));
				}
			}
		}
		if (!static_tags_json.empty()) {
			cohort_json["sts"] = std::move(static_tags_json);
		}
	}

	// Text suffix ("z") - Optional, trim final newline
	if (!cohort->text.empty()) {
		UString z_text = cohort->text;
		// Remove trailing '\n'
		if (!z_text.empty() && z_text.back() == u'\n') {
			z_text.pop_back();
		}
		cohort_json["z"] = ustring_to_utf8(z_text);
	}

	// Dependency relations: only if dependencies enabled and cohort not removed
	if (has_dep && !(cohort->type & CT_REMOVED)) {
		// ensure self‐ID
		if (cohort->dep_self == 0) {
			cohort_json["ds"] = cohort->global_number;
		} else {
			cohort_json["ds"] = cohort->dep_self;
			if (cohort->dep_parent != DEP_NO_PARENT) {
				cohort_json["dp"] = cohort->dep_parent;
			}
		}
	}

	// Readings ("rs")
	json::array readings_json;
	ReadingList* readings_to_print = &cohort->readings;

	// Sort active readings for consistent output
	std::sort(readings_to_print->begin(), readings_to_print->end(), Reading::cmp_number);

	for (const auto& reading : *readings_to_print) {
		if (reading->noprint) { // Skip readings marked as noprint
			continue;
		}
		json::object reading_json;
		buildJsonReading(reading, reading_json); // Build the reading recursively
		readings_json.emplace_back(std::move(reading_json));

		if (!profiling) {
			// In non-profiling mode, typically only the first (best) reading is printed.
			// The schema allows multiple readings, so we keep this behavior for now.
			// If only the single best reading should be output, uncomment the break.
			// break;
		}
	}
	// Only add "rs" if there are readings to print
	if (!readings_json.empty()) {
		cohort_json["rs"] = std::move(readings_json);
	}

	// Deleted Readings ("drs") - Optional
	// Often populated when trace is enabled or during specific rule applications.
	if (!cohort->deleted.empty()) {
		json::array deleted_readings_json;
		// Sort deleted readings for consistent output
		std::sort(cohort->deleted.begin(), cohort->deleted.end(), Reading::cmp_number);
		for (const auto& reading : cohort->deleted) {
			// Assuming deleted readings should always be included if present, regardless of noprint flag?
			json::object reading_json;
			buildJsonReading(reading, reading_json);
			deleted_readings_json.emplace_back(std::move(reading_json));
		}
		if (!deleted_readings_json.empty()) {
			cohort_json["drs"] = std::move(deleted_readings_json);
		}
	}

	// Serialize and print the complete cohort JSON object
	std::string json_line = json::serialize(cohort_json);
	u_fprintf(output, "%s\n", json_line.c_str());
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
