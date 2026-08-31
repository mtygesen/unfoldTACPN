/* PeTe - Petri Engine exTremE
 * Copyright (C) 2011  Jonas Finnemann Jensen <jopsen@gmail.com>,
 *                     Thomas Søndersø Nielsen <primogens@gmail.com>,
 *                     Lars Kærlund Østergaard <larsko@gmail.com>,
 *                     Peter Gjøl Jensen <root@petergjoel.dk>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef PNMLPARSER_H
#define PNMLPARSER_H

#include "TAPNBuilderInterface.h"
#include "PQL/PQL.h"
#include "Colored/ColoredNetStructures.h"
#include "Colored/Expressions.h"
#include "Colored/Colors.h"
#include "Colored/ColoredPetriNetBuilder.h"
#include "Colored/TimeInterval.h"

#include <unordered_map>
#include <map>
#include <unordered_set>
#include <string>
#include <vector>
#include <fstream>
#include <rapidxml.hpp>
#include "../types.hpp"

namespace unfoldtacpn {
class PNMLParser {
public:

    typedef std::unordered_map<std::string, const unfoldtacpn::Colored::ColorType*> ColorTypeMap;
    typedef std::unordered_map<std::string, const unfoldtacpn::Colored::Variable*> VariableMap;
    typedef std::vector<rapidxml::xml_node<>*> node_vector_t;

    PNMLParser() {
        _builder = nullptr;
    }

    void parse(std::istream& xml,
        ColoredPetriNetBuilder* builder);

private:
    int parseWeight(rapidxml::xml_node<>* element);
    unfoldtacpn::Colored::ArcExpression_ptr parseHLInscriptions(rapidxml::xml_node<>* element, const Colored::ColorType* type);
    std::vector<Colored::TimeInterval> parseTimeGuard(rapidxml::xml_node<>* element);
    void findNodes(rapidxml::xml_node<>* element, node_vector_t& colored_arc, node_vector_t& regular_arcs, node_vector_t& inhib_arcs, node_vector_t& trans_arcs, node_vector_t& transitions, node_vector_t& places, node_vector_t& custom_distributions);
    types::InitialMarkingAges parseInitialMarkingAges(rapidxml::xml_node<>* element, const Colored::ColorType* type);
    void parsePlace(rapidxml::xml_node<>* element);
    std::pair<std::string, std::vector<const unfoldtacpn::Colored::Color*>> parseTimeConstraint(rapidxml::xml_node<> *element);
    void parseArc(rapidxml::xml_node<>* element, bool inhibitor = false);
    void handleArc(rapidxml::xml_node<>* element);
    void parseTransition(rapidxml::xml_node<>* element);
    void parseCustomDistribution(rapidxml::xml_node<>* element);
    void parseDeclarations(rapidxml::xml_node<>* element);
    void parseNamedSort(rapidxml::xml_node<>* element);
    unfoldtacpn::Colored::ArcExpression_ptr parseArcExpression(rapidxml::xml_node<>* element, const Colored::ColorType* type);
    unfoldtacpn::Colored::GuardExpression_ptr parseGuardExpression(rapidxml::xml_node<>* element, const Colored::ColorType* type);
    const Colored::ColorType* inferGuardColorType(rapidxml::xml_node<>* element);
    unfoldtacpn::Colored::ColorExpression_ptr parseColorExpression(rapidxml::xml_node<>* element, const Colored::ColorType* type);
    unfoldtacpn::Colored::AllExpression_ptr parseAllExpression(rapidxml::xml_node<>* element);
    const unfoldtacpn::Colored::ColorType* parseUserSort(rapidxml::xml_node<>* element);
    unfoldtacpn::Colored::ArcExpression_ptr parseNumberOfExpression(rapidxml::xml_node<>* element, const Colored::ColorType* type);
    void collectColorsInTuple(rapidxml::xml_node<>* element, std::vector<std::vector<unfoldtacpn::Colored::ColorExpression_ptr>>& collectedColors, const Colored::ColorType* type);
    unfoldtacpn::Colored::ArcExpression_ptr constructAddExpressionFromTupleExpression(rapidxml::xml_node<>* element,
    const std::vector<std::vector<unfoldtacpn::Colored::ColorExpression_ptr>>& collectedColors, uint32_t numberof, const Colored::ColorType* type);
    std::vector<std::vector<unfoldtacpn::Colored::ColorExpression_ptr>> cartesianProduct(const std::vector<unfoldtacpn::Colored::ColorExpression_ptr>& rightSet, const std::vector<unfoldtacpn::Colored::ColorExpression_ptr>& leftSet);
    std::vector<std::vector<unfoldtacpn::Colored::ColorExpression_ptr>> cartesianProduct(const std::vector<std::vector<unfoldtacpn::Colored::ColorExpression_ptr>>& rightSet, const std::vector<unfoldtacpn::Colored::ColorExpression_ptr>& leftSet);
    void parseTransportArc(rapidxml::xml_node<>* element);
    void parseSingleTransportArc(rapidxml::xml_node<>* element);
    void parseValue(rapidxml::xml_node<>* element, std::string& text);
    uint32_t parseNumberConstant(rapidxml::xml_node<>* element);
    void parsePosition(rapidxml::xml_node<>* element, double& x, double& y);
    std::tuple<Colored::SMC::Distribution, Colored::SMC::DistributionParameters> parseDistribution(rapidxml::xml_node<>* element);
    bool isTransition(const std::string& s);
    void checkKeyword(const char* id);
    unfoldtacpn::ColoredPetriNetBuilder* _builder;
    ColorTypeMap _colorTypes;
    VariableMap _variables;
    ColorTypeMap _place_types;
    std::unordered_map<std::string, uint32_t> constantValues;
    std::map<std::pair<std::string,std::string>, rapidxml::xml_node<>*> _transportArcs;
    std::unordered_set<std::string> _used_keywords;
    Colored::ScopeType _global_scope;
    std::unordered_map<std::string, Colored::SMC::DistributionParameters> _customDistributions;
};
}
#endif // PNMLPARSER_H
