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

using namespace CG3;
using namespace CG3::Strings;

bool GrammarApplicator::doesTagMatchSet(const uint32_t tag, const uint32_t set) {
	bool retval = false;

	stdext::hash_map<uint32_t, Set*>::const_iterator iter = grammar->sets_by_contents.find(set);
	if (iter != grammar->sets_by_contents.end()) {
		const Set *theset = iter->second;

		if (theset->single_tags.find(tag) != theset->single_tags.end()) {
			retval = true;
		}
		else {
			CompositeTag *ctag = new CompositeTag();
			ctag->addTag(tag);
			ctag->rehash();

			if (theset->tags.find(ctag->hash) != theset->tags.end()) {
				retval = true;
			}
			delete ctag;
		}
	}
	return retval;
}

inline bool GrammarApplicator::__index_matches(const stdext::hash_map<uint32_t, Index*> *me, const uint32_t value, const uint32_t set) {
	if (me->find(value) != me->end()) {
		const Index *index = me->find(value)->second;
		if (index->values.find(set) != index->values.end()) {
			cache_hits++;
			return true;
		}
	}
	return false;
}

bool GrammarApplicator::doesTagMatchReading(const Reading *reading, const uint32_t ztag, bool bypass_index) {
	bool retval = false;
	bool match = true;
	bypass_index = false;

	const Tag *tag = grammar->single_tags.find(ztag)->second;
	if (tag->type & T_VARIABLE) {
		if (variables.find(tag->comparison_hash) == variables.end()) {
			u_fprintf(ux_stderr, "Info: %u failed.\n", tag->comparison_hash);
			match = false;
		}
		else {
			u_fprintf(ux_stderr, "Info: %u matched.\n", tag->comparison_hash);
			match = true;
		}
	}
	else if (tag->type & T_NUMERICAL && !reading->tags_numerical.empty()) {
		match = false;
		std::set<uint32_t>::const_iterator mter;
		for (mter = reading->tags_numerical.begin() ; mter != reading->tags_numerical.end() ; mter++) {
			const Tag *itag = single_tags.find(*mter)->second;
			if (tag->comparison_hash == itag->comparison_hash) {
				if (tag->comparison_op == OP_EQUALS && itag->comparison_op == OP_EQUALS && tag->comparison_val == itag->comparison_val) {
					match = true;
				}
				else if (tag->comparison_op == OP_EQUALS && itag->comparison_op == OP_LESSTHAN && tag->comparison_val < itag->comparison_val) {
					match = true;
				}
				else if (tag->comparison_op == OP_EQUALS && itag->comparison_op == OP_GREATERTHAN && tag->comparison_val > itag->comparison_val) {
					match = true;
				}
				else if (tag->comparison_op == OP_LESSTHAN && itag->comparison_op == OP_EQUALS && tag->comparison_val > itag->comparison_val) {
					match = true;
				}
				else if (tag->comparison_op == OP_LESSTHAN && itag->comparison_op == OP_LESSTHAN) {
					match = true;
				}
				else if (tag->comparison_op == OP_LESSTHAN && itag->comparison_op == OP_GREATERTHAN && tag->comparison_val > itag->comparison_val) {
					match = true;
				}
				else if (tag->comparison_op == OP_GREATERTHAN && itag->comparison_op == OP_EQUALS && tag->comparison_val < itag->comparison_val) {
					match = true;
				}
				else if (tag->comparison_op == OP_GREATERTHAN && itag->comparison_op == OP_GREATERTHAN) {
					match = true;
				}
				else if (tag->comparison_op == OP_GREATERTHAN && itag->comparison_op == OP_LESSTHAN && tag->comparison_val < itag->comparison_val) {
					match = true;
				}
				if (match) {
					break;
				}
			}
		}
	}
	else if (tag->regexp && !reading->tags_textual.empty()) {
		std::set<uint32_t>::const_iterator mter;
		for (mter = reading->tags_textual.begin() ; mter != reading->tags_textual.end() ; mter++) {
			// ToDo: Cache regexp and icase hits/misses
			const Tag *itag = single_tags.find(*mter)->second;
			UErrorCode status = U_ZERO_ERROR;
			uregex_setText(tag->regexp, itag->tag, u_strlen(itag->tag), &status);
			if (status != U_ZERO_ERROR) {
				u_fprintf(ux_stderr, "Error: uregex_setText(MatchSet) returned %s - cannot continue!\n", u_errorName(status));
				exit(1);
			}
			status = U_ZERO_ERROR;
			match = (uregex_matches(tag->regexp, 0, &status) == TRUE);
			if (status != U_ZERO_ERROR) {
				u_fprintf(ux_stderr, "Error: uregex_matches(MatchSet) returned %s - cannot continue!\n", u_errorName(status));
				exit(1);
			}
			if (match) {
				break;
			}
		}
	}
	else if (reading->tags.find(ztag) == reading->tags.end()) {
		match = false;
		if (tag->type & T_NEGATIVE) {
			match = true;
		}
	}

	if (tag->type & T_NEGATIVE) {
		match = !match;
	}

	if (match) {
		match_single++;
		if (tag->type & T_FAILFAST) {
			retval = false;
		}
		else {
			retval = true;
		}
		if (tag->type & T_MAPPING || tag->tag[0] == grammar->mapping_prefix) {
			last_mapping_tag = tag->hash;
		}
	}
	return retval;
}

