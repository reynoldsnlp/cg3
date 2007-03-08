/*
* Copyright (C) 2006, GrammarSoft Aps
* and the VISL project at the University of Southern Denmark.
* All Rights Reserved.
*
* The contents of this file are subject to the GrammarSoft Public
* License Version 1.0 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.grammarsoft.com/GSPL or
* http://visl.sdu.dk/GSPL.txt
* 
* Software distributed under the License is distributed on an "AS
* IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
* implied. See the License for the specific language governing
* rights and limitations under the License.
*/

#include "GrammarApplicator.h"
#include "Window.h"
#include "SingleWindow.h"
#include "uextras.h"

using namespace CG3;
using namespace CG3::Strings;

uint32_t GrammarApplicator::runRulesOnWindow(Window *window, const std::vector<Rule*> *rules, const uint32_t start, const uint32_t end) {
	SingleWindow *current = window->current;

	uint32_t retval = RV_NOTHING;
	bool section_did_good = false;
	bool delimited = false;

	if (!rules->empty()) {
		for (uint32_t j=start;j<end;j++) {
			PACC_TimeStamp tstamp = 0;
			const Rule *rule = rules->at(j);
			KEYWORDS type = rule->type;

			if (!apply_mappings && (rule->type == K_MAP || rule->type == K_ADD || rule->type == K_REPLACE)) {
				continue;
			}
			if (!apply_corrections && (rule->type == K_SUBSTITUTE || rule->type == K_APPEND)) {
				continue;
			}
			if (statistics) {
				tstamp = timer->getCount();
			}

			for (uint32_t c=0 ; c < current->cohorts.size() ; c++) {
				if (c == 0) {
					continue;
				}
				Cohort *cohort = current->cohorts[c];
				if (cohort->readings.empty()) {
					continue;
				}

				const Set *set = grammar->sets_by_contents.find(rule->target)->second;
				if ((type == K_SELECT || type == K_REMOVE || type == K_IFF) && cohort->readings.size() == 1) {
					if (!set->has_mappings || cohort->readings.front()->tags_mapped.size() <= 1) {
						continue;
					}
				}
				if (type == K_DELIMIT && c == current->cohorts.size()-1) {
					continue;
				}
				if (rule->wordform && rule->wordform != cohort->wordform) {
					rule->num_fail++;
					continue;
				}

				size_t num_active = 0;
				size_t num_iff = 0;
				bool all_active = false;
				std::list<Reading*>::iterator rter;

				if (rule->type == K_IFF) {
					type = K_REMOVE;
				}

				bool did_test = false;
				bool did_append = false;
				bool test_good = false;
				for (rter = cohort->readings.begin() ; rter != cohort->readings.end() ; rter++) {
					Reading *reading = *rter;
					reading->matched_target = false;
					reading->matched_tests = false;
					reading->current_mapping_tag = 0;
					if (!reading->hash) {
						reading->rehash();
					}
					if (reading->mapped && (rule->type == K_MAP || rule->type == K_ADD || rule->type == K_REPLACE)) {
						continue;
					}
					last_mapping_tag = 0;
					if (rule->target && doesSetMatchReading(reading, rule->target, set->has_mappings)) {
						reading->current_mapping_tag = last_mapping_tag;
						reading->matched_target = true;
						bool good = true;
						if (!rule->tests.empty() && !did_test) {
							std::list<ContextualTest*>::iterator iter;
							for (iter = rule->tests.begin() ; iter != rule->tests.end() ; iter++) {
								ContextualTest *test = *iter;
								test_good = runContextualTest(window, current, c, test);
								if (!test_good) {
									good = test_good;
									if (!statistics) {
										break;
									}
								}
							}
						}
						else if (did_test) {
							good = test_good;
						}
						if (good) {
							if (rule->type == K_IFF) {
								type = K_SELECT;
							}
							reading->matched_tests = true;
							num_active++;
							rule->num_match++;
						}
						num_iff++;
					}
					else {
						rule->num_fail++;
					}
				}

				if (num_active == 0 && (num_iff == 0 || rule->type != K_IFF)) {
					continue;
				}
				if (num_active == cohort->readings.size()) {
					all_active = true;
				}
				if (all_active && rule->type == K_SELECT) {
					continue;
				}
				if (all_active && rule->type == K_REMOVE) {
					continue;
				}

				std::list<Reading*> removed;
				std::list<Reading*> selected;
				removed.clear();
				selected.clear();
				
				for (rter = cohort->readings.begin() ; rter != cohort->readings.end() ; rter++) {
					Reading *reading = *rter;
					bool good = reading->matched_tests;

					if (rule->type == K_IFF && type == K_REMOVE && reading->matched_target) {
						rule->num_match++;
						good = true;
					}

					if (type == K_REMOVE) {
						if (good && reading->current_mapping_tag && reading->tags_mapped.size() > 1) {
							reading->tags_list.remove(reading->current_mapping_tag);
							reflowReading(reading);
						}
						else {
							if (good) {
								removed.push_back(reading);
								reading->deleted = true;
								reading->hit_by.push_back(rule->line);
							}
						}
						if (good) {
							section_did_good = true;
						}
					}
					else if (type == K_SELECT) {
						if (good && reading->current_mapping_tag && reading->tags_mapped.size() > 1) {
							std::map<uint32_t, uint32_t>::iterator iter_maps;
							for (iter_maps = reading->tags_mapped.begin() ; iter_maps != reading->tags_mapped.end() ; iter_maps++) {
								reading->tags_list.remove(iter_maps->second);
							}
							reading->tags_list.push_back(reading->current_mapping_tag);
							reflowReading(reading);
						}
						if (good) {
							selected.push_back(reading);
							reading->hit_by.push_back(rule->line);
						}
						else {
							removed.push_back(reading);
							reading->deleted = true;
							reading->hit_by.push_back(rule->line);
						}
						if (good) {
							section_did_good = true;
						}
					}
					if (good) {
						if (type == K_REMVARIABLE) {
							u_fprintf(ux_stderr, "Info: RemVariable fired for %u.\n", rule->varname);
							variables.erase(rule->varname);
						}
						else if (type == K_SETVARIABLE) {
							u_fprintf(ux_stderr, "Info: SetVariable fired for %u.\n", rule->varname);
							variables[rule->varname] = 1;
						}
						else if (type == K_DELIMIT) {
							SingleWindow *nwin = new SingleWindow();
							uint32_t nc = c+1;

							Reading *cReading = new Reading();
							cReading->baseform = begintag;
							cReading->wordform = begintag;
							cReading->tags[begintag] = begintag;
							cReading->tags_list.push_back(begintag);
							cReading->rehash();

							Cohort *cCohort = new Cohort();
							cCohort->wordform = begintag;
							cCohort->readings.push_back(cReading);

							nwin->cohorts.push_back(cCohort);

							for ( ; nc < current->cohorts.size() ; nc++) {
								nwin->cohorts.push_back(current->cohorts.at(nc));
							}
							c = (uint32_t)current->cohorts.size()-c;
							for (nc = 0 ; nc < c-1 ; nc++) {
								current->cohorts.pop_back();
							}
							window->next.push_front(nwin);

							cohort = current->cohorts.back();
							for (rter = cohort->readings.begin() ; rter != cohort->readings.end() ; rter++) {
								Reading *reading = *rter;
								reading->tags_list.push_back(endtag);
								reading->tags[endtag] = endtag;
								reading->rehash();
							}
							delimited = true;
							break;
						}
						else if (rule->type == K_ADD || rule->type == K_MAP) {
							reading->hit_by.push_back(rule->line);
							reading->noprint = false;
							std::list<uint32_t>::const_iterator tter;
							for (tter = rule->maplist.begin() ; tter != rule->maplist.end() ; tter++) {
								reading->tags_list.push_back(*tter);
							}
							reflowReading(reading);
							if (rule->type == K_MAP) {
								reading->mapped = true;
							}
						}
						else if (rule->type == K_REPLACE) {
							reading->hit_by.push_back(rule->line);
							reading->noprint = false;
							std::list<uint32_t>::const_iterator tter;
							reading->tags_list.clear();
							reading->tags_list.push_back(reading->wordform);
							reading->tags_list.push_back(reading->baseform);
							for (tter = rule->maplist.begin() ; tter != rule->maplist.end() ; tter++) {
								reading->tags_list.push_back(*tter);
							}
							reflowReading(reading);
							if (!reading->tags_mapped.empty()) {
								reading->mapped = true;
							}
						}
						else if (rule->type == K_SUBSTITUTE) {
							std::list<uint32_t>::const_iterator tter;
							size_t tagb = reading->tags_list.size();
							for (tter = rule->sublist.begin() ; tter != rule->sublist.end() ; tter++) {
								reading->tags_list.remove(*tter);
							}
							if (tagb != reading->tags_list.size()) {
								reading->hit_by.push_back(rule->line);
								reading->noprint = false;
								for (tter = rule->maplist.begin() ; tter != rule->maplist.end() ; tter++) {
									reading->tags_list.push_back(*tter);
								}
								reflowReading(reading);
								if (!reading->tags_mapped.empty()) {
									reading->mapped = true;
								}
							}
						}
						else if (rule->type == K_APPEND && !did_append) {
							Reading *nr = cohort->allocateAppendReading();
							nr->hit_by.push_back(rule->line);
							nr->noprint = false;
							nr->tags_list.push_back(cohort->wordform);
							std::list<uint32_t>::const_iterator tter;
							for (tter = rule->maplist.begin() ; tter != rule->maplist.end() ; tter++) {
								nr->tags_list.push_back(*tter);
							}
							reflowReading(nr);
							if (!nr->tags_mapped.empty()) {
								nr->mapped = true;
							}
							did_append = true;
						}
					}
				}
				
				if (!removed.empty()) {
					cohort->deleted.insert(cohort->deleted.end(), removed.begin(), removed.end());
					while (!removed.empty()) {
						cohort->readings.remove(removed.back());
						removed.pop_back();
					}
					removed.clear();
				}
				if (!selected.empty()) {
					cohort->readings.clear();
					cohort->readings.insert(cohort->readings.begin(), selected.begin(), selected.end());
					selected.clear();
				}
				
				if (delimited) {
					break;
				}
			}

			if (statistics) {
				rule->total_time += (timer->getCount() - tstamp);
			}
			if (delimited) {
				break;
			}
		}
	}
	if (section_did_good) {
		retval |= RV_SOMETHING;
	}
	if (delimited) {
		retval |= RV_DELIMITED;
	}
	return retval;
}

