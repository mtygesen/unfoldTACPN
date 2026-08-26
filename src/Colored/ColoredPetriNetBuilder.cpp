/*
 * File:   ColoredPetriNetBuilder.cpp
 * Author: Klostergaard
 *
 * Created on 17. februar 2018, 16:25
 */

#include <chrono>
#include <tuple>
#include <sstream>

#include "Colored/ColoredPetriNetBuilder.h"
#include "PetriParse/PNMLParser.h"
#include "errorcodes.h"

namespace unfoldtacpn {
    ColoredPetriNetBuilder::ColoredPetriNetBuilder(std::stringstream *output_stream):
    _output_stream(output_stream)
    {
    
    }

    ColoredPetriNetBuilder::ColoredPetriNetBuilder(const ColoredPetriNetBuilder& orig)
    : _placenames(orig._placenames), _transitionnames(orig._transitionnames),
       _placelocations(orig._placelocations), _transitionlocations(orig._transitionlocations),
       _transitions(orig._transitions), _places(orig._places)
    {

    }

    ColoredPetriNetBuilder::~ColoredPetriNetBuilder() {
    }

    void ColoredPetriNetBuilder::parseNet(std::istream& stream) {
        PNMLParser parser;
        parser.parse(stream, this);
    }

    void ColoredPetriNetBuilder::addPlace(const std::string &name,
                                          unfoldtacpn::Colored::Multiset &&tokens,
                                          const Colored::ColorType* type,
                                          const std::vector<unfoldtacpn::Colored::TimeInvariant> &invariant, 
                                          types::InitialMarkingAges &&initialAges,
                                          double x,
                                          double y) {
        if(_placenames.count(name) == 0){
            uint32_t next = _placenames.size();
            if(type == nullptr)
                type = _colors["dot"];
            _places.emplace_back(Colored::Place{name, std::move(initialAges), type, std::move(tokens), invariant});
            _placenames[name] = next;
            _placelocations.push_back(std::tuple<double, double>(x,y));
        }
        else
        {
            std::cerr << "ERROR: Adding place " << name << " twice!" << std::endl;
            std::exit(ErrorCode);
        }
    }

    void ColoredPetriNetBuilder::addTransition(const std::string& name,
            const Colored::GuardExpression_ptr& guard,
            int player,
            bool urgent,
            double x,
            double y,
            Colored::SMC::Distribution distrib,
            Colored::SMC::DistributionParameters params,
            double weight,
            Colored::SMC::FiringMode firingMode) {
        if (_transitionnames.count(name) == 0) {
            uint32_t next = _transitionnames.size();
            _transitions.emplace_back(Colored::Transition {name, guard, player, urgent, distrib, params, weight, firingMode});
            _transitionnames[name] = next;
            _transitionlocations.push_back(std::tuple<double, double>(x,y));
        }
    }

    void ColoredPetriNetBuilder::addArc(const std::string& source, const std::string& target,
                    int weight, bool inhibitor, const unfoldtacpn::Colored::ArcExpression_ptr &expr,
                                             const std::vector<unfoldtacpn::Colored::TimeInterval>& intervals) {
        auto& transition = _transitionnames.count(source) == 0 ? target : source;
        auto& place = _transitionnames.count(source) == 0 ? source : target;

        if(_transitionnames.count(transition) == 0)
        {
            std::cout << "Transition '" << transition << "' not found." << std::endl;
            std::exit(ErrorCode);
        }
        if(_placenames.count(place) == 0)
        {
            std::cout << "Place '" << place << "' not found." << std::endl;
            std::exit(ErrorCode);
        }

        if(weight <= 0)
        {
            std::cout << "Arc between " << source << " and " << target << " is 0" << std::endl;
            std::exit(ErrorCode);
        }

        if(inhibitor && &place != &source)
        {
            std::cout << "Source of inhibitor must be a place, but is " << source << std::endl;
            std::exit(ErrorCode);
        }

        // if we have an inhibitor, call intervals/guards must be zero-infinity.
        if(inhibitor && std::any_of(std::begin(intervals), std::end(intervals), [](auto& i){ return !i.isZeroInfinity();}))
        {
            std::cout << "Source inhibitors cannot have guards, between " << source << " and " << target << std::endl;
            std::exit(ErrorCode);
        }

        uint32_t p = _placenames[place];
        uint32_t t = _transitionnames[transition];

        assert(t < _transitions.size());
        assert(p < _places.size());

        Colored::Arc arc;
        arc.place = p;
        arc.transition = t;
        arc.expr = expr;
        if(arc.expr == nullptr)
        {
            std::vector<Colored::ColorExpression_ptr> colors{std::make_shared<Colored::DotConstantExpression>()};
            arc.expr = std::make_shared<Colored::NumberOfExpression>(
                                                std::move(colors), weight);
        }
        arc.input = (&source) == (&place);
        arc.weight = weight;
        arc.interval = intervals;
        arc.inhibitor = inhibitor;
        if(inhibitor){
            _inhibitorArcs[arc.transition].emplace_back(std::move(arc));
            _places[p].inhibiting = true;
        } else {
            _transitions[t].arcs.emplace_back(std::move(arc));
        }
    }

