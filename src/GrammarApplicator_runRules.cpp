/*
* Copyright (C) 2007-2010, GrammarSoft ApS
* Developed by Tino Didriksen <tino@didriksen.cc>
* Design by Eckhard Bick <eckhard.bick@mail.dk>, Tino Didriksen <tino@didriksen.cc>
*
* This file is part of VISL CG-3
*
* VISL CG-3 is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* VISL CG-3 is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with VISL CG-3.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "GrammarApplicator.h"
#include "Strings.h"
#include "Tag.h"
#include "Grammar.h"
#include "Window.h"
#include "SingleWindow.h"
#include "Reading.h"
#include "ContextualTest.h"

namespace CG3 {

bool GrammarApplicator::updateRuleToCohorts(Cohort& c, const uint32_t& rsit) {
	// Check whether this rule is in the allowed rule list from cmdline flag --rule(s)
	if (!valid_rules.empty() && valid_rules.find(rsit) == valid_rules.end()) {
		return false;
	}
	SingleWindow *current = c.parent;
	const Rule *r = grammar->rule_by_line.find(rsit)->second;
	if (r->wordform && r->wordform != c.wordform) {
		return false;
	}
	CohortSet& s = current->rule_to_cohorts[rsit];
	s.insert(&c);
	return current->valid_rules.insert(rsit).second;
}

void intersectInitialize(const uint32SortedVector& first, const uint32Set& second, uint32Vector& intersects) {
	//intersects.reserve(std::max(first.size(), second.size()));
	uint32SortedVector::const_iterator iiter = first.begin();
	uint32Set::const_iterator oiter = second.begin();
	while (oiter != second.end() && iiter != first.end()) {
		while (oiter != second.end() && iiter != first.end() && *oiter < *iiter) {
			++oiter;
		}
		while (oiter != second.end() && iiter != first.end() && *iiter < *oiter) {
			++iiter;
		}
		while (oiter != second.end() && iiter != first.end() && *oiter == *iiter) {
			intersects.push_back(*oiter);
			++oiter;
			++iiter;
		}
	}
}

template<typename TFirst, typename TSecond>
void intersectUpdate(const TFirst& first, const TSecond& second, uint32Vector& intersects) {
	/* This is never true, so don't bother...
	if (intersects.empty()) {
		intersectInitialize(first, second, intersects);
		return;
	}
	//*/
	//intersects.reserve(std::max(first.size(), second.size()));
	typename TFirst::const_iterator iiter = first.begin();
	typename TSecond::const_iterator oiter = second.begin();
	uint32Vector::iterator ins = intersects.begin();
	while (oiter != second.end() && iiter != first.end()) {
		while (oiter != second.end() && iiter != first.end() && *oiter < *iiter) {
			++oiter;
		}
		while (oiter != second.end() && iiter != first.end() && *iiter < *oiter) {
			++iiter;
		}
		while (oiter != second.end() && iiter != first.end() && *oiter == *iiter) {
			ins = std::lower_bound(ins, intersects.end(), *oiter);
			if (ins == intersects.end() || *ins != *oiter) {
				ins = intersects.insert(ins, *oiter);
			}
			++oiter;
			++iiter;
		}
	}
}

void GrammarApplicator::updateValidRules(const uint32SortedVector& rules, uint32Vector& intersects, const uint32_t& hash, Reading& reading) {
	uint32HashSetuint32HashMap::const_iterator it = grammar->rules_by_tag.find(hash);
	if (it != grammar->rules_by_tag.end()) {
		Cohort& c = *(reading.parent);
		uint32SortedVector inserted;
		const_foreach (uint32HashSet, (it->second), rsit, rsit_end) {
			if (updateRuleToCohorts(c, *rsit)) {
				inserted.push_back(*rsit);
			}
		}
		if (!inserted.empty()) {
			intersectUpdate(rules, inserted, intersects);
		}
	}
}

void GrammarApplicator::indexSingleWindow(SingleWindow& current) {
	current.valid_rules.clear();

	foreach (CohortVector, current.cohorts, iter, iter_end) {
		Cohort *c = *iter;
		foreach (uint32HashSet, c->possible_sets, psit, psit_end) {
			if (grammar->rules_by_set.find(*psit) == grammar->rules_by_set.end()) {
				continue;
			}
			const uint32Set& rules = grammar->rules_by_set.find(*psit)->second;
			const_foreach (uint32Set, rules, rsit, rsir_end) {
				updateRuleToCohorts(*c, *rsit);
			}
		}
	}
}

