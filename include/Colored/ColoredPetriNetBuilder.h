/*
 * File:   ColoredPetriNetBuilder.h
 * Author: Klostergaard
 *
 * Created on 17. februar 2018, 16:25
 */

#ifndef COLOREDPETRINETBUILDER_H
#define COLOREDPETRINETBUILDER_H

#include <vector>
#include <unordered_map>
#include <sstream>

#include "ColoredNetStructures.h"
#include "../TAPNBuilderInterface.h"
#include "../types.hpp"

namespace unfoldtacpn {
    class ColoredPetriNetBuilder {
    public:
        using ColorTypeMap = std::unordered_map<std::string, const Colored::ColorType*>;
        using PTPlaceMap = std::unordered_map<std::string, std::unordered_map<uint32_t , std::string>>;
        using PTTransitionMap = std::unordered_map<std::string, std::vector<std::string>>;
        using PlaceColorIdMap = std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>>;
        using VariableBinding = std::pair<std::string, std::string>;
        using TransitionBindings = std::vector<VariableBinding>;
        using PTTransitionBindingsMap = std::unordered_map<std::string, TransitionBindings>;

    public:
        ColoredPetriNetBuilder(std::stringstream *output_stream = nullptr);
        ColoredPetriNetBuilder(const ColoredPetriNetBuilder& orig);
        virtual ~ColoredPetriNetBuilder();
        void parseNet(std::istream& istream);
        void addPlace(const std::string& name,
                      Colored::Multiset&& tokens,
                      const Colored::ColorType* type,
                      const std::vector<Colored::TimeInvariant>& invariant,
                      types::InitialMarkingAges &&initialAges,
                      double x = 0,
                      double y = 0);
        void addTransition(const std::string& name,
                           const Colored::GuardExpression_ptr& guard,
                           int player,
                           bool urgent,
                           double x,
                           double y,
                           Colored::SMC::Distribution distrib = Colored::SMC::Constant,
                           Colored::SMC::DistributionParameters params = { 0.0, 0.0 },
                           double weight = 1.0,
                           Colored::SMC::FiringMode firingMode = Colored::SMC::Oldest);
        void addArc(const std::string& source, const std::string& target,
                    int weight, bool inhibitor, const unfoldtacpn::Colored::ArcExpression_ptr &expr,
                                             const std::vector<unfoldtacpn::Colored::TimeInterval>& intervals);

        void addColorType(const std::string& id,
                const Colored::ColorType* type);

        void addTransportArc(const std::string& source,
                const std::string& transition,
                const std::string& destination,
                int weight,
                const Colored::ArcExpression_ptr& in_expr,
                const Colored::ArcExpression_ptr& out_expr,
                const std::vector<Colored::TimeInterval>& interval);

        double getUnfoldTime() const {
            return _time;
        }

        uint32_t getPlaceCount() const {
            return _places.size();
        }

        uint32_t getTransitionCount() const {
            return _transitions.size();
        }

        uint32_t getArcCount() const {
            uint32_t sum = 0;
            for (auto& t : _transitions) {
                sum += t.arcs.size();
            }
            return sum;
        }

        const PTPlaceMap& getUnfoldedPlaceNames() const {
            return _ptplacenames;
        }

        const PTTransitionMap& getUnfoldedTransitionNames() const {
            return _pttransitionnames;
        }

        const PlaceColorIdMap& getPlaceColorIds() const {
            return _placeColorIds;
        }

        const PTTransitionBindingsMap& getTransitionBindings() const {
            return _transitionBindings;
        }

        void unfold(TAPNBuilderInterface& builder);
        bool resolvePlace(const std::string& placeName, const std::string& colorName, std::string& out) const;
        void clear() { _sumPlacesNames.clear(); _pttransitionnames.clear(); _ptplacenames.clear(); _placeColorIds.clear(); _transitionBindings.clear(); }
    private:
        std::unordered_map<std::string,uint32_t> _placenames;
        std::unordered_map<std::string,uint32_t> _transitionnames;
        PTPlaceMap _ptplacenames;
        PlaceColorIdMap _placeColorIds;
        std::unordered_map<std::string, std::string> _sumPlacesNames;
        PTTransitionMap _pttransitionnames;
        PTTransitionBindingsMap _transitionBindings;
        std::vector< std::tuple<double, double> > _placelocations;
        std::vector< std::tuple<double, double> > _transitionlocations;

        std::vector<Colored::Transition> _transitions;
        std::vector<Colored::Place> _places;
        std::map<uint32_t, std::vector<Colored::Arc>> _inhibitorArcs;
        ColorTypeMap _colors;
        double _time;

        std::stringstream* _output_stream;
        
        std::string arcToString(const Colored::Arc& arc) const;
        const std::string& findSumName(const std::string& id) const;
        const std::string& findSumName(uint32_t id) const { return findSumName(_places[id].name); }
        const std::string& findPlaceName(uint32_t id, const Colored::Color* color) const
        {
            return findPlaceName(_places[id].name, color);
        }
        const std::string& findPlaceName(const std::string& place, const Colored::Color* color) const;
        const Colored::TimeInterval& getTimeIntervalForArc(const std::vector< Colored::TimeInterval>& timeIntervals,const Colored::Color* color) const;
        void unfoldPlace(TAPNBuilderInterface& builder, const Colored::Place& place);
        const Colored::TimeInvariant& getTimeInvariantForPlace(const std::vector< Colored::TimeInvariant>& TimeInvariants, const Colored::Color* color) const;
        void unfoldTransition(TAPNBuilderInterface& builder, const Colored::Transition& transition);
        void unfoldArc(TAPNBuilderInterface& builder, const Colored::Arc& arc, const Colored::ExpressionContext::BindingMap& binding, const std::string& name);
        void unfoldTransport(TAPNBuilderInterface& builder, const Colored::TransportArc& arc, const Colored::ExpressionContext::BindingMap& binding, const std::string& name);
        void unfoldInhibitorArc(TAPNBuilderInterface& builder, uint32_t transition, const std::string &newname);
    };

    class BindingGenerator {
    public:
        class Iterator {
        private:
            BindingGenerator* _generator;

        public:
            Iterator(BindingGenerator* generator);

            bool operator==(Iterator& other);
            bool operator!=(Iterator& other);
            Iterator& operator++();
            const Colored::ExpressionContext::BindingMap operator++(int);
            Colored::ExpressionContext::BindingMap& operator*();
        };
    private:
        Colored::GuardExpression_ptr _expr;
        Colored::ExpressionContext::BindingMap _bindings;
        const ColoredPetriNetBuilder::ColorTypeMap& _colorTypes;
        bool _empty = false;

        bool eval();
        Colored::ExpressionContext::BindingMap& nextBinding();
        Colored::ExpressionContext::BindingMap& currentBinding();
    public:
        BindingGenerator(const Colored::Transition& transition,
                const ColoredPetriNetBuilder::ColorTypeMap& colorTypes);
        bool isInitial() const;
        Iterator begin();
        Iterator end();
    };
}

#endif /* COLOREDPETRINETBUILDER_H */