    void ColoredPetriNetBuilder::addTransportArc(const std::string &source, const std::string &transition,
                                                 const std::string &destination, int weight,
                                                 const unfoldtacpn::Colored::ArcExpression_ptr &in_expr,
                                                 const unfoldtacpn::Colored::ArcExpression_ptr &out_expr,
                                                 const std::vector<unfoldtacpn::Colored::TimeInterval> &interval) {
        if(_transitionnames.count(transition) == 0)
        {
            std::cout << "Transition '" << transition << "' not found." << std::endl;
            std::exit(ErrorCode);
        }
        if(_placenames.count(source) == 0)
        {
            std::cout << "Place '" << source << "' not found." << std::endl;
            std::exit(ErrorCode);
        }
        if(_placenames.count(destination) == 0)
        {
            std::cout << "Place '" << destination << "' not found." << std::endl;
            std::exit(ErrorCode);
        }

        uint32_t s = _placenames[source];
        uint32_t t = _transitionnames[transition];
        uint32_t d = _placenames[destination];

        assert(t < _transitions.size());
        assert(s < _places.size());

        Colored::TransportArc transportArc;
        transportArc.source = s;
        transportArc.transition = t;
        transportArc.destination = d;
        transportArc.in_expr = in_expr;
        transportArc.out_expr = out_expr;
        transportArc.interval = interval;
        transportArc.weight = weight;

        if(transportArc.in_expr == nullptr)
        {
            std::vector<Colored::ColorExpression_ptr> colors{std::make_shared<Colored::DotConstantExpression>()};
            transportArc.in_expr = std::make_shared<Colored::NumberOfExpression>(
                                                std::move(colors), weight);
        }
        if(transportArc.out_expr == nullptr)
        {
            std::vector<Colored::ColorExpression_ptr> colors{std::make_shared<Colored::DotConstantExpression>()};
            transportArc.out_expr = std::make_shared<Colored::NumberOfExpression>(
                                                std::move(colors), weight);
        }
        _transitions[t].transport.emplace_back(std::move(transportArc));
    }

    void ColoredPetriNetBuilder::addColorType(const std::string& id, const Colored::ColorType* type) {
        _colors[id] = type;
    }

    void ColoredPetriNetBuilder::unfold(TAPNBuilderInterface& builder) {
        clear();
        auto start = std::chrono::high_resolution_clock::now();
        for (auto& place : _places) {
            unfoldPlace(builder, place);
        }

        if (_output_stream) {
            (*_output_stream) << "\nBINDINGS FOR EACH UNFOLDED TRANSITION\n";
            (*_output_stream) << "<bindings>\n";
        }

        for (auto& transition : _transitions) {
            unfoldTransition(builder, transition);
        }

        if (_output_stream) {
            (*_output_stream) << "</bindings>\n";
        }

        auto end = std::chrono::high_resolution_clock::now();
        _time = (std::chrono::duration_cast<std::chrono::microseconds>(end - start).count())*0.000001;
    }