int GrammarApplicator::runGrammarOnWindow(Window *window) {
label_runGrammarOnWindow_begin:
	SingleWindow *current = window->current;

	if (!grammar->before_sections.empty()) {
		reflowSingleWindow(current);
		uint32_t rv = runRulesOnWindow(window, &grammar->before_sections, 0, (uint32_t)grammar->before_sections.size());
		if (rv & RV_DELIMITED) {
			goto label_runGrammarOnWindow_begin;
		}
	}

	// ToDo: Make old cohort -> rules order available via switch
	if (!grammar->rules.empty()) {
		reflowSingleWindow(current);
		for (uint32_t i=0;i<grammar->sections.size()-1;) {
			uint32_t rv = runRulesOnWindow(window, &grammar->rules, 0, grammar->sections[i+1]);
			if (rv & RV_DELIMITED) {
				goto label_runGrammarOnWindow_begin;
			}
			if (!(rv & RV_SOMETHING)) {
				i++;
			}
		}
	}

	if (!grammar->after_sections.empty()) {
		reflowSingleWindow(current);
		uint32_t rv = runRulesOnWindow(window, &grammar->after_sections, 0, (uint32_t)grammar->after_sections.size());
		if (rv & RV_DELIMITED) {
			goto label_runGrammarOnWindow_begin;
		}
	}

	return 0;
}

