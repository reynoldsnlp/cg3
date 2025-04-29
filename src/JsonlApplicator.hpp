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

#ifndef JSONLAPPLICATOR_HPP
#define JSONLAPPLICATOR_HPP

#include "GrammarApplicator.hpp"
#include <boost/json.hpp>

namespace CG3 {

class JsonlApplicator : public virtual GrammarApplicator {
public:
	JsonlApplicator(std::ostream& ux_err);
	~JsonlApplicator() override;

	void runGrammarOnText(std::istream& input, std::ostream& output) override;

protected:
	void printCohort(Cohort* cohort, std::ostream& output, bool profiling = false) override;
	void printSingleWindow(SingleWindow* window, std::ostream& output, bool profiling = false) override;

private:
	void parseJsonCohort(const boost::json::object& obj, SingleWindow* cSWindow, Cohort*& cCohort);
	Reading* parseJsonReading(const boost::json::object& reading_obj, Cohort* parentCohort);
	void buildJsonReading(const Reading* reading, boost::json::object& reading_json);
	void buildJsonTags(const Reading* reading, boost::json::array& tags_json);
	boost::json::value cohortToJson(const Cohort* cohort, bool profiling);
	boost::json::value readingToJson(const Reading* reading);
};

} // namespace CG3

#endif // JSONLAPPLICATOR_HPP