    void ColoredPetriNetBuilder::unfoldPlace(TAPNBuilderInterface& builder, const Colored::Place& place) {
        uint32_t index = _placenames[place.name];
        auto placePos = _placelocations[index];
        size_t size = place.type == nullptr ? 1 : place.type->size();
        if(size != 1)
        {
            double offset = 0;
            for (size_t i = 0; i < place.type->size(); ++i) {
                double x = std::get<0>(placePos);
                double y = std::get<1>(placePos);
                std::string name = place.name + "__" + std::to_string(i);
                const Colored::Color* color = &place.type->operator[](i);
                Colored::TimeInvariant invariant = getTimeInvariantForPlace(place.invariants, color); //TODO:: this does not take the correct time invariant
                auto r = place.marking[color];
                auto ageIt = place.initialMarkingAges.find(color->getId());
                auto nonZeroTokenAges = (ageIt != place.initialMarkingAges.end()) ? ageIt->second : types::InitialTokenAges{};
                assert(nonZeroTokenAges.size() <= r && "There are more non-zero token ages than tokens in the marking");

                builder.addPlace(name, r, invariant.isBoundStrict(), invariant.getBound(), std::move(nonZeroTokenAges), x, y + offset);

                _ptplacenames[place.name][color->getId()] = std::move(name);
                _placeColorIds[place.name][color->toString()] = color->getId();
                offset += 15;
            }

            if(place.inhibiting)
            {
                double x = std::get<0>(placePos);
                double y = std::get<1>(placePos);
                std::string placeName = "__" + place.name + "__SUM";

                types::InitialTokenAges nonZeroTokenAges;
                nonZeroTokenAges.reserve(place.initialMarkingAges.size());
                for (const auto &[_, ages] : place.initialMarkingAges) {
                    nonZeroTokenAges.insert(nonZeroTokenAges.end(), ages.begin(), ages.end());
                }

                builder.addPlace(placeName, place.marking.size(), true, std::numeric_limits<int>::max(), std::move(nonZeroTokenAges), x + 30, y - 30);
                _sumPlacesNames[place.name] = std::move(placeName);
            }
        }
        else
        {
            _ptplacenames[place.name][0] = place.name;
            const unfoldtacpn::Colored::Color* color = &(*place.type)[0];
            _placeColorIds[place.name][color->toString()] = color->getId();
            auto ageIt = place.initialMarkingAges.find(color->getId());
            auto nonZeroTokenAges = (ageIt != place.initialMarkingAges.end()) ? ageIt->second : types::InitialTokenAges{};
            assert(nonZeroTokenAges.size() <= place.marking[color] && "There are more non-zero token ages than tokens in the marking");
            Colored::TimeInvariant invariant = getTimeInvariantForPlace(place.invariants, color);
            builder.addPlace(place.name, place.marking.size(), invariant.isBoundStrict(), invariant.getBound(), std::move(nonZeroTokenAges),
                std::get<0>(placePos), std::get<1>(placePos));
        }
    }

    const Colored::TimeInvariant& ColoredPetriNetBuilder::getTimeInvariantForPlace(const std::vector< Colored::TimeInvariant>& time_invariants, const Colored::Color* color) const {
        if(color != nullptr)
        {
            for (const Colored::TimeInvariant& element : time_invariants) {
                if(element.getColor().getColorType() == Colored::StarColorType::starColorType())
                {
                    continue;
                }
                if (!element.getColor().isTuple()) {
                    if (element.getColor().getId() == color->getId()) {
                        return element;
                    }
                } else {
                    bool matches = true;
                    Colored::Color color = element.getColor();
                    const auto& colors = color.getTuple();
                    if (colors.size() == color.getTuple().size()) {
                        for(uint32_t i = 0;i < colors.size(); i++) {
                            if (colors[i]->getId() != color.getTuple()[i]->getId())
                                matches = false;
                        }
                        if (matches)
                        {
                            return element;
                        }
                    }
                }
            }
        }
        for (uint32_t j = 0; j < time_invariants.size(); ++j) {
            if (time_invariants[j].getColor().getColorType() == Colored::StarColorType::starColorType()) {
                return time_invariants[j];
            }
        }
        exit(ErrorCode);
    }