uint32_t GrammarApplicator::runRulesOnSingleWindow(SingleWindow& current, uint32SortedVector& rules) {
	uint32_t retval = RV_NOTHING;
	bool section_did_something = false;
	bool delimited = false;

	typedef stdext::hash_map<uint32_t,Reading*> readings_plain_t;
	readings_plain_t readings_plain;

	uint32Vector intersects;
	intersectInitialize(rules, current.valid_rules, intersects);

	foreach (uint32Vector, intersects, iter_rules, iter_rules_end) {
		uint32_t j = (*iter_rules);

		// Check whether this rule is in the allowed rule list from cmdline flag --rule(s)
		if (!valid_rules.empty() && valid_rules.find(j) == valid_rules.end()) {
			continue;
		}

		const Rule& rule = *(grammar->rule_by_line.find(j)->second);

		ticks tstamp(gtimer);
		KEYWORDS type = rule.type;

		if (!apply_mappings && (rule.type == K_MAP || rule.type == K_ADD || rule.type == K_REPLACE)) {
			continue;
		}
		if (!apply_corrections && (rule.type == K_SUBSTITUTE || rule.type == K_APPEND)) {
			continue;
		}
		if (has_enclosures) {
			if (rule.flags & RF_ENCL_FINAL && !did_final_enclosure) {
				continue;
			}
			else if (did_final_enclosure && !(rule.flags & RF_ENCL_FINAL)) {
				continue;
			}
		}
		if (statistics) {
			tstamp = getticks();
		}

		const Set& set = *(grammar->sets_by_contents.find(rule.target)->second);

		// ToDo: Make better use of rules_by_tag

		if (debug_level > 1) {
			std::cout << "DEBUG: " << current.rule_to_cohorts.find(rule.line)->second.size() << "/" << current.cohorts.size() << " = " << double(current.rule_to_cohorts.find(rule.line)->second.size())/double(current.cohorts.size()) << std::endl;
		}
		for (CohortSet::iterator rocit = current.rule_to_cohorts.find(rule.line)->second.begin() ; rocit != current.rule_to_cohorts.find(rule.line)->second.end() ; ) {
			Cohort *cohort = *rocit;
			++rocit;
			if (cohort->local_number == 0) {
				continue;
			}
			if (cohort->type & CT_REMOVED) {
				continue;
			}

			uint32_t c = cohort->local_number;
			if ((cohort->type & CT_ENCLOSED) || cohort->parent != &current) {
				continue;
			}
			if (cohort->readings.empty()) {
				continue;
			}
			if (cohort->possible_sets.find(rule.target) == cohort->possible_sets.end()) {
				continue;
			}

			if (cohort->readings.size() == 1) {
				if (type == K_SELECT) {
					continue;
				}
				else if ((type == K_REMOVE || type == K_IFF) && (!unsafe || (rule.flags & RF_SAFE)) && !(rule.flags & RF_UNSAFE)) {
					continue;
				}
			}
			if (type == K_DELIMIT && c == current.cohorts.size()-1) {
				continue;
			}
			if (rule.wordform && rule.wordform != cohort->wordform) {
				rule.num_fail++;
				continue;
			}

			if (rule.flags & RF_ENCL_INNER) {
				if (!par_left_pos) {
					continue;
				}
				if (cohort->local_number < par_left_pos || cohort->local_number > par_right_pos) {
					continue;
				}
			}
			else if (rule.flags & RF_ENCL_OUTER) {
				if (par_left_pos && cohort->local_number >= par_left_pos && cohort->local_number <= par_right_pos) {
					continue;
				}
			}

			uint32_t ih = hash_sdbm_uint32_t(rule.line, cohort->global_number);
			if (index_matches(index_ruleCohort_no, ih)) {
				continue;
			}
			index_ruleCohort_no.insert(ih);

			size_t num_active = 0;
			size_t num_iff = 0;

			if (rule.type == K_IFF) {
				type = K_REMOVE;
			}

			bool did_test = false;
			bool test_good = false;
			bool matched_target = false;

			readings_plain.clear();
			if (!regexgrps.empty()) {
				regexgrps.clear();
			}

			foreach (ReadingList, cohort->readings, rter1, rter1_end) {
				Reading *reading = *rter1;
				reading->matched_target = false;
				reading->matched_tests = false;

				if (reading->mapped && (rule.type == K_MAP || rule.type == K_ADD || rule.type == K_REPLACE)) {
					continue;
				}
				if (reading->noprint && !allow_magic_readings) {
					continue;
				}
				if (!(set.type & (ST_MAPPING|ST_CHILD_UNIFY)) && !readings_plain.empty()) {
					readings_plain_t::const_iterator rpit = readings_plain.find(reading->hash_plain);
					if (rpit != readings_plain.end()) {
						reading->matched_target = rpit->second->matched_target;
						reading->matched_tests = rpit->second->matched_tests;
						if (reading->matched_tests) {
							++num_active;
						}
						continue;
					}
				}

				unif_last_wordform = 0;
				unif_last_baseform = 0;
				unif_last_textual = 0;
				if (!unif_tags.empty()) {
					unif_tags.clear();
				}
				unif_sets_firstrun = true;
				if (!unif_sets.empty()) {
					unif_sets.clear();
				}

				target = 0;
				mark = cohort;
				if (rule.target && doesSetMatchReading(*reading, rule.target, (set.type & (ST_CHILD_UNIFY|ST_SPECIAL)) != 0)) {
					target = cohort;
					reading->matched_target = true;
					matched_target = true;
					bool good = true;
					if (!did_test) {
						ContextualTest *test = rule.test_head;
						while (test) {
							if (rule.flags & RF_RESETX || !(rule.flags & RF_REMEMBERX)) {
								mark = cohort;
							}
							dep_deep_seen.clear();
							std::fill(ci_depths.begin(), ci_depths.end(), 0);
							if (!(test->pos & POS_PASS_ORIGIN) && (no_pass_origin || (test->pos & POS_NO_PASS_ORIGIN))) {
								test_good = (runContextualTest(&current, c, test, 0, cohort) != 0);
							}
							else {
								test_good = (runContextualTest(&current, c, test) != 0);
							}
							if (!test_good) {
								good = test_good;
								if (!statistics) {
									if (test != rule.test_head && !(rule.flags & (RF_REMEMBERX|RF_KEEPORDER))) {
										test->detach();
										if (rule.test_head) {
											rule.test_head->prev = test;
											test->next = rule.test_head;
										}
										rule.test_head = test;
									}
									break;
								}
							}
							test = test->next;
						}
					}
					else if (did_test) {
						good = test_good;
					}
					if (good) {
						if (rule.type == K_IFF) {
							type = K_SELECT;
						}
						reading->matched_tests = true;
						num_active++;
						rule.num_match++;
					}
					num_iff++;
				}
				else {
					rule.num_fail++;
				}
				readings_plain[reading->hash_plain] = reading;
			}

			if (num_active == 0 && (num_iff == 0 || rule.type != K_IFF)) {
				if (!matched_target) {
					--rocit;
					current.rule_to_cohorts.find(rule.line)->second.erase(rocit++);
				}
				continue;
			}

			if (num_active == cohort->readings.size()) {
				if (type == K_SELECT) {
					continue;
				}
				else if (type == K_REMOVE && (!unsafe || (rule.flags & RF_SAFE)) && !(rule.flags & RF_UNSAFE)) {
					continue;
				}
			}

			uint32_t did_append = 0;
			ReadingList removed;
			ReadingList selected;

			const size_t state_num_readings = cohort->readings.size();
			const size_t state_num_removed = cohort->deleted.size();
			const size_t state_num_delayed = cohort->delayed.size();
			bool readings_changed = false;

			foreach (ReadingList, cohort->readings, rter2, rter2_end) {
				Reading& reading = **rter2;
				bool good = reading.matched_tests;
				const uint32_t state_hash = reading.hash;

				if (rule.type == K_IFF && type == K_REMOVE && reading.matched_target) {
					rule.num_match++;
					good = true;
				}

				if (type == K_REMOVE) {
					if (good) {
						removed.push_back(&reading);
						index_ruleCohort_no.clear();
						reading.hit_by.push_back(rule.line);
						if (debug_level > 0) {
							std::cerr << "DEBUG: Rule " << rule.line << " hit cohort " << cohort->local_number << std::endl;
						}
					}
				}
				else if (type == K_SELECT) {
					if (good) {
						selected.push_back(&reading);
						index_ruleCohort_no.clear();
						reading.hit_by.push_back(rule.line);
					}
					else {
						removed.push_back(&reading);
						index_ruleCohort_no.clear();
						reading.hit_by.push_back(rule.line);
					}
					if (good) {
						if (debug_level > 0) {
							std::cerr << "DEBUG: Rule " << rule.line << " hit cohort " << cohort->local_number << std::endl;
						}
					}
				}
				else if (good) {
					if (type == K_REMVARIABLE) {
						u_fprintf(ux_stderr, "Info: RemVariable fired for %u.\n", rule.varname);
						variables.erase(rule.varname);
					}
					else if (type == K_SETVARIABLE) {
						u_fprintf(ux_stderr, "Info: SetVariable fired for %u.\n", rule.varname);
						variables[rule.varname] = 1;
					}
					else if (type == K_DELIMIT) {
						delimitAt(current, cohort);
						delimited = true;
						readings_changed = true;
						break;
					}
					else if (type == K_REMCOHORT) {
						foreach (ReadingList, cohort->readings, iter, iter_end) {
							(*iter)->hit_by.push_back(rule.line);
							(*iter)->deleted = true;
						}
						cohort->type |= CT_REMOVED;
						cohort->prev->removed.push_back(cohort);
						current.cohorts.erase(current.cohorts.begin()+cohort->local_number);
						foreach (CohortVector, current.cohorts, iter, iter_end) {
							(*iter)->local_number = std::distance(current.cohorts.begin(), iter);
						}
						gWindow->rebuildCohortLinks();
						readings_changed = true;
						break;
					}
					else if (rule.type == K_ADD || rule.type == K_MAP) {
						index_ruleCohort_no.clear();
						reading.hit_by.push_back(rule.line);
						reading.noprint = false;
						TagList mappings;
						const_foreach (TagList, rule.maplist, tter, tter_end) {
							uint32_t hash = (*tter)->hash;
							if ((*tter)->type & T_MAPPING || (*tter)->tag[0] == grammar->mapping_prefix) {
								mappings.push_back(*tter);
							}
							else {
								hash = addTagToReading(reading, hash);
							}
							updateValidRules(rules, intersects, hash, reading);
							iter_rules = std::lower_bound(intersects.begin(), intersects.end(), rule.line);
							iter_rules_end = intersects.end();
						}
						if (!mappings.empty()) {
							splitMappings(mappings, *cohort, reading, rule.type == K_MAP);
						}
						if (rule.type == K_MAP) {
							reading.mapped = true;
						}
						if (reading.hash != state_hash) {
							readings_changed = true;
						}
					}
					else if (rule.type == K_REPLACE) {
						index_ruleCohort_no.clear();
						reading.hit_by.push_back(rule.line);
						reading.noprint = false;
						reading.tags_list.clear();
						reading.tags_list.push_back(reading.wordform);
						reading.tags_list.push_back(reading.baseform);
						reflowReading(reading);
						TagList mappings;
						const_foreach (TagList, rule.maplist, tter, tter_end) {
							uint32_t hash = (*tter)->hash;
							if ((*tter)->type & T_MAPPING || (*tter)->tag[0] == grammar->mapping_prefix) {
								mappings.push_back(*tter);
							}
							else {
								hash = addTagToReading(reading, hash);
							}
							updateValidRules(rules, intersects, hash, reading);
							iter_rules = std::lower_bound(intersects.begin(), intersects.end(), rule.line);
							iter_rules_end = intersects.end();
						}
						if (!mappings.empty()) {
							splitMappings(mappings, *cohort, reading, true);
						}
						if (reading.hash != state_hash) {
							readings_changed = true;
						}
					}
					else if (rule.type == K_SUBSTITUTE) {
						// ToDo: Check whether this substitution will do nothing at all to the end result
						// ToDo: Not actually...instead, test whether any reading in the cohort already is the end result

						uint32_t tloc = 0;
						size_t tagb = reading.tags_list.size();
						const_foreach (uint32List, rule.sublist, tter, tter_end) {
							if (!tloc) {
								foreach (uint32List, reading.tags_list, tfind, tfind_end) {
									if (*tfind == *tter) {
										tloc = *(--tfind);
										break;
									}
								}
							}
							reading.tags_list.remove(*tter);
							reading.tags.erase(*tter);
							if (reading.baseform == *tter) {
								reading.baseform = 0;
							}
						}
						if (tagb != reading.tags_list.size()) {
							index_ruleCohort_no.clear();
							reading.hit_by.push_back(rule.line);
							reading.noprint = false;
							uint32List::iterator tpos = reading.tags_list.end();
							foreach (uint32List, reading.tags_list, tfind, tfind_end) {
								if (*tfind == tloc) {
									tpos = ++tfind;
									break;
								}
							}
							TagList mappings;
							const_foreach (TagList, rule.maplist, tter, tter_end) {
								if ((*tter)->hash == grammar->tag_any) {
									break;
								}
								if (reading.tags.find((*tter)->hash) != reading.tags.end()) {
									continue;
								}
								if ((*tter)->type & T_MAPPING || (*tter)->tag[0] == grammar->mapping_prefix) {
									mappings.push_back(*tter);
								}
								else {
									reading.tags_list.insert(tpos, (*tter)->hash);
								}
								updateValidRules(rules, intersects, (*tter)->hash, reading);
								iter_rules = std::lower_bound(intersects.begin(), intersects.end(), rule.line);
								iter_rules_end = intersects.end();
							}
							reflowReading(reading);
							if (!mappings.empty()) {
								splitMappings(mappings, *cohort, reading, true);
							}
						}
						if (reading.hash != state_hash) {
							readings_changed = true;
						}
					}
					else if (rule.type == K_APPEND && rule.line != did_append) {
						Reading *cReading = cohort->allocateAppendReading();
						numReadings++;
						index_ruleCohort_no.clear();
						cReading->hit_by.push_back(rule.line);
						cReading->noprint = false;
						addTagToReading(*cReading, cohort->wordform);
						TagList mappings;
						const_foreach (TagList, rule.maplist, tter, tter_end) {
							uint32_t hash = (*tter)->hash;
							if ((*tter)->type & T_MAPPING || (*tter)->tag[0] == grammar->mapping_prefix) {
								mappings.push_back(*tter);
							}
							else {
								hash = addTagToReading(*cReading, hash);
							}
							updateValidRules(rules, intersects, hash, reading);
							iter_rules = std::lower_bound(intersects.begin(), intersects.end(), rule.line);
							iter_rules_end = intersects.end();
						}
						if (!mappings.empty()) {
							splitMappings(mappings, *cohort, *cReading, true);
						}
						did_append = rule.line;
						readings_changed = true;
					}
					else if (type == K_SETPARENT || type == K_SETCHILD) {
						int32_t orgoffset = rule.dep_target->offset;
						uint32Set seen_targets;

						bool attached = false;
						Cohort *target = cohort;
						while (!attached) {
							Cohort *attach = 0;
							seen_targets.insert(target->global_number);
							dep_deep_seen.clear();
							attach_to = 0;
							if (runContextualTest(target->parent, target->local_number, rule.dep_target, &attach) && attach) {
								if (attach_to) {
									attach = attach_to;
								}
								bool good = true;
								ContextualTest *test = rule.dep_test_head;
								while (test) {
									mark = attach;
									dep_deep_seen.clear();
									test_good = (runContextualTest(attach->parent, attach->local_number, test) != 0);
									if (!test_good) {
										good = test_good;
										break;
									}
									test = test->next;
								}
								if (good) {
									if (type == K_SETPARENT) {
										attached = attachParentChild(*attach, *cohort, (rule.flags & RF_ALLOWLOOP) != 0, (rule.flags & RF_ALLOWCROSS) != 0);
									}
									else {
										attached = attachParentChild(*cohort, *attach, (rule.flags & RF_ALLOWLOOP) != 0, (rule.flags & RF_ALLOWCROSS) != 0);
									}
									if (attached) {
										index_ruleCohort_no.clear();
										reading.hit_by.push_back(rule.line);
										reading.noprint = false;
										has_dep = true;
										readings_changed = true;
										break;
									}
								}
								if (rule.flags & RF_NEAREST) {
									break;
								}
								if (seen_targets.find(attach->global_number) != seen_targets.end()) {
									// We've found a cohort we have seen before...
									// We assume running the test again would result in the same, so don't bother.
									break;
								}
								if (!attached) {
									// Did not successfully attach due to loop restrictions; look onwards from here
									target = attach;
									if (rule.dep_target->offset != 0) {
										// Temporarily set offset to +/- 1
										rule.dep_target->offset = ((rule.dep_target->offset < 0) ? -1 : 1);
									}
								}
							}
							else {
								break;
							}
						}
						rule.dep_target->offset = orgoffset;
						break;
					}
					else if (type == K_MOVE_AFTER || type == K_MOVE_BEFORE || type == K_SWITCH) {
						// ToDo: ** tests will not correctly work for MOVE/SWITCH; cannot move cohorts between windows
						Cohort *attach = 0;
						dep_deep_seen.clear();
						attach_to = 0;
						if (runContextualTest(&current, c, rule.dep_target, &attach) && attach && cohort->parent == attach->parent) {
							if (attach_to) {
								attach = attach_to;
							}
							bool good = true;
							ContextualTest *test = rule.dep_test_head;
							while (test) {
								mark = attach;
								dep_deep_seen.clear();
								test_good = (runContextualTest(attach->parent, attach->local_number, test) != 0);
								if (!test_good) {
									good = test_good;
									break;
								}
								test = test->next;
							}

							if (!good || cohort == attach || cohort->local_number == 0) {
								break;
							}

							if (type == K_SWITCH) {
								if (attach->local_number == 0) {
									break;
								}
								current.cohorts[cohort->local_number] = attach;
								current.cohorts[attach->local_number] = cohort;
								foreach (ReadingList, cohort->readings, iter, iter_end) {
									(*iter)->hit_by.push_back(rule.line);
								}
								foreach (ReadingList, attach->readings, iter, iter_end) {
									(*iter)->hit_by.push_back(rule.line);
								}
							}
							else {
								CohortVector cohorts;
								if (rule.childset1) {
									for (CohortVector::iterator iter = current.cohorts.begin() ; iter != current.cohorts.end() ; ) {
										if (isChildOf(*iter, cohort) && doesSetMatchCohortNormal(**iter, rule.childset1)) {
											cohorts.push_back(*iter);
											iter = current.cohorts.erase(iter);
										}
										else {
											++iter;
										}
									}
								}
								else {
									cohorts.push_back(cohort);
									current.cohorts.erase(current.cohorts.begin()+cohort->local_number);
								}

								foreach (CohortVector, current.cohorts, iter, iter_end) {
									(*iter)->local_number = std::distance(current.cohorts.begin(), iter);
								}

								CohortVector edges;
								if (rule.childset2) {
									foreach (CohortVector, current.cohorts, iter, iter_end) {
										if (isChildOf(*iter, attach) && doesSetMatchCohortNormal(**iter, rule.childset2)) {
											edges.push_back(*iter);
										}
									}
								}
								else {
									edges.push_back(attach);
								}
								uint32_t spot = 0;
								if (type == K_MOVE_BEFORE) {
									spot = edges.front()->local_number;
									if (spot == 0) {
										spot = 1;
									}
								}
								else if (type == K_MOVE_AFTER) {
									spot = edges.back()->local_number+1;
								}

								while (!cohorts.empty()) {
									foreach (ReadingList, cohorts.back()->readings, iter, iter_end) {
										(*iter)->hit_by.push_back(rule.line);
									}
									current.cohorts.insert(current.cohorts.begin()+spot, cohorts.back());
									cohorts.pop_back();
								}
							}
							foreach (CohortVector, current.cohorts, iter, iter_end) {
								(*iter)->local_number = std::distance(current.cohorts.begin(), iter);
							}
							gWindow->rebuildCohortLinks();
							readings_changed = true;
							break;
						}
					}
					else if (type == K_ADDRELATION || type == K_SETRELATION || type == K_REMRELATION) {
						Cohort *attach = 0;
						dep_deep_seen.clear();
						attach_to = 0;
						if (runContextualTest(&current, c, rule.dep_target, &attach) && attach) {
							if (attach_to) {
								attach = attach_to;
							}
							bool good = true;
							ContextualTest *test = rule.dep_test_head;
							while (test) {
								mark = attach;
								dep_deep_seen.clear();
								test_good = (runContextualTest(attach->parent, attach->local_number, test) != 0);
								if (!test_good) {
									good = test_good;
									break;
								}
								test = test->next;
							}
							if (good) {
								index_ruleCohort_no.clear();
								reading.hit_by.push_back(rule.line);
								reading.noprint = false;
								if (type == K_ADDRELATION) {
									attach->type |= CT_RELATED;
									cohort->type |= CT_RELATED;
									cohort->addRelation(rule.maplist.front()->hash, attach->global_number);
								}
								else if (type == K_SETRELATION) {
									attach->type |= CT_RELATED;
									cohort->type |= CT_RELATED;
									cohort->setRelation(rule.maplist.front()->hash, attach->global_number);
								}
								else {
									cohort->remRelation(rule.maplist.front()->hash, attach->global_number);
								}
								readings_changed = true;
							}
						}
						break;
					}
					else if (type == K_ADDRELATIONS || type == K_SETRELATIONS || type == K_REMRELATIONS) {
						Cohort *attach = 0;
						dep_deep_seen.clear();
						attach_to = 0;
						if (runContextualTest(&current, c, rule.dep_target, &attach) && attach) {
							if (attach_to) {
								attach = attach_to;
							}
							bool good = true;
							ContextualTest *test = rule.dep_test_head;
							while (test) {
								mark = attach;
								dep_deep_seen.clear();
								test_good = (runContextualTest(attach->parent, attach->local_number, test) != 0);
								if (!test_good) {
									good = test_good;
									break;
								}
								test = test->next;
							}
							if (good) {
								index_ruleCohort_no.clear();
								reading.hit_by.push_back(rule.line);
								reading.noprint = false;
								if (type == K_ADDRELATIONS) {
									attach->type |= CT_RELATED;
									cohort->type |= CT_RELATED;
									cohort->addRelation(rule.maplist.front()->hash, attach->global_number);
									attach->addRelation(rule.sublist.front(), cohort->global_number);
								}
								else if (type == K_SETRELATIONS) {
									attach->type |= CT_RELATED;
									cohort->type |= CT_RELATED;
									cohort->setRelation(rule.maplist.front()->hash, attach->global_number);
									attach->setRelation(rule.sublist.front(), cohort->global_number);
								}
								else {
									cohort->remRelation(rule.maplist.front()->hash, attach->global_number);
									attach->remRelation(rule.sublist.front(), cohort->global_number);
								}
								readings_changed = true;
							}
						}
					}
				}
			}

			if (type == K_REMOVE && removed.size() == cohort->readings.size() && (!unsafe || (rule.flags & RF_SAFE)) && !(rule.flags & RF_UNSAFE)) {
				removed.clear();
			}

			if (!removed.empty()) {
				if (rule.flags & RF_DELAYED) {
					cohort->delayed.insert(cohort->delayed.end(), removed.begin(), removed.end());
				}
				else {
					cohort->deleted.insert(cohort->deleted.end(), removed.begin(), removed.end());
				}
				while (!removed.empty()) {
					removed.back()->deleted = true;
					cohort->readings.remove(removed.back());
					removed.pop_back();
				}
				if (debug_level > 0) {
					std::cerr << "DEBUG: Rule " << rule.line << " hit cohort " << cohort->local_number << std::endl;
				}
			}
			if (!selected.empty()) {
				cohort->readings = selected;
			}

			// Cohort state has changed, so mark that the section did something
			if (state_num_readings != cohort->readings.size()
				|| state_num_removed != cohort->deleted.size()
				|| state_num_delayed != cohort->delayed.size()
				|| readings_changed) {
				if (!(rule.flags & RF_NOITERATE) && section_max_count != 1) {
					section_did_something = true;
				}
				cohort->type &= ~CT_NUM_CURRENT;
			}

			if (delimited) {
				break;
			}
		}

		if (statistics) {
			ticks tmp = getticks();
			rule.total_time += elapsed(tmp, tstamp);
		}

		if (delimited) {
			break;
		}
	}

	if (section_did_something) {
		retval |= RV_SOMETHING;
	}
	if (delimited) {
		retval |= RV_DELIMITED;
	}
	return retval;
}

