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

#include <string>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <istream>
#include <cstring>
#include <stdexcept>
#include <cassert>


#include "PetriParse/PNMLParser.h"
#include "errorcodes.h"
#include "types.hpp"

using namespace unfoldtacpn::PQL;

inline bool stringToBool(std::string b)
{
    return b == "true" ? 1 : 0;
}

namespace unfoldtacpn {
void PNMLParser::parse(std::istream& xml,
        ColoredPetriNetBuilder* builder) {
    //Clear any left overs
    _colorTypes.clear();

    //Set the builder
    this->_builder = builder;

    //Parser the xml
    rapidxml::xml_document<> doc;
    std::vector<char> buffer((std::istreambuf_iterator<char>(xml)), std::istreambuf_iterator<char>());
    buffer.push_back('\0');
    doc.parse<0>(&buffer[0]);

    rapidxml::xml_node<>* root = doc.first_node();

    if(root == nullptr)
    {
        std::cerr << "ERROR: Could not open the file for reading " << std::endl;
    }

    if(strcmp(root->name(), "pnml") != 0)
    {
        std::cerr << "ERROR: expecting <pnml> tag as root-node in xml tree." << std::endl;
        exit(ErrorCode);
    }

    auto declarations = root->first_node("declaration");
    if(declarations == nullptr){
        declarations = root->first_node("net")->first_node("declaration");
    }

    {   // add the default color-type
        auto ct = unfoldtacpn::Colored::Color::dotConstant()->getColorType();
        _colorTypes["dot"] = ct;
        builder->addColorType("dot", ct);
        _global_scope.addType(ct);
    }

    if (declarations) {
        parseDeclarations(declarations);
    }

    for (auto it = root->first_node(); it; it = it->next_sibling()) {
        if (strcmp(it->name(), "constant") == 0)
            constantValues[it->first_attribute("name")->value()] = atoi(it->first_attribute("value")->value());
         else
            break;
    }

    // we need to parse things in order, so first find the nodes
    node_vector_t regular_arcs, colored_arc, places,
                  inhib_arcs, trans_arcs, transitions, custom_distributions;
    findNodes(root, colored_arc, regular_arcs, inhib_arcs, trans_arcs, transitions, places, custom_distributions);
    for(auto* d : custom_distributions)
        parseCustomDistribution(d);
    for(auto* p : places)
        parsePlace(p);
    for(auto* trans : transitions)
        parseTransition(trans);
    for(auto* a : regular_arcs)
        parseArc(a, false);
    for(auto* ia : inhib_arcs)
        parseArc(ia, true);
    for(auto* ta : trans_arcs)
        parseTransportArc(ta);
    for(auto* ca : colored_arc)
        handleArc(ca);

    //Cleanup
    if(!_transportArcs.empty())
    {
        std::cerr << "ERROR: Could not match the following transport-arcs";
        for(auto& kv : _transportArcs)
        {
            std::cerr << "\tgoing through transition " << kv.first.first << " with id " << kv.first.second << std::endl;
        }
        std::exit(ErrorCode);
    }
    _transportArcs.clear();
    transitions.clear();
    _colorTypes.clear();
}

void PNMLParser::parseDeclarations(rapidxml::xml_node<>* element) {
    for (auto it = element->first_node(); it; it = it->next_sibling()) {
        if (strcmp(it->name(), "namedsort") == 0) {
            parseNamedSort(it);
        } else if (strcmp(it->name(), "variabledecl") == 0) {
            auto sort = parseUserSort(it);
            auto id = it->first_attribute("id")->value();
            auto var = new unfoldtacpn::Colored::Variable {id,sort};
            checkKeyword(id);
            _variables[id] = var;
        } else {
            parseDeclarations(element->first_node());
        }
    }
}

void PNMLParser::parseCustomDistribution(rapidxml::xml_node<>* element) {
    auto name = element->first_attribute("name")->value();
    Colored::SMC::DistributionParameters params;
    for (auto it = element->first_node("value"); it; it = it->next_sibling("value")) {
        params.push_back(atof(it->value()));
    }
    auto randomStartAttr = element->first_attribute("randomStart");
    if (randomStartAttr && strcmp(randomStartAttr->value(), "true") == 0) {
        params.customDistributionRandomStart = true;
    }
    _customDistributions[name] = params;
}

void PNMLParser::parseNamedSort(rapidxml::xml_node<>* element) {
    auto type = element->first_node();
    bool is_product = strcmp(type->name(), "productsort") == 0;
    std::string id = element->first_attribute("id")->value();
    auto* ct = is_product?
              new unfoldtacpn::Colored::ProductType(std::string(element->first_attribute("id")->value())) :
              new unfoldtacpn::Colored::ColorType(std::string(element->first_attribute("id")->value()));
    if (strcmp(type->name(), "dot") == 0) {
        return;
    } else if (is_product) {
        for (auto it = type->first_node(); it; it = it->next_sibling()) {
            if (strcmp(it->name(), "usersort") == 0) {
                auto* child_type = _colorTypes[it->first_attribute("declaration")->value()];
                ((unfoldtacpn::Colored::ProductType*)ct)->addType(child_type);
            }
            else
            {
                throw "Cannot to deal with int-range in product sort";
            }
        }
    } else if (strcmp(type->name(), "finiteintrange") == 0) {
        uint32_t start = (uint32_t)atoll(type->first_attribute("start")->value());
        uint32_t end = (uint32_t)atoll(type->first_attribute("end")->value());
        for (uint32_t i = start; i<=end;i++) {
            auto str = std::to_string(i);
            ct->addColor(str.c_str());
        }
    } else {
        for (auto it = type->first_node(); it; it = it->next_sibling()) {
            auto id = it->first_attribute("id")->value();
            assert(id != nullptr);
            checkKeyword(id);
            ct->addColor(id);
        }
    }

    _colorTypes[id] = ct;
    _builder->addColorType(id, ct);
    _global_scope.addType(ct);
}

unfoldtacpn::Colored::ArcExpression_ptr PNMLParser::parseArcExpression(rapidxml::xml_node<>* element, const Colored::ColorType* type) {
    if (strcmp(element->name(), "numberof") == 0) {
        return parseNumberOfExpression(element, type);
    } else if (strcmp(element->name(), "add") == 0) {
        std::vector<unfoldtacpn::Colored::ArcExpression_ptr> constituents;
        for (auto it = element->first_node(); it; it = it->next_sibling()) {
            constituents.push_back(parseArcExpression(it, type));
        }
        return std::make_shared<unfoldtacpn::Colored::AddExpression>(std::move(constituents));
    } else if (strcmp(element->name(), "subtract") == 0) {
        auto left = element->first_node();
        auto right = left->next_sibling();
        auto res = std::make_shared<unfoldtacpn::Colored::SubtractExpression>(parseArcExpression(left, type), parseArcExpression(right, type));
        auto next = right;
        while ((next = next->next_sibling())) {
            res = std::make_shared<unfoldtacpn::Colored::SubtractExpression>(res, parseArcExpression(next, type));
        }
        return res;
    } else if (strcmp(element->name(), "scalarproduct") == 0) {
        auto scalar = element->first_node();
        auto ms = scalar->next_sibling();
        return std::make_shared<unfoldtacpn::Colored::ScalarProductExpression>(parseArcExpression(ms, type), parseNumberConstant(scalar));
    } else if (strcmp(element->name(), "all") == 0) {
        return parseNumberOfExpression(element->parent(), type);
    } else if (strcmp(element->name(), "subterm") == 0 || strcmp(element->name(), "structure") == 0) {
        return parseArcExpression(element->first_node(), type);
    }
    printf("Could not parse '%s' as an arc expression\n", element->name());
    assert(false);
    return nullptr;
}

unfoldtacpn::Colored::GuardExpression_ptr PNMLParser::parseGuardExpression(rapidxml::xml_node<>* element) {
    const auto operands = [this](rapidxml::xml_node<>* left, rapidxml::xml_node<>* right) {
        const auto* leftType = inferGuardColorType(left);
        const auto* rightType = inferGuardColorType(right);
        if (leftType == nullptr && rightType == nullptr) {
            throw std::invalid_argument("There must be at least one variable in each guard comparison.");
        }
        auto colors = std::make_pair(
            parseColorExpression(left, leftType == nullptr ? rightType : leftType, false),
            parseColorExpression(right, rightType == nullptr ? leftType : rightType, false));
        if (colors.first->getColorType() != colors.second->getColorType()) {
            throw std::invalid_argument("Both operands of each guard comparison must have the same color type.");
        }
        
        return colors;
    };
    if (strcmp(element->name(), "lt") == 0 || strcmp(element->name(), "lessthan") == 0) {
        auto left = element->first_node();
        auto right = left->next_sibling();
        auto colors = operands(left, right);
        return std::make_shared<unfoldtacpn::Colored::LessThanExpression>(std::move(colors.first), std::move(colors.second));
    } else if (strcmp(element->name(), "gt") == 0 || strcmp(element->name(), "greaterthan") == 0) {
        auto left = element->first_node();
        auto right = left->next_sibling();
        auto colors = operands(left, right);
        return std::make_shared<unfoldtacpn::Colored::GreaterThanExpression>(std::move(colors.first), std::move(colors.second));
    } else if (strcmp(element->name(), "leq") == 0 || strcmp(element->name(), "lessthanorequal") == 0) {
        auto left = element->first_node();
        auto right = left->next_sibling();
        auto colors = operands(left, right);
        return std::make_shared<unfoldtacpn::Colored::LessThanEqExpression>(std::move(colors.first), std::move(colors.second));
    } else if (strcmp(element->name(), "geq") == 0 || strcmp(element->name(), "greaterthanorequal") == 0) {
        auto left = element->first_node();
        auto right = left->next_sibling();
        auto colors = operands(left, right);
        return std::make_shared<unfoldtacpn::Colored::GreaterThanEqExpression>(std::move(colors.first), std::move(colors.second));
    } else if (strcmp(element->name(), "eq") == 0 || strcmp(element->name(), "equality") == 0) {
        auto left = element->first_node();
        auto right = left->next_sibling();
        auto colors = operands(left, right);
        return std::make_shared<unfoldtacpn::Colored::EqualityExpression>(std::move(colors.first), std::move(colors.second));
    } else if (strcmp(element->name(), "neq") == 0 || strcmp(element->name(), "inequality") == 0) {
        auto left = element->first_node();
        auto right = left->next_sibling();
        auto colors = operands(left, right);
        return std::make_shared<unfoldtacpn::Colored::InequalityExpression>(std::move(colors.first), std::move(colors.second));
    } else if (strcmp(element->name(), "not") == 0) {
        return std::make_shared<unfoldtacpn::Colored::NotExpression>(parseGuardExpression(element->first_node()));
    } else if (strcmp(element->name(), "and") == 0) {
        auto left = element->first_node();
        auto right = left->next_sibling();
        return std::make_shared<unfoldtacpn::Colored::AndExpression>(parseGuardExpression(left), parseGuardExpression(right));
    } else if (strcmp(element->name(), "or") == 0) {
        auto left = element->first_node();
        auto right = left->next_sibling();
        return std::make_shared<unfoldtacpn::Colored::OrExpression>(parseGuardExpression(left), parseGuardExpression(right));
    } else if (strcmp(element->name(), "subterm") == 0 || strcmp(element->name(), "structure") == 0) {
        return parseGuardExpression(element->first_node());
    }

    printf("Could not parse '%s' as a guard expression\n", element->name());
    assert(false);
    return nullptr;
}

const Colored::ColorType* PNMLParser::inferGuardColorType(rapidxml::xml_node<>* element) {
    const Colored::ColorType* type = nullptr;
    std::vector<rapidxml::xml_node<>*> pending {element};
    while (!pending.empty()) {
        auto* node = pending.back();
        pending.pop_back();
        if (strcmp(node->name(), "variable") == 0) {
            auto variable = _variables.find(node->first_attribute("refvariable")->value());
            if (variable == _variables.end()) {
                throw std::invalid_argument("Unknown variable in guard expression.");
            }

            if (type == nullptr) type = variable->second->colorType;
        }

        for (auto* child = node->first_node(); child; child = child->next_sibling()) {
            pending.push_back(child);
        }
    }

    return type;
}

unfoldtacpn::Colored::ColorExpression_ptr PNMLParser::parseColorExpression(rapidxml::xml_node<>* element, const Colored::ColorType* type, bool resolveNamedFromType) {
    if (strcmp(element->name(), "dotconstant") == 0) {
        return std::make_shared<unfoldtacpn::Colored::DotConstantExpression>();
    } else if (strcmp(element->name(), "variable") == 0) {
        const auto* variable = _variables[element->first_attribute("refvariable")->value()];
        if (type != &_global_scope && !(*type == *variable->colorType)) {
            throw std::invalid_argument("Variable does not belong to the expected color type.");
        }
        
        return std::make_shared<unfoldtacpn::Colored::VariableExpression>(variable);
    } else if (strcmp(element->name(), "useroperator") == 0) {
        auto declaration = element->first_attribute("declaration")->value();
        if (!resolveNamedFromType) {
            return std::make_shared<unfoldtacpn::Colored::UserOperatorExpression>(&_global_scope[declaration]);
        }
        for (const auto& color : *type) {
            if (!color.isTuple() && color.getColorName() == declaration) {
                return std::make_shared<unfoldtacpn::Colored::UserOperatorExpression>(&color);
            }
        }

        throw std::invalid_argument("Guard constant does not belong to the guard variable color type.");
    } else if (strcmp(element->name(), "successor") == 0) {
        return std::make_shared<unfoldtacpn::Colored::SuccessorExpression>(parseColorExpression(element->first_node(), type, resolveNamedFromType));
    } else if (strcmp(element->name(), "predecessor") == 0) {
        return std::make_shared<unfoldtacpn::Colored::PredecessorExpression>(parseColorExpression(element->first_node(), type, resolveNamedFromType));
    } else if (strcmp(element->name(), "tuple") == 0) {
        std::vector<unfoldtacpn::Colored::ColorExpression_ptr> colors;
        auto* pt = static_cast<const Colored::ProductType*>(type);
        size_t i = 0;
        for (auto it = element->first_node(); it; it = it->next_sibling()) {
            if(type != &_global_scope)
                colors.push_back(parseColorExpression(it, pt->getType(i), resolveNamedFromType));
            else
                colors.push_back(parseColorExpression(it, type, resolveNamedFromType));
            ++i;
        }
        if(type != &_global_scope)
            return std::make_shared<unfoldtacpn::Colored::TupleExpression>(std::move(colors), type);
        else
            return std::make_shared<unfoldtacpn::Colored::TupleExpression>(std::move(colors), nullptr);
    } else if (strcmp(element->name(), "subterm") == 0 || strcmp(element->name(), "structure") == 0) {
        return parseColorExpression(element->first_node(), type, resolveNamedFromType);
    }
    else if (strcmp(element->name(), "finiteintrangeconstant") == 0)
    {
        auto value = atoll(element->first_attribute("value")->value());
        auto intRangeElement = element->first_node("finiteintrange");
        auto start = intRangeElement->first_attribute("start")->value();
        auto end = intRangeElement->first_attribute("end")->value();
        auto si = atoll(start);
        auto ei = atoll(end);
        if (type != &_global_scope) {
            if (value >= si && value <= ei && type->size() == size_t(ei - si) + 1 && type->begin()->getColorName() == start) {
                const auto* color = &(*type)[static_cast<size_t>(value - si)];
                return std::make_shared<unfoldtacpn::Colored::UserOperatorExpression>(color);
            }

            throw std::invalid_argument("Guard constant does not belong to the guard variable color type.");
        }

        for(auto [_, ct] : _colorTypes)
        {
            if(ct->size() != size_t(ei-si)+1) continue;
            if(ct->begin()->getColorName().compare(start) == 0)
            {
                assert(value >= si);
                const unfoldtacpn::Colored::Color* color = &(*ct)[(size_t)(value-si)];
                auto res = std::make_shared<unfoldtacpn::Colored::UserOperatorExpression>(color);
                return std::dynamic_pointer_cast<unfoldtacpn::Colored::ColorExpression>(res);
            }
        }
    }
    assert(false);
    return nullptr;
}

unfoldtacpn::Colored::AllExpression_ptr PNMLParser::parseAllExpression(rapidxml::xml_node<>* element) {
    if (strcmp(element->name(), "all") == 0) {
        return std::make_shared<unfoldtacpn::Colored::AllExpression>(parseUserSort(element));
    } else if (strcmp(element->name(), "subterm") == 0) {
        return parseAllExpression(element->first_node());
    }

    return nullptr;
}

const unfoldtacpn::Colored::ColorType* PNMLParser::parseUserSort(rapidxml::xml_node<>* element) {
    if (element) {
        for (auto it = element->first_node(); it; it = it->next_sibling()) {
            if (strcmp(it->name(), "usersort") == 0) {
                return _colorTypes[it->first_attribute("declaration")->value()];
            } else if (strcmp(it->name(), "structure") == 0
                    || strcmp(it->name(), "type") == 0
                    || strcmp(it->name(), "subterm") == 0) {
                return parseUserSort(it);
            }
        }
    }
    assert(false);
    return nullptr;
}

unfoldtacpn::Colored::ArcExpression_ptr PNMLParser::parseNumberOfExpression(rapidxml::xml_node<>* element, const Colored::ColorType* type) {
    auto num = element->first_node();
    uint32_t number = parseNumberConstant(num);
    rapidxml::xml_node<>* first;
    if (number) {
        first = num->next_sibling();
    } else {
        number = 1;
        first = num;
    }

    if(strcmp(first->first_node()->name(), "tuple") == 0){
        std::vector<std::vector<unfoldtacpn::Colored::ColorExpression_ptr>> collectedColors;
        collectColorsInTuple(first->first_node(), collectedColors, type);
        return constructAddExpressionFromTupleExpression(first->first_node(), collectedColors, number, type);
    }

    auto allExpr = parseAllExpression(first);
    if (allExpr) {
        return std::make_shared<unfoldtacpn::Colored::NumberOfExpression>(std::move(allExpr), number);
    } else {
        std::vector<unfoldtacpn::Colored::ColorExpression_ptr> colors;
        for (auto it = first; it; it = it->next_sibling()) {
            colors.push_back(parseColorExpression(it, type));
        }
        return std::make_shared<unfoldtacpn::Colored::NumberOfExpression>(std::move(colors), number);
    }
}

void PNMLParser::collectColorsInTuple(rapidxml::xml_node<>* element,
    std::vector<std::vector<unfoldtacpn::Colored::ColorExpression_ptr>>& collectedColors,
    const Colored::ColorType* type){
    if (strcmp(element->name(), "tuple") == 0) {
        size_t i = 0;
        for (auto it = element->first_node(); it; it = it->next_sibling()) {
            auto* prod = dynamic_cast<const Colored::ProductType*>(type);
            assert(prod);
            collectColorsInTuple(it->first_node(), collectedColors, prod->getType(i));
            ++i;
        }
    } else if (strcmp(element->name(), "all") == 0) {
        std::vector<unfoldtacpn::Colored::ColorExpression_ptr> expressionsToAdd;
        auto expr = parseAllExpression(element);
        std::unordered_map<uint32_t, std::vector<const unfoldtacpn::Colored::Color *>> constantMap;
        uint32_t index = 0;
        expr->getConstants(constantMap, index);
        for(auto& positionColors : constantMap){
            for(auto& color : positionColors.second){
                expressionsToAdd.push_back(std::make_shared<unfoldtacpn::Colored::UserOperatorExpression>(color));
            }
        }
        collectedColors.push_back(expressionsToAdd);
        return;
    } else if (strcmp(element->name(), "subterm") == 0 || strcmp(element->name(), "structure") == 0) {
        collectColorsInTuple(element->first_node(), collectedColors, type);
    } else if (strcmp(element->name(), "useroperator") == 0 || strcmp(element->name(), "dotconstant") == 0 || strcmp(element->name(), "variable") == 0
                    || strcmp(element->name(), "successor") == 0 || strcmp(element->name(), "predecessor") == 0 || strcmp(element->name(), "finiteintrangeconstant") == 0) {
        std::vector<unfoldtacpn::Colored::ColorExpression_ptr> expressionsToAdd;
        auto color = parseColorExpression(element, type);
        expressionsToAdd.push_back(color);
        collectedColors.push_back(expressionsToAdd);
        return;
    } else {
        throw "Could not collect tuple colors from arc expression";
        return;
    }
}

unfoldtacpn::Colored::ArcExpression_ptr PNMLParser::constructAddExpressionFromTupleExpression(rapidxml::xml_node<>* element, const std::vector<std::vector<unfoldtacpn::Colored::ColorExpression_ptr>>& collectedColors, uint32_t numberof, const Colored::ColorType* type){
    auto initCartesianSet = cartesianProduct(collectedColors[0], collectedColors[1]);
    for(uint32_t i = 2; i < collectedColors.size(); i++){
        initCartesianSet = cartesianProduct(initCartesianSet, collectedColors[i]);
    }
    std::vector<unfoldtacpn::Colored::ArcExpression_ptr> constituents;
    for(const auto& set : initCartesianSet){
        std::vector<unfoldtacpn::Colored::ColorExpression_ptr> colors;
        for (const auto& color : set) {
            colors.push_back(color);
        }
        std::shared_ptr<unfoldtacpn::Colored::TupleExpression> tupleExpr = std::make_shared<unfoldtacpn::Colored::TupleExpression>(std::move(colors), type);
        std::vector<unfoldtacpn::Colored::ColorExpression_ptr> placeholderVector;
        placeholderVector.push_back(tupleExpr);
        constituents.emplace_back(std::make_shared<unfoldtacpn::Colored::NumberOfExpression>(std::move(placeholderVector),numberof));
    }
    return std::make_shared<unfoldtacpn::Colored::AddExpression>(std::move(constituents));
}

std::vector<std::vector<unfoldtacpn::Colored::ColorExpression_ptr>> PNMLParser::cartesianProduct
    (const std::vector<unfoldtacpn::Colored::ColorExpression_ptr>& rightSet,
    const std::vector<unfoldtacpn::Colored::ColorExpression_ptr>& leftSet){
    std::vector<std::vector<unfoldtacpn::Colored::ColorExpression_ptr>> returnSet;
    for(auto& expr : rightSet){
        for(auto& expr2 : leftSet){
            std::vector<unfoldtacpn::Colored::ColorExpression_ptr> toAdd;
            toAdd.push_back(expr);
            toAdd.push_back(expr2);
            returnSet.push_back(toAdd);
        }
    }
    return returnSet;
}
std::vector<std::vector<unfoldtacpn::Colored::ColorExpression_ptr>> PNMLParser::cartesianProduct
    (const std::vector<std::vector<unfoldtacpn::Colored::ColorExpression_ptr>>& rightSet,
    const std::vector<unfoldtacpn::Colored::ColorExpression_ptr>& leftSet){
    std::vector<std::vector<unfoldtacpn::Colored::ColorExpression_ptr>> returnSet;
    for(auto& set : rightSet){
        for(auto& expr2 : leftSet){
            returnSet.push_back(set);
            returnSet.back().emplace_back(expr2);
        }
    }
    return returnSet;
}

void PNMLParser::findNodes(rapidxml::xml_node<>* element,
    node_vector_t& colored_arc, node_vector_t& regular_arcs, node_vector_t& inhib_arcs, node_vector_t& trans_arcs, node_vector_t& transitions, node_vector_t& places, node_vector_t& custom_distributions) {

    for (auto it = element->first_node(); it; it = it->next_sibling()) {
        if (strcmp(it->name(), "place") == 0) {
            places.emplace_back(it);
        } else if (strcmp(it->name(),"transition") == 0) {
            transitions.emplace_back(it);
        } else if ( strcmp(it->name(), "inputArc") == 0) {
            regular_arcs.emplace_back(it);
        } else if ( strcmp(it->name(), "outputArc") == 0) {
            regular_arcs.emplace_back(it);
        } else if (strcmp(it->name(),"transportArc") == 0) {
            trans_arcs.emplace_back(it);
        } else if (strcmp(it->name(),"inhibitorArc") == 0) {
            inhib_arcs.emplace_back(it);
        } else if (strcmp(it->name(),"arc") == 0) {
            colored_arc.emplace_back(it);
        } else if (strcmp(it->name(), "custom_distribution") == 0) {
            custom_distributions.emplace_back(it);
        } else if (strcmp(it->name(), "variable") == 0) {
            std::cerr << "ERROR: variable not supported" << std::endl;
            exit(ErrorCode);
        }
        else
        {
            findNodes(it, colored_arc, regular_arcs, inhib_arcs, trans_arcs, transitions, places, custom_distributions);
        }
    }
}

void PNMLParser::handleArc(rapidxml::xml_node<>* element)
{
    auto t = element->first_attribute("type")->value();
    if(t == nullptr ||
       strcmp(t, "normal") == 0 ||
       strcmp(t, "timed") == 0)
    {
        parseArc(element, false);
    }
    else if(strcmp(t, "inhibitor") == 0)
    {
        parseArc(element, true);
    }
    else if(strcmp(t, "tapnInhibitor") == 0)
    {
        parseArc(element, true);
    }
    else if(strcmp(t, "transport") == 0)
    {
        parseSingleTransportArc(element);
    }
    else
    {
        std::cerr << "ERROR: Arc type '" << t << "' not supported";
        std::exit(ErrorCode);
    }
}

std::pair<std::string, std::vector<const Colored::Color*>> PNMLParser::parseTimeConstraint(rapidxml::xml_node<> *element) { // parses the inscription and colors belonging to either an interval or invariant and returns it.
    std::string colorTypeName;
    std::string inscription;
    std::vector<const Colored::Color*> colors;
    for (auto i = element->first_node(); i; i = i->next_sibling()) {
        if (strcmp(i->name(), "colortype") == 0) {
            colorTypeName = i->first_attribute("name")->value();

            if (_colorTypes.find(colorTypeName) == _colorTypes.end()) {
                std::cerr << "ERROR: The color type " << colorTypeName << " does not exist" << std::endl;
                std::exit(ErrorCode);
            }
            else {
                size_t id = 0;
                auto* type = _colorTypes[colorTypeName];
                for (auto it = i->first_node(); it; it = it->next_sibling()) {
                    if (strcmp(it->name(), "color") == 0) {
                        if(auto* prod = dynamic_cast<const Colored::ProductType*>(type))
                        {
                            colors.emplace_back(&(*prod->getType(id))[it->first_attribute("value")->value()]);
                        }
                        else
                        {
                            colors.emplace_back(&(*type)[it->first_attribute("value")->value()]);
                        }
                    }
                    else {
                        std::cerr << "ERROR: The colortype to the place element " << element->first_attribute("id")->value() << " does not have or should only have colors" << std::endl;
                        exit(ErrorCode);
                    }
                    ++id;
                }
            }
        } else if (strcmp(i->name(), "inscription") == 0) {
            inscription = i->first_attribute("inscription")->value();
        }
    }
    return std::make_pair(inscription, colors);
}

void PNMLParser::parsePlace(rapidxml::xml_node<>* element) {
    double x = 0, y = 0;
    std::string starInvariant;
    std::string id(element->first_attribute("id")->value());

    auto initial = element->first_attribute("initialMarking");
    long long initialMarking = 0;
    unfoldtacpn::Colored::Multiset hlinitialMarking;
    const unfoldtacpn::Colored::ColorType* type = nullptr;
    std::vector<unfoldtacpn::Colored::TimeInvariant> timeInvariants;
    if(initial)
         initialMarking = atoll(initial->value());

    auto posX = element->first_attribute("positionX");
    if (posX != nullptr){
         x = atoll(posX->value());
    }
    auto posY = element->first_attribute("positionY");
    if ( posY != nullptr){
        y = atoll(posY->value());
    }
    auto invariant = element->first_attribute("invariant");
    if(invariant != nullptr){
        starInvariant = invariant->value();
    }


    std::vector<const Colored::Color*> colors;
    colors.push_back(new Colored::Color());
    timeInvariants.push_back(Colored::TimeInvariant::createFor(starInvariant, colors, constantValues));

    bool found_hl = false;
    types::InitialMarkingAges initialAges;

    // we first need the type
    if(auto* node = element->first_node("type"))
    {
        _place_types[id] = type = parseUserSort(node);
    }
    else
    {
        _place_types[id] = type = _colorTypes["dot"];
    }
    for (auto it = element->first_node(); it; it = it->next_sibling()) {
        // name element is ignored
        if (strcmp(it->name(), "colorinvariant") == 0) {
            auto pair = parseTimeConstraint(it);
            timeInvariants.push_back(Colored::TimeInvariant::createFor(pair.first, pair.second, constantValues));
        } else if (strcmp(it->name(),"hlinitialMarking") == 0) {
            unfoldtacpn::Colored::ExpressionContext::BindingMap binding;
            unfoldtacpn::Colored::ExpressionContext::TypeMap typeMap{{type->getName(), type}};
            unfoldtacpn::Colored::ExpressionContext context {binding, typeMap};
            hlinitialMarking = parseArcExpression(it->first_node("structure"), type)->eval(context);
            found_hl = true;
        } else if (strcmp(it->name(), "initialMarkingAge") == 0) {
            initialAges = parseInitialMarkingAges(it, type);
        } 
    }

    if(initialMarking >= std::numeric_limits<int>::max())
    {
        std::cerr << "ERROR: Number of tokens in " << id << " exceeded " << std::numeric_limits<int>::max() << std::endl;
        exit(ErrorCode);
    }

    if(!found_hl && type->size() == 1)
    {
        hlinitialMarking = unfoldtacpn::Colored::Multiset(&(*type)[0], initialMarking);
    }
    _builder->addPlace(id, std::move(hlinitialMarking), type, timeInvariants, std::move(initialAges), x, y);
}

types::InitialMarkingAges PNMLParser::parseInitialMarkingAges(rapidxml::xml_node<>* element, const Colored::ColorType* type) {
    types::InitialMarkingAges ages;
    ages.reserve(type->size());
    for (auto it = element->first_node("token"); it; it = it->next_sibling("token")) {
        auto colorAttr = it->first_attribute("color");
        std::string colorName = colorAttr ? colorAttr->value() : "dot";
        auto colorId = (*type)[colorName].getId();
        auto ageStr = it->first_attribute("age")->value();
        assert(std::stoll(ageStr) >= 0 && "Age must be non-negative");
        ages[colorId].push_back(std::stoul(ageStr));
    }

    return ages;
}

unfoldtacpn::Colored::ArcExpression_ptr PNMLParser::parseHLInscriptions(rapidxml::xml_node<>* element, const Colored::ColorType* type)
{
    unfoldtacpn::Colored::ArcExpression_ptr expr;
    bool first = true;
    for (auto it = element->first_node("hlinscription"); it; it = it->next_sibling("hlinscription")) {
        expr = parseArcExpression(it->first_node("structure"), type);
        if(!first)
        {
            std::cerr << "ERROR: Multiple hlinscription tags in xml" << std::endl;
            exit(ErrorCode);
        }
        first = false;
    }
    return expr;
}

std::vector<Colored::TimeInterval> PNMLParser::parseTimeGuard(rapidxml::xml_node<>* element) {
    auto el = element->first_attribute("inscription");
    std::vector<const Colored::Color*> colors;
    colors.push_back(new Colored::Color());
    std::vector<Colored::TimeInterval> intervals;
    if(el == nullptr) {
        std::string def = "[0,inf)";
        intervals.push_back(Colored::TimeInterval::createFor(def, colors, constantValues));
    }
    else
    {
        auto interval = el->value();
        intervals.push_back(Colored::TimeInterval::createFor(interval, colors, constantValues));
        for (auto it = element->first_node("colorinterval"); it; it = it->next_sibling("colorinterval")) {
            auto pair = parseTimeConstraint(it);
            intervals.push_back(Colored::TimeInterval::createFor(pair.first, pair.second, constantValues));
        }
    }
    return intervals;
}

int PNMLParser::parseWeight(rapidxml::xml_node<>* element) {
    int weight = 1;
    bool first = true;
    auto weightTag = element->first_attribute("weight");
    if(weightTag != nullptr){
        weight = atoi(weightTag->value());
        assert(weight > 0);
    } else {
        for (auto it = element->first_node("inscription"); it; it = it->next_sibling("inscription")) {
            std::string text;
            parseValue(it, text);
            weight = atoi(text.c_str());
            assert(weight > 0);
            if(!first)
            {
                std::cerr << "ERROR: Multiple inscription tags in xml of a arc";
                exit(ErrorCode);
            }
            first = false;
        }
    }
    return weight;
}

void PNMLParser::parseArc(rapidxml::xml_node<>* element, bool inhibitor) {
    std::string source = element->first_attribute("source")->value(),
           target = element->first_attribute("target")->value();
    auto weight = parseWeight(element);
    auto target_is_trans = isTransition(target);
    auto source_is_trans = isTransition(source);
    if(source_is_trans == target_is_trans)
    {
        std::cerr << "ERROR: at least one of '" << source << "' or '" << target << "' of an arc must be a transition" << std::endl;
        std::exit(ErrorCode);
    }


    auto type = _place_types[target_is_trans ? source : target];
    auto expr = parseHLInscriptions(element, type);

    std::vector<Colored::TimeInterval> intervals;
    if(target_is_trans)
    {
        intervals = parseTimeGuard(element);
    }

    if(weight != 0)
    {
        _builder->addArc(source, target, weight, inhibitor, expr, intervals);
    }
    else
    {
        std::cerr << "ERROR: Arc from " << source << " to " << target << " has non-sensible weight 0." << std::endl;
        std::exit(ErrorCode);
    }
}

void PNMLParser::parseSingleTransportArc(rapidxml::xml_node<>* element)
{
    const char* tid = element->first_attribute("transportID")->value();
    if(tid == nullptr)
    {
        std::cerr << "ERROR: Missing transportID on transport-arc." << std::endl;
        exit(ErrorCode);
    }
    std::string source	= element->first_attribute("source")->value();
    std::string target	= element->first_attribute("target")->value();
    auto target_is_trans = isTransition(target);
    std::string trans = target_is_trans ? target : source;
    // technically isTransition only checks if it is not a place. Due to parsing we know that places are defined
    if(!isTransition(trans))
    {
        std::cerr << "ERROR: Could not find transition '" << trans << "' for transport arc" << std::endl;
        std::exit(ErrorCode);
    }
    auto* other = _transportArcs[{trans,tid}];
    if(other == nullptr)
    {
        _transportArcs[{trans,tid}] = element;
    }
    else
    {
        _transportArcs.erase({trans,tid});
        rapidxml::xml_node<>* in = element, *out = other;
        if(target != trans)
        {
            std::swap(in, out);
        }
        auto weight = parseWeight(in);
        auto intervals = parseTimeGuard(in);
        source = in->first_attribute("source")->value();
        target = out->first_attribute("target")->value();
        auto in_expr = parseHLInscriptions(in, _place_types[source]);
        auto out_expr = parseHLInscriptions(out, _place_types[target]);
        _builder->addTransportArc(source, trans, target, weight, in_expr, out_expr, intervals);
    }
}

void PNMLParser::parseTransportArc(rapidxml::xml_node<>* element){
    std::string source	= element->first_attribute("source")->value(),
           transition	= element->first_attribute("transition")->value(),
           target	= element->first_attribute("target")->value();
    auto weight = parseWeight(element);
    auto intervals = parseTimeGuard(element);
    if(weight != 0)
    {
        _builder->addTransportArc(source, transition, target, weight, nullptr, nullptr, intervals);
    }
    else
    {
        std::cerr << "ERROR: Arc from " << source << " to " << target << " has non-sensible weight 0." << std::endl;
        exit(ErrorCode);
    }
}

void PNMLParser::parseTransition(rapidxml::xml_node<>* element) {
    double x = 0, y = 0;
    bool urgent = false;
    int player = 0;
    unfoldtacpn::Colored::GuardExpression_ptr expr = nullptr;
    auto name = element->first_attribute("id")->value();
    Colored::SMC::Distribution distrib = Colored::SMC::Constant;
    Colored::SMC::DistributionParameters distrib_params = { 1.0, 0.0 };
    double weight = 1.0;
    Colored::SMC::FiringMode firingMode = Colored::SMC::Oldest;

    auto posX = element->first_attribute("positionX");
    if (posX != nullptr){
        x = atoll(posX->value());
    }
    auto posY = element->first_attribute("positionY");
    if ( posY != nullptr){
        y = atoll(posY->value());
    }
    auto urg_el = element->first_attribute("urgent");
    if (urg_el != nullptr) {
        urgent = stringToBool(urg_el->value());
    }

    auto pl_el = element->first_attribute("player");
    if(pl_el != nullptr) {
        player = atoi(pl_el->value());
    }

    if(!urgent) {
        auto dist_data = parseDistribution(element);
        distrib = std::get<0>(dist_data);
        distrib_params = std::get<1>(dist_data);
    } else if(urgent) {
        distrib_params = { 0 };
    }

    auto weight_el = element->first_attribute("weight");
    if(weight_el != nullptr) {
        weight = atof(weight_el->value());
    }

    auto firing_el = element->first_attribute("firingMode");
    if(firing_el != nullptr) {
        if (strcasecmp(firing_el->value(), "oldest") == 0) {
            firingMode = Colored::SMC::Oldest;
        } else if (strcasecmp(firing_el->value(), "youngest") == 0) {
            firingMode = Colored::SMC::Youngest;
        } else if (strcasecmp(firing_el->value(), "random") == 0) {
            firingMode = Colored::SMC::Random;
        } 
    }

    for (auto it = element->first_node(); it; it = it->next_sibling()) {
        if (strcmp(it->name(), "graphics") == 0) {
            parsePosition(it, x, y);
        } else if (strcmp(it->name(), "condition") == 0) {
            auto structure = it->first_node("structure");
            expr = parseGuardExpression(structure);
        } else if (strcmp(it->name(), "conditions") == 0) {
            std::cerr << "ERROR: Conditions not supported" << std::endl;
            exit(ErrorCode);
        } else if (strcmp(it->name(), "assignments") == 0) {
            std::cerr << "ERROR: Assignments not supported" << std::endl;
            exit(ErrorCode);
        }
    }
    _builder->addTransition(name, expr, player, urgent, x, y, distrib, distrib_params, weight, firingMode);
}

std::tuple<Colored::SMC::Distribution, Colored::SMC::DistributionParameters> PNMLParser::parseDistribution(rapidxml::xml_node<>* element) {
    Colored::SMC::Distribution distrib = Colored::SMC::Constant;
    Colored::SMC::DistributionParameters distrib_params;
    auto distrib_el = element->first_attribute("distribution");
    if(distrib_el != nullptr) {
        char* distrib_name = distrib_el->value();
        if(strcasecmp(distrib_name, "constant") == 0) {
            distrib = Colored::SMC::Constant;
            distrib_params.push_back(atof(element->first_attribute("value")->value()));
        } else if(strcasecmp(distrib_name, "uniform") == 0) {
            distrib = Colored::SMC::Uniform;
            distrib_params.push_back(atof(element->first_attribute("a")->value()));
            distrib_params.push_back(atof(element->first_attribute("b")->value()));
        } else if(strcasecmp(distrib_name, "exponential") == 0) {
            distrib = Colored::SMC::Exponential;
            distrib_params.push_back(atof(element->first_attribute("rate")->value()));
        } else if(strcasecmp(distrib_name, "normal") == 0) {
            distrib = Colored::SMC::Normal;
            distrib_params.push_back(atof(element->first_attribute("mean")->value()));
            distrib_params.push_back(atof(element->first_attribute("stddev")->value()));
        } else if(strcasecmp(distrib_name, "gamma") == 0) {
            distrib = Colored::SMC::Gamma;
            distrib_params.push_back(atof(element->first_attribute("shape")->value()));
            distrib_params.push_back(atof(element->first_attribute("scale")->value()));
        } else if(strcasecmp(distrib_name, "erlang") == 0) {
            distrib = Colored::SMC::Erlang;
            distrib_params.push_back(atof(element->first_attribute("shape")->value()));
            distrib_params.push_back(atof(element->first_attribute("scale")->value()));
        } else if(strcasecmp(distrib_name, "discrete uniform") == 0) {
            distrib = Colored::SMC::DiscreteUniform;
            distrib_params.push_back(atof(element->first_attribute("a")->value()));
            distrib_params.push_back(atof(element->first_attribute("b")->value()));
        } else if(strcasecmp(distrib_name, "geometric") == 0) {
            distrib = Colored::SMC::Geometric;
            distrib_params.push_back(atof(element->first_attribute("p")->value()));
        } else if(strcasecmp(distrib_name, "triangular") == 0) {
            distrib = Colored::SMC::Triangular;
            distrib_params.push_back(atof(element->first_attribute("a")->value()));
            distrib_params.push_back(atof(element->first_attribute("b")->value()));
            distrib_params.push_back(atof(element->first_attribute("c")->value()));
        } else if(strcasecmp(distrib_name, "log normal") == 0) {
            distrib = Colored::SMC::LogNormal;
            distrib_params.push_back(atof(element->first_attribute("logMean")->value()));
            distrib_params.push_back(atof(element->first_attribute("logStddev")->value()));
        } else if(strcasecmp(distrib_name, "custom") == 0) {
            distrib = Colored::SMC::Custom;
            auto name = element->first_attribute("distributionName")->value();
            if (_customDistributions.count(name)) {
                distrib_params = _customDistributions[name];
            } else {
                std::cerr << "ERROR: Custom distribution '" << name << "' not defined.\n";
                std::exit(ErrorCode);
            }
        }
    }
    return std::make_pair(distrib, distrib_params);
}

void PNMLParser::parseValue(rapidxml::xml_node<>* element, std::string& text) {
    for (auto it = element->first_node(); it; it = it->next_sibling()) {
        if (strcmp(it->name(), "value") == 0 || strcmp(it->name(), "text") == 0) {
            text = it->value();
        } else
            parseValue(it, text);
    }
}

uint32_t PNMLParser::parseNumberConstant(rapidxml::xml_node<>* element) {
    if (strcmp(element->name(), "numberconstant") == 0) {
        auto value = element->first_attribute("value")->value();
        return (uint32_t)atoll(value);
    } else if (strcmp(element->name(), "subterm") == 0) {
        return parseNumberConstant(element->first_node());
    }
    return 0;
}

void PNMLParser::parsePosition(rapidxml::xml_node<>* element, double& x, double& y) {
    for (auto it = element->first_node(); it; it = it->first_node()) {
        if (strcmp(it->name(), "position") == 0) {
            x = atof(it->first_attribute("x")->value());
            y = atof(it->first_attribute("y")->value());
        } else
        {
            parsePosition(it, x, y);
        }
    }
}

bool PNMLParser::isTransition(const std::string& trans)
{
    return (_place_types.count(trans) == 0);
}

void PNMLParser::checkKeyword(const char* id)
{
    auto tmp = id;
    bool all_digit = true;
    while(*tmp!= '\0')
    {
        all_digit &= std::isdigit(*tmp) || std::isspace(*tmp);
        ++tmp;
    }
    if(all_digit)
        return;
    auto res = _used_keywords.insert(id).second;
    if(!res)
    {
        std::cerr << "ERROR: Duplicate use of name " << id << std::endl;
        std::exit(ErrorCode);
    }
}

}