    void ColoredPetriNetBuilder::unfoldTransition(TAPNBuilderInterface& builder, const Colored::Transition& transition) {
        BindingGenerator gen(transition, _colors);
        double offset = 0;
        uint32_t transitionId = _transitionnames[transition.name];
        auto transitionPos = _transitionlocations[transitionId];
        size_t i = 0;
        for (auto& b : gen) {
           
            std::string name = transition.name;
            if(!gen.isInitial())
                name += "__" + std::to_string(i++);

            TransitionBindings bindings;
            for (const auto& var : b) {
                bindings.emplace_back(var.first, var.second->getColorName());
            }
            
            _transitionBindings[name] = std::move(bindings);

            // Print bindings for each transition if output stream exists
            if (_output_stream) {     
                (*_output_stream) << "   <transition id=\"" << name << "\">\n";    
                for(auto const &var: b) {
                    (*_output_stream) << "      <variable id=\"" << var.first << "\">\n";
                    (*_output_stream) << "         <color>" << var.second->getColorName() << "</color>\n";
                    (*_output_stream) << "      </variable>\n";
                }
                (*_output_stream) << "   </transition>\n";
            }

            builder.addTransition(name, transition.player, transition.urgent, std::get<0>(transitionPos), std::get<1>(transitionPos) + offset,
                transition.distribution, transition.distributionParams, transition.distributionParams.customDistributionRandomStart, transition.weight, transition.firingMode);
            _pttransitionnames[transition.name].push_back(name);
            for (auto& arc : transition.arcs) {
                unfoldArc(builder, arc, b, name);
            }
            for (auto& transport : transition.transport)
            {
                unfoldTransport(builder, transport, b, name);
            }
            unfoldInhibitorArc(builder, transitionId, name);
            offset += 15;
        }
    }

    void ColoredPetriNetBuilder::unfoldInhibitorArc(TAPNBuilderInterface& builder, uint32_t transition, const std::string &newname) {
        for (auto& inhibitor : _inhibitorArcs[transition]) {
            auto& place = findSumName(inhibitor.place);
            if(place.size() != 0)
                builder.addInputArc(place, newname, true, inhibitor.weight, false, true, 0, std::numeric_limits<int>::max());
            else
                builder.addInputArc(_places[inhibitor.place].name, newname, true, inhibitor.weight, false, true, 0, std::numeric_limits<int>::max());
        }
    }

    void ColoredPetriNetBuilder::unfoldTransport(TAPNBuilderInterface& builder, const Colored::TransportArc& arc, const Colored::ExpressionContext::BindingMap& binding, const std::string& tName) {
        Colored::ExpressionContext context {binding, _colors};
        auto in_ms = arc.in_expr->eval(context);
        auto out_ms = arc.out_expr->eval(context);
        const auto& in_color = *in_ms.begin();
        const auto& out_color = *out_ms.begin();
        for(auto oc : in_ms)
        {
            if(*oc.first != *in_color.first)
            {
                std::cerr << "ERROR: Ill-formed transport-arc color" << std::endl;
                std::exit(ErrorCode);
            }
        }
        for(auto oc : out_ms)
        {
            if(*oc.first != *out_color.first)
            {
                std::cerr << "ERROR: Ill-formed transport-arc color" << std::endl;
                std::exit(ErrorCode);
            }
        }

        const std::string& inName = findPlaceName(arc.source, in_color.first);
        const std::string& outName = findPlaceName(arc.destination, out_color.first);
        {
            // add actual transport
            auto& timeInterval = getTimeIntervalForArc(arc.interval, in_color.first);
            builder.addTransportArc(inName, tName, outName, in_color.second,
                    timeInterval.isLowerBoundStrict(), timeInterval.isUpperBoundStrict(),
                    timeInterval.getLowerBound(), timeInterval.getUpperBound());
        }
        {
            // add sum
            auto& in_sum = findSumName(arc.source);
            if(in_sum.size() != 0)
            {
                Colored::Color color;
                Colored::TimeInterval timeInterval(color);
                builder.addInputArc(in_sum, tName, false, in_color.second,
                    timeInterval.isLowerBoundStrict(), timeInterval.isUpperBoundStrict(),
                    timeInterval.getLowerBound(), timeInterval.getUpperBound());
            }
            auto& out_sum = findSumName(arc.destination);
            if(out_sum.size() > 0)
                builder.addOutputArc(out_sum, findSumName(arc.destination), out_color.second);
        }
    }