int GrammarApplicator::runGrammarOnSingleWindow(SingleWindow& current) {
	if (!grammar->before_sections.empty() && !no_before_sections) {
		uint32_t rv = runRulesOnSingleWindow(current, runsections[-1]);
		if (rv & RV_DELIMITED) {
			return rv;
		}
	}

	if (!grammar->rules.empty() && !no_sections) {
		std::map<uint32_t,uint32_t> counter;
		// Caveat: This may look as if it is not recursing previous sections, but those rules are preprocessed into the successive sections so they are actually run.
		RSType::iterator iter = runsections.begin();
		RSType::iterator iter_end = runsections.end();
		for (; iter != iter_end ;) {
			if (iter->first < 0 || (section_max_count && counter[iter->first] >= section_max_count)) {
				++iter;
				continue;
			}
			uint32_t rv = 0;
			if (debug_level > 0) {
				std::cerr << "Running section " << iter->first << " (rules " << *(iter->second.begin()) << " through " << *(--(iter->second.end())) << ") on window " << current.number << std::endl;
			}
			rv = runRulesOnSingleWindow(current, iter->second);
			counter[iter->first]++;
			if (rv & RV_DELIMITED) {
				return rv;
			}
			if (!(rv & RV_SOMETHING)) {
				++iter;
			}
		}
	}

	if (!grammar->after_sections.empty() && !no_after_sections) {
		uint32_t rv = runRulesOnSingleWindow(current, runsections[-2]);
		if (rv & RV_DELIMITED) {
			return rv;
		}
	}

	return 0;
}