bool GrammarApplicator::doesSetMatchReading(const Reading *reading, const uint32_t set, bool bypass_index) {
	bool retval = false;

	assert(reading->hash != 0);

	if (reading->hash && reading->hash != 1) {
		if (!bypass_index && __index_matches(&index_reading_yes, reading->hash, set)) { return true; }
		if (__index_matches(&index_reading_no, reading->hash, set)) { return false; }
	}

	cache_miss++;

	stdext::hash_map<uint32_t, Set*>::const_iterator iter = grammar->sets_by_contents.find(set);
	if (iter != grammar->sets_by_contents.end()) {
		const Set *theset = iter->second;
		if (!theset->is_special) {
			bool possible = false;
			std::list<uint32_t>::const_iterator iter_tags;
			for (iter_tags = reading->tags_list.begin() ; iter_tags != reading->tags_list.end() ; iter_tags++) {
				if (grammar->sets_by_tag.find(*iter_tags) != grammar->sets_by_tag.end() && grammar->sets_by_tag.find(*iter_tags)->second->find(set) != grammar->sets_by_tag.find(*iter_tags)->second->end()) {
					possible = true;
					break;
				}
			}
			if (!possible) {
				if (reading->hash && reading->hash != 1) {
					if (index_reading_no.find(reading->hash) == index_reading_no.end()) {
						index_reading_no[reading->hash] = new Index();
					}
					index_reading_no[reading->hash]->values[set] = set;
				}
				return false;
			}
		}

		if (theset->match_any) {
			retval = true;
		}
		else if (theset->sets.empty()) {
			stdext::hash_set<uint32_t>::const_iterator ster;

			for (ster = theset->single_tags.begin() ; ster != theset->single_tags.end() ; ster++) {
				bool match = doesTagMatchReading(reading, *ster, bypass_index);
				if (match) {
					retval = true;
					break;
				}
			}

			if (!retval) {
				for (ster = theset->tags.begin() ; ster != theset->tags.end() ; ster++) {
					bool match = true;
					const CompositeTag *ctag = grammar->tags.find(*ster)->second;

					stdext::hash_set<uint32_t>::const_iterator cter;
					for (cter = ctag->tags.begin() ; cter != ctag->tags.end() ; cter++) {
						bool inner = doesTagMatchReading(reading, *cter, bypass_index);
						if (!inner) {
							match = false;
							break;
						}
					}
					if (match) {
						match_comp++;
						retval = true;
						break;
					} else {
						last_mapping_tag = 0;
					}
				}
			}
		}
		else {
			uint32_t size = (uint32_t)theset->sets.size();
			for (uint32_t i=0;i<size;i++) {
				bool match = doesSetMatchReading(reading, theset->sets.at(i), bypass_index);
				bool failfast = false;
				while (i < size-1 && theset->set_ops.at(i) != S_OR) {
					switch (theset->set_ops.at(i)) {
						case S_PLUS:
							if (match) {
								match = doesSetMatchReading(reading, theset->sets.at(i+1), bypass_index);
							}
							break;
						case S_FAILFAST:
							if (match) {
								if (doesSetMatchReading(reading, theset->sets.at(i+1), bypass_index)) {
									match = false;
									failfast = true;
								}
							}
							break;
						case S_MINUS:
							if (match) {
								if (doesSetMatchReading(reading, theset->sets.at(i+1), bypass_index)) {
									match = false;
								}
							}
							break;
						case S_NOT:
							if (!match) {
								if (!doesSetMatchReading(reading, theset->sets.at(i+1))) {
									match = true;
								}
							}
							break;
						default:
							break;
					}
					i++;
				}
				if (match) {
					match_sub++;
					retval = true;
					break;
				}
				if (failfast) {
					match_sub++;
					retval = false;
					break;
				}
			}
		}
	}

	if (retval) {
		if (reading->hash && reading->hash != 1) {
			if (index_reading_yes.find(reading->hash) == index_reading_yes.end()) {
				index_reading_yes[reading->hash] = new Index();
			}
			index_reading_yes[reading->hash]->values[set] = set;
		}
	}
	else {
		if (reading->hash && reading->hash != 1) {
			if (index_reading_no.find(reading->hash) == index_reading_no.end()) {
				index_reading_no[reading->hash] = new Index();
			}
			index_reading_no[reading->hash]->values[set] = set;
		}
	}

	return retval;
}