    const std::string empty_sum;
    const std::string& ColoredPetriNetBuilder::findSumName(const std::string& id) const
    {
        auto it = _sumPlacesNames.find(id);
        if(it == std::end(_sumPlacesNames))
            return empty_sum;
        else
            return it->second;
    }

    const std::string& ColoredPetriNetBuilder::findPlaceName(const std::string& place, const Colored::Color* color) const
    {
        auto it = _ptplacenames.find(place);
        if(it == std::end(_ptplacenames))
            return place;
        else
        {
            auto pit = it->second.find(color->getId());
            if(pit == std::end(it->second))
            {
                std::cerr << "ERROR: No match on id of color for place " << place << std::endl;
                std::exit(ErrorCode);
            }
            return pit->second;
        }
    }

    bool ColoredPetriNetBuilder::resolvePlace(const std::string& placeName, const std::string& colorName, std::string& out) const {
        auto colorPlaceIt = _placeColorIds.find(placeName);
        if (colorPlaceIt == _placeColorIds.end()) return false;

        auto colorIt = colorPlaceIt->second.find(colorName);
        if (colorIt == colorPlaceIt->second.end()) return false;

        auto placeIt = _ptplacenames.find(placeName);
        if (placeIt == _ptplacenames.end()) return false;

        auto unfoldedIt = placeIt->second.find(colorIt->second);
        if (unfoldedIt == placeIt->second.end()) return false;

        out = unfoldedIt->second;
        return true;
    }

    void ColoredPetriNetBuilder::unfoldArc(TAPNBuilderInterface& builder, const Colored::Arc& arc, const Colored::ExpressionContext::BindingMap& binding, const std::string& tName) {
        Colored::ExpressionContext context {binding, _colors};
        auto ms = arc.expr->eval(context);
        int sumWeight = 0;
        auto& timeIntervals = arc.interval;
        bool is_singular = true;
        for (const auto& color : ms) {
            if (color.second == 0) {
                continue;
            }
            sumWeight += color.second;
            is_singular &= color.first->getColorType()->size() == 1;
            auto& pName = findPlaceName(arc.place, color.first);
            if (!arc.input) {
                builder.addOutputArc(tName, pName, color.second);
            } else {
                auto& timeInterval = getTimeIntervalForArc(timeIntervals, color.first);
                builder.addInputArc(pName, tName, arc.inhibitor, color.second,
                    timeInterval.isLowerBoundStrict(), timeInterval.isUpperBoundStrict(),
                    timeInterval.getLowerBound(), timeInterval.getUpperBound());
            }
        }
        // we only add sum-places if we have a non-singleton color and we are modifying an inhibiting place
        if(sumWeight > 0 && !is_singular && _places[arc.place].inhibiting) {
            const std::string& pName = findSumName(arc.place);
            if(pName.size() > 0)
            {
                if (!arc.input) {
                    builder.addOutputArc(tName, pName, sumWeight);
                } else {
                    Colored::Color color;
                    Colored::TimeInterval timeInterval(color);
                    builder.addInputArc(pName, tName, arc.inhibitor, sumWeight,
                    timeInterval.isLowerBoundStrict(), timeInterval.isUpperBoundStrict(),
                    timeInterval.getLowerBound(), timeInterval.getUpperBound());
                }
            }
        }
    }

