/*
* Copyright (C) 2007-2025, GrammarSoft ApS
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

#include "PlaintextApplicator.hpp"
#include "Strings.hpp"
#include "Tag.hpp"
#include "Grammar.hpp"
#include "Window.hpp"
#include "SingleWindow.hpp"
#include "Reading.hpp"

namespace CG3 {

PlaintextApplicator::PlaintextApplicator(std::ostream& ux_err)
  : GrammarApplicator(ux_err)
{
}

void PlaintextApplicator::runGrammarOnText(std::istream& input, std::ostream& output) {
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
			u_fprintf(ux_stderr, "Warning: No soft or hard delimiters defined in grammar. Hard limit of %u cohorts may break windows in unintended places.\n", hard_limit);
		}
		else {
			u_fprintf(ux_stderr, "Warning: No hard delimiters defined in grammar. Soft limit of %u cohorts may break windows in unintended places.\n", soft_limit);
		}
	}

	UString line(1024, 0);
	UString cleaned(line.size(), 0);
	bool ignoreinput = false;
	bool did_soft_lookback = false;

	index();

	uint32_t resetAfter = ((num_windows + 4) * 2 + 1);
	uint32_t lines = 0;

	SingleWindow* cSWindow = nullptr;
	Cohort* cCohort = nullptr;

	SingleWindow* lSWindow = nullptr;
	Cohort* lCohort = nullptr;

	gWindow->window_span = num_windows;

	ux_stripBOM(input);

	while (!input.eof()) {
		++lines;
		auto packoff = get_line_clean(line, cleaned, input);

		// Trim trailing whitespace
		while (cleaned[0] && ISSPACE(cleaned[packoff - 1])) {
			cleaned[packoff - 1] = 0;
			--packoff;
		}
		if (ignoreinput) {
			goto istext;
		}
		if (cleaned[0]) {
			UChar* space = &cleaned[0];
			SKIPTO_NOSPAN_RAW(space, ' ');

			if (space[0] != ' ') {
				goto istext;
			}
			space[0] = 0;

			UString tag;
			tag.append(u"\"<");
			tag += &cleaned[0];
			tag.append(u">\"");

			if (cCohort && cCohort->readings.empty()) {
				initEmptyCohort(*cCohort);
			}
			if (cSWindow && cSWindow->cohorts.size() >= soft_limit && grammar->soft_delimiters && !did_soft_lookback) {
				did_soft_lookback = true;
				for (auto c : reversed(cSWindow->cohorts)) {
					if (doesSetMatchCohortNormal(*c, grammar->soft_delimiters->number)) {
						did_soft_lookback = false;
						Cohort* cohort = delimitAt(*cSWindow, c);
						cSWindow = cohort->parent->next;
						if (cCohort) {
							cCohort->parent = cSWindow;
						}
						if (verbosity_level > 0) {
							u_fprintf(ux_stderr, "Warning: Soft limit of %u cohorts reached at line %u but found suitable soft delimiter in buffer.\n", soft_limit, numLines);
							u_fflush(ux_stderr);
						}
						break;
					}
				}
			}
			if (cCohort && cSWindow->cohorts.size() >= soft_limit && grammar->soft_delimiters && doesSetMatchCohortNormal(*cCohort, grammar->soft_delimiters->number)) {
				if (verbosity_level > 0) {
					u_fprintf(ux_stderr, "Warning: Soft limit of %u cohorts reached at line %u but found suitable soft delimiter.\n", soft_limit, numLines);
					u_fflush(ux_stderr);
				}
				for (auto iter : cCohort->readings) {
					addTagToReading(*iter, endtag);
				}

				cSWindow->appendCohort(cCohort);
				lSWindow = cSWindow;
				cSWindow = nullptr;
				cCohort = nullptr;
				numCohorts++;
				did_soft_lookback = false;
			}
			if (cCohort && (cSWindow->cohorts.size() >= hard_limit || (!dep_delimit && grammar->delimiters && doesSetMatchCohortNormal(*cCohort, grammar->delimiters->number)))) {
				if (!is_conv && cSWindow->cohorts.size() >= hard_limit) {
					u_fprintf(ux_stderr, "Warning: Hard limit of %u cohorts reached at cohort %S (#%u) on line %u - forcing break.\n", hard_limit, cCohort->wordform->tag.data(), numCohorts, numLines);
					u_fflush(ux_stderr);
				}
				for (auto iter : cCohort->readings) {
					addTagToReading(*iter, endtag);
				}

				cSWindow->appendCohort(cCohort);
				lSWindow = cSWindow;
				cSWindow = nullptr;
				cCohort = nullptr;
				numCohorts++;
				did_soft_lookback = false;
			}
			if (!cSWindow) {
				cSWindow = gWindow->allocAppendSingleWindow();
				initEmptySingleWindow(cSWindow);

				lSWindow = cSWindow;
				cCohort = nullptr;
				numWindows++;
				did_soft_lookback = false;
			}
			if (cCohort && cSWindow) {
				cSWindow->appendCohort(cCohort);
			}
			if (gWindow->next.size() > num_windows) {
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
			cCohort = alloc_cohort(cSWindow);
			cCohort->global_number = gWindow->cohort_counter++;
			cCohort->wordform = addTag(tag);
			lCohort = cCohort;
			numCohorts++;
		}
		else {
		istext:
			if (cleaned[0] && line[0]) {
				// Plaintext format doesn't have stream commands.
				if (lCohort) {
					lCohort->text += &line[0];
				}
				else if (lSWindow) {
					lSWindow->text += &line[0];
				}
				else {
					printPlainTextLine(&line[0], output, false); // Original line included newline
				}
			}
		}
		numLines++;
		line[0] = cleaned[0] = 0;
	}

	input_eof = true;

	if (cCohort && cSWindow) {
		cSWindow->appendCohort(cCohort);
		if (cCohort->readings.empty()) {
			initEmptyCohort(*cCohort);
		}
		for (auto iter : cCohort->readings) {
			addTagToReading(*iter, endtag);
		}
		cCohort = nullptr;
		cSWindow = nullptr;
	}
	while (!gWindow->next.empty()) {
		gWindow->shuffleWindowsDown();
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
}

// Update signature here
void PlaintextApplicator::printReading(const Reading* reading, std::ostream& output, size_t sub) {
	if (reading->noprint) {
		return;
	}
	if (reading->deleted) {
		return;
	}

	if (reading->baseform) {
		auto& tag = grammar->single_tags.find(reading->baseform)->second->tag;
		u_fprintf(output, "%.*S", tag.size() - 2, tag.data() + 1);
	}
	u_fputc('\n', output);

	if (reading->next) {
		printReading(reading->next, output, sub + 1); // Pass sub + 1 for potential indentation (though not used here)
	}
}

// Remove override
void PlaintextApplicator::printCohort(Cohort* cohort, std::ostream& output, bool profiling) {
	if (cohort->local_number == 0) {
		goto removed;
	}
	if (cohort->type & CT_REMOVED) {
		goto removed;
	}

	if (!cohort->wblank.empty()) {
		printPlainTextLine(cohort->wblank, output, !ISNL(cohort->wblank.back()));
	}

	if (!profiling) {
		cohort->unignoreAll();
	}

	std::sort(cohort->readings.begin(), cohort->readings.end(), Reading::cmp_number);
	for (auto rter1 : cohort->readings) {
		printReading(rter1, output);
		break; // Only print the first reading in plaintext mode
	}

removed:
	if (!cohort->text.empty() && cohort->text.find_first_not_of(ws) != UString::npos) {
		printPlainTextLine(cohort->text, output, !ISNL(cohort->text.back()));
	}
}

// Remove override
void PlaintextApplicator::printSingleWindow(SingleWindow* window, std::ostream& output, bool profiling) {
	if (!window->text.empty()) {
		printPlainTextLine(window->text, output, !ISNL(window->text.back()));
	}

	for (auto& cohort : window->all_cohorts) {
		printCohort(cohort, output, profiling);
	}

	if (!window->text_post.empty()) {
		printPlainTextLine(window->text_post, output, !ISNL(window->text_post.back()));
	}

	u_fflush(output);
}

} // namespace CG3