bool GrammarApplicator::runContextualTest(const Window *window, const SingleWindow *sWindow, const uint32_t position, const ContextualTest *test) {
	bool retval = true;
	PACC_TimeStamp tstamp = 0;
	int pos = position + test->offset;
	const Cohort *cohort = 0;

	if (statistics) {
		tstamp = timer->getCount();
	}

	// ToDo: (NOT *) and (*C) tests can be cached
	if (test->absolute) {
		if (test->offset < 0) {
			pos = ((int)sWindow->cohorts.size()-1) - test->offset;
		}
		else {
			pos = test->offset;
		}
	}
	if (pos >= 0 && (uint32_t)pos < sWindow->cohorts.size()) {
		cohort = sWindow->cohorts.at(pos);
	}
	else if (test->span_both && pos < 0) {
		sWindow = window->previousFrom(sWindow);
		if (sWindow) {
			pos = (int)sWindow->cohorts.size()+pos;
			cohort = sWindow->cohorts.at(pos);
		}
	}
	else if (test->span_both && (uint32_t)pos >= sWindow->cohorts.size()) {
		sWindow = window->nextFrom(sWindow);
	}

	if (!cohort) {
		retval = false;
	}
	else {
		bool foundfirst = false;
		if (test->offset < 0 && pos >= 0 && (test->scanall || test->scanfirst)) {
			for (int i=pos;i>=0;i--) {
				cohort = sWindow->cohorts.at(i);
				retval = doesSetMatchCohortNormal(cohort, test->target);
				foundfirst = retval;
				if (test->careful) {
					if (retval) {
						retval = doesSetMatchCohortCareful(cohort, test->target);
					}
					else {
						retval = false;
					}
				}
				if (test->negative) {
					retval = !retval;
				}
				if (retval && test->linked) {
					retval = runContextualTest(window, sWindow, i, test->linked);
				}
				if (foundfirst && test->scanfirst) {
					break;
				}
				if (test->barrier) {
					bool barrier = doesSetMatchCohortNormal(cohort, test->barrier);
					if (barrier) {
						break;
					}
				}
			}
		}
		else if (test->offset > 0 && (uint32_t)pos <= sWindow->cohorts.size() && (test->scanall || test->scanfirst)) {
			for (uint32_t i=pos;i<sWindow->cohorts.size();i++) {
				cohort = sWindow->cohorts.at(i);
				retval = doesSetMatchCohortNormal(cohort, test->target);
				foundfirst = retval;
				if (test->careful) {
					if (retval) {
						retval = doesSetMatchCohortCareful(cohort, test->target);
					}
					else {
						retval = false;
					}
				}
				if (test->negative) {
					retval = !retval;
				}
				if (retval && test->linked) {
					retval = runContextualTest(window, sWindow, i, test->linked);
				}
				if (foundfirst && test->scanfirst) {
					break;
				}
				if (test->barrier) {
					bool barrier = doesSetMatchCohortNormal(cohort, test->barrier);
					if (barrier) {
						break;
					}
				}
			}
		}
		else {
			if (test->dep_child || test->dep_sibling || test->dep_parent) {
				int32_t rv = doesSetMatchDependency(sWindow, cohort, test);
				if (rv != -1) {
					retval = true;
					pos = rv;
				}
				else {
					retval = false;
				}
			}
			else {
				if (test->careful) {
					retval = doesSetMatchCohortCareful(cohort, test->target);
				}
				else {
					retval = doesSetMatchCohortNormal(cohort, test->target);
				}
			}
			if (test->negative) {
				retval = !retval;
			}
			if (retval && test->linked) {
				retval = runContextualTest(window, sWindow, pos, test->linked);
			}
		}
	}
	if (!cohort && test->negative) {
		retval = !retval;
	}
	if (retval) {
		test->num_match++;
	} else {
		test->num_fail++;
	}

	if (statistics) {
		test->total_time += (timer->getCount() - tstamp);
	}
	return retval;
}