    const Colored::TimeInterval& ColoredPetriNetBuilder::getTimeIntervalForArc(const std::vector< Colored::TimeInterval>& timeIntervals,
        const Colored::Color* color) const {
        for (const auto& element : timeIntervals) {
            if(element.getColor().getColorType() == Colored::StarColorType::starColorType())
                continue;
            if (!element.getColor().isTuple()) {
                if (element.getColor().getId() == color->getId()) {
                    return element;
                }
            } else {
                bool matches = true;
                std::vector<const Colored::Color*> colors = element.getColor().getTuple();

                if (colors.size() == color->getTuple().size()) {
                    for(uint32_t i = 0; i < colors.size(); i++) {
                        if (colors[i]->getId() != color->getTuple()[i]->getId())
                            matches = false;
                    }
                    if (matches)
                        return element;
                }
            }
        }
        for (uint32_t j = 0; j < timeIntervals.size(); ++j) { // If there is no timeinterval for the specific color, we use the default * interval
            if (timeIntervals[j].getColor().getColorType() == Colored::StarColorType::starColorType()) {
                return timeIntervals[j];
            }
        }
        std::cerr << "There is no matching time interval to an arc" << std::endl;;
        exit(ErrorCode);
    }

    std::string ColoredPetriNetBuilder::arcToString(const Colored::Arc& arc) const {
        return !arc.input ? "(" + _transitions[arc.transition].name + ", " + _places[arc.place].name + ")" :
               "(" + _places[arc.place].name + ", " + _transitions[arc.transition].name + ")";
    }
  
    BindingGenerator::Iterator::Iterator(BindingGenerator* generator)
            : _generator(generator)
    {
    }

    bool BindingGenerator::Iterator::operator==(Iterator& other) {
        return _generator == other._generator;
    }

    bool BindingGenerator::Iterator::operator!=(Iterator& other) {
        return _generator != other._generator;
    }

    BindingGenerator::Iterator& BindingGenerator::Iterator::operator++() {
        _generator->nextBinding();
        if (_generator->isInitial()) _generator = nullptr;
        return *this;
    }

    const Colored::ExpressionContext::BindingMap BindingGenerator::Iterator::operator++(int) {
        auto prev = _generator->currentBinding();
        ++*this;
        return prev;
    }

    Colored::ExpressionContext::BindingMap& BindingGenerator::Iterator::operator*() {
        return _generator->currentBinding();
    }

    BindingGenerator::BindingGenerator(const Colored::Transition& transition,
            const ColoredPetriNetBuilder::ColorTypeMap& colorTypes)
        : _colorTypes(colorTypes)
    {
        _expr = transition.guard;
        std::set<const Colored::Variable*> variables;
        if (_expr != nullptr) {
            _expr->getVariables(variables);
        }
        for (auto& arc : transition.arcs) {
            assert(arc.expr != nullptr);
            arc.expr->getVariables(variables);
        }
        for (auto& arc : transition.transport) {
            assert(arc.in_expr != nullptr);
            assert(arc.out_expr != nullptr);
            arc.in_expr->getVariables(variables);
            arc.out_expr->getVariables(variables);
        }
        for (auto& var : variables) {
            _bindings[var->name] = &var->colorType->operator[](0);
        }

        if (!eval())
            nextBinding();
        _empty = !eval();
    }

    bool BindingGenerator::eval() {
        if (_expr == nullptr)
            return true;

        Colored::ExpressionContext context {_bindings, _colorTypes};
        return _expr->eval(context);
    }

    Colored::ExpressionContext::BindingMap& BindingGenerator::nextBinding() {
        bool test = false;
        while (!test) {
            for (auto& _binding : _bindings) {
                _binding.second = &_binding.second->operator++();
                if (_binding.second->getId() != 0) {
                    break;
                }
            }

            if (isInitial())
                break;

            test = eval();
        }
        return _bindings;
    }

    Colored::ExpressionContext::BindingMap& BindingGenerator::currentBinding() {
        return _bindings;
    }

    bool BindingGenerator::isInitial() const {
        for (auto& b : _bindings) {
            if (b.second->getId() != 0) return false;
        }
        return true;
    }

    BindingGenerator::Iterator BindingGenerator::begin() {
        if(_empty)
            return {nullptr};
        return {this};
    }

    BindingGenerator::Iterator BindingGenerator::end() {
        return {nullptr};
    }
}