bool GrammarApplicator::doesSetMatchCohortNormal(const Cohort *cohort, const uint32_t set) {
	bool retval = false;
	std::list<Reading*>::const_iterator iter;
	for (iter = cohort->readings.begin() ; iter != cohort->readings.end() ; iter++) {
		Reading *reading = *iter;
		if (!reading->deleted) {
			if (doesSetMatchReading(reading, set)) {
				retval = true;
				break;
			}
		}
	}
	return retval;
}

bool GrammarApplicator::doesSetMatchCohortCareful(const Cohort *cohort, const uint32_t set) {
	bool retval = true;
	std::list<Reading*>::const_iterator iter;
	for (iter = cohort->readings.begin() ; iter != cohort->readings.end() ; iter++) {
		Reading *reading = *iter;
		if (!reading->deleted) {
			last_mapping_tag = 0;
			const Set *theset = grammar->sets_by_contents.find(set)->second;
			if (!doesSetMatchReading(reading, set, theset->has_mappings)) {
				retval = false;
				break;
			}
			// A mapped tag must be the only mapped tag in the reading to be considered a Careful match
			if (last_mapping_tag && reading->tags_mapped.size() > 1) {
				retval = false;
				break;
			}
		}
	}
	return retval;
}

Cohort *GrammarApplicator::doesSetMatchDependency(SingleWindow *sWindow, const Cohort *current, const ContextualTest *test) {
	Cohort *rv = 0;

	bool retval = false;
	if (test->dep_parent && current->dep_self != current->dep_parent) {
		if (sWindow->parent->cohort_map.find(current->dep_parent) == sWindow->parent->cohort_map.end()) {
			u_fprintf(ux_stderr, "Warning: Dependency %u does not exist - ignoring.\n", current->dep_parent);
			return 0;
		}

		Cohort *cohort = sWindow->parent->cohort_map.find(current->dep_parent)->second;
		bool good = true;
		if (current->parent != cohort->parent) {
			if ((!test->span_both || !test->span_left) && cohort->parent->number < current->parent->number) {
				good = false;
			}
			else if ((!test->span_both || !test->span_right) && cohort->parent->number > current->parent->number) {
				good = false;
			}

		}
		if (good) {
			if (test->careful) {
				retval = doesSetMatchCohortCareful(cohort, test->target);
			}
			else {
				retval = doesSetMatchCohortNormal(cohort, test->target);
			}
		}
		if (retval) {
			rv = cohort;
		}
	}
	else {
		const std::set<uint32_t> *deps = 0;
		if (test->dep_child) {
			deps = &current->dep_children;
		}
		else {
			deps = &current->dep_siblings;
		}

		std::set<uint32_t>::const_iterator dter;
		for (dter = deps->begin() ; dter != deps->end() ; dter++) {
			if (sWindow->parent->cohort_map.find(*dter) == sWindow->parent->cohort_map.end()) {
				u_fprintf(ux_stderr, "Warning: Dependency %u does not exist - ignoring.\n", *dter);
				continue;
			}
			Cohort *cohort = sWindow->parent->cohort_map.find(*dter)->second;
			bool good = true;
			if (current->parent != cohort->parent) {
				if ((!test->span_both || !test->span_left) && cohort->parent->number < current->parent->number) {
					good = false;
				}
				else if ((!test->span_both || !test->span_right) && cohort->parent->number > current->parent->number) {
					good = false;
				}

			}
			if (good) {
				if (test->careful) {
					retval = doesSetMatchCohortCareful(cohort, test->target);
				}
				else {
					retval = doesSetMatchCohortNormal(cohort, test->target);
				}
			}
			if (retval) {
				rv = cohort;
				break;
			}
		}
	}

	return rv;
}