int GrammarApplicator::runGrammarOnWindow() {
	SingleWindow *current = gWindow->current;
	did_final_enclosure = false;

	if (has_dep) {
		reflowDependencyWindow();
		gWindow->dep_map.clear();
		gWindow->dep_window.clear();
		dep_highest_seen = 0;
	}

	indexSingleWindow(*current);

	has_enclosures = false;
	if (!grammar->parentheses.empty()) {
		label_scanParentheses:
		reverse_foreach (CohortVector, current->cohorts, iter, iter_end) {
			Cohort *c = *iter;
			if (c->is_pleft == 0) {
				continue;
			}
			uint32Map::const_iterator p = grammar->parentheses.find(c->is_pleft);
			if (p != grammar->parentheses.end()) {
				CohortVector::iterator right = iter.base();
				--right;
				--right;
				c = *right;
				++right;
				bool found = false;
				CohortVector encs;
				for (; right != current->cohorts.end() ; ++right) {
					Cohort *s = *right;
					encs.push_back(s);
					if (s->is_pright == p->second) {
						found = true;
						break;
					}
				}
				if (!found) {
					encs.clear();
				}
				else {
					CohortVector::iterator left = iter.base();
					--left;
					uint32_t lc = (*left)->local_number;
					++right;
					for (; right != current->cohorts.end() ; ++right) {
						*left = *right;
						(*left)->local_number = lc;
						++lc;
						++left;
					}
					current->cohorts.resize(current->cohorts.size() - encs.size());
					foreach (CohortVector, encs, eiter, eiter_end) {
						(*eiter)->type |= CT_ENCLOSED;
					}
					foreach (CohortVector, c->enclosed, eiter2, eiter2_end) {
						encs.push_back(*eiter2);
					}
					c->enclosed = encs;
					has_enclosures = true;
					goto label_scanParentheses;
				}
			}
		}
	}

	par_left_tag = 0;
	par_right_tag = 0;
	par_left_pos = 0;
	par_right_pos = 0;
	uint32_t pass = 0;

label_runGrammarOnWindow_begin:
	index_ruleCohort_no.clear();
	current = gWindow->current;

	++pass;
	if (trace_encl) {
		uint32_t hitpass = std::numeric_limits<uint32_t>::max() - pass;
		size_t nc = current->cohorts.size();
		for (size_t i=0 ; i<nc ; ++i) {
			Cohort *c = current->cohorts[i];
			foreach (ReadingList, c->readings, rit, rit_end) {
				(*rit)->hit_by.push_back(hitpass);
			}
		}
	}

	int rv = runGrammarOnSingleWindow(*current);
	if (rv & RV_DELIMITED) {
		goto label_runGrammarOnWindow_begin;
	}

	if (!grammar->parentheses.empty() && has_enclosures) {
		bool found = false;
		size_t nc = current->cohorts.size();
		for (size_t i=0 ; i<nc ; ++i) {
			Cohort *c = current->cohorts[i];
			if (!c->enclosed.empty()) {
				current->cohorts.resize(current->cohorts.size() + c->enclosed.size(), 0);
				size_t ne = c->enclosed.size();
				for (size_t j=nc-1 ; j>i ; --j) {
					current->cohorts[j+ne] = current->cohorts[j];
					current->cohorts[j+ne]->local_number = j+ne;
				}
				for (size_t j=0 ; j<ne ; ++j) {
					current->cohorts[i+j+1] = c->enclosed[j];
					current->cohorts[i+j+1]->local_number = i+j+1;
					current->cohorts[i+j+1]->parent = current;
					current->cohorts[i+j+1]->type &= ~CT_ENCLOSED;
				}
				par_left_tag = c->enclosed[0]->is_pleft;
				par_right_tag = c->enclosed[ne-1]->is_pright;
				par_left_pos = i+1;
				par_right_pos = i+ne;
				c->enclosed.clear();
				found = true;
				goto label_runGrammarOnWindow_begin;
			}
		}
		if (!found && !did_final_enclosure) {
			par_left_tag = 0;
			par_right_tag = 0;
			par_left_pos = 0;
			par_right_pos = 0;
			did_final_enclosure = true;
			goto label_runGrammarOnWindow_begin;
		}
	}

	return 0;
}

}
