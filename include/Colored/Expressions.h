/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   Expressions.h
 * Author: andreas
 *
 * Created on February 19, 2018, 7:00 PM
 */

#ifndef COLORED_EXPRESSIONS_H
#define COLORED_EXPRESSIONS_H

#include <string>
#include <unordered_map>
#include <set>
#include <stdlib.h>
#include <iostream>
#include <cassert>
#include <memory>
#include <stdexcept>


#include "Colors.h"
#include "Multiset.h"

namespace unfoldtacpn {
    class ColoredPetriNetBuilder;

    namespace Colored {
        struct ExpressionContext {
            typedef std::unordered_map<std::string, const Color*> BindingMap;
            typedef std::unordered_map<std::string, const ColorType*> TypeMap;
            const BindingMap& binding;
            const TypeMap& colorTypes;

            const Color* findColor(const std::string& color) const;

            const ProductType* findProductColorType(const std::vector<const ColorType*>& types) const;
        };

        class WeightException : public std::exception {
        private:
            std::string _message;
        public:
            explicit WeightException(std::string message) : _message(message) {}

            const char* what() const noexcept override {
                return ("Undefined weight: " + _message).c_str();
            }
        };

        class Expression {
        public:
            Expression() {}

            virtual void getVariables(std::set<const Variable*>& variables) const {
            }

            virtual void expressionType() {
                std::cout << "Expression" << std::endl;
            }

            virtual std::string toString() const {
                return "Unsupported";
            }
        };

        class ColorExpression : public Expression {
        public:
            ColorExpression() {}
            virtual ~ColorExpression() {}

            virtual const Color* eval(ExpressionContext& context) const = 0;

            virtual const ColorType* getColorType() const = 0;

            virtual void getConstants(std::unordered_map<uint32_t, const Color*> &constantMap, uint32_t &index) const = 0;
        };

        class DotConstantExpression : public ColorExpression {
        public:
            const Color* eval(ExpressionContext& context) const override {
                return Color::dotConstant();
            }

            const ColorType* getColorType() const override {
                return Color::dotConstant()->getColorType();
            }

            void getConstants(std::unordered_map<uint32_t, const Color*> &constantMap, uint32_t &index) const override {
                const Color *dotColor = Color::dotConstant();
                constantMap[index] = dotColor;
            }
        };

        typedef std::shared_ptr<ColorExpression> ColorExpression_ptr;

        class VariableExpression : public ColorExpression {
        private:
            const Variable* _variable;

        public:
            const Color* eval(ExpressionContext& context) const override {
                auto it = context.binding.find(_variable->name);
                if(it == std::end(context.binding))
                {
                    std::cerr << "ERROR: Could not find varible " << _variable->name << std::endl;
                }
                return it->second;
            }

            void getVariables(std::set<const Variable*>& variables) const override {
                variables.insert(_variable);
            }

            const ColorType* getColorType() const override{
                return _variable->colorType;
            }

            std::string toString() const override {
                return _variable->name;
            }

            void getConstants(std::unordered_map<uint32_t, const Color*> &constantMap, uint32_t &index) const override {
            }

            VariableExpression(const Variable* variable)
                    : _variable(variable) {}
        };

        class UserOperatorExpression : public ColorExpression {
        private:
            const Color* _userOperator;

        public:
            const Color* eval(ExpressionContext& context) const override {
                return _userOperator;
            }

            std::string toString() const override {
                return _userOperator->toString();
            }

            void getConstants(std::unordered_map<uint32_t, const Color*> &constantMap, uint32_t &index) const override {
                constantMap[index] = _userOperator;
            }

            const ColorType* getColorType() const override{
                return _userOperator->getColorType();
            }

            UserOperatorExpression(const Color* userOperator)
                    : _userOperator(userOperator) {}
        };

        class UserSortExpression : public Expression {
        private:
            ColorType* _userSort;

        public:
            ColorType* eval(ExpressionContext& context) const {
                return _userSort;
            }

            std::string toString() const override {
                return _userSort->getName();
            }

            UserSortExpression(ColorType* userSort)
                    : _userSort(userSort) {}
        };

        typedef std::shared_ptr<UserSortExpression> UserSortExpression_ptr;

        class NumberConstantExpression : public Expression {
        private:
            uint32_t _number;

        public:
            uint32_t eval(ExpressionContext& context) const {
                return _number;
            }

            NumberConstantExpression(uint32_t number)
                    : _number(number) {}
        };

        typedef std::shared_ptr<NumberConstantExpression> NumberConstantExpression_ptr;

        class SuccessorExpression : public ColorExpression {
        private:
            ColorExpression_ptr _color;

        public:
            const Color* eval(ExpressionContext& context) const override {
                return &++(*_color->eval(context));
            }

            void getVariables(std::set<const Variable*>& variables) const override {
                _color->getVariables(variables);
            }

            std::string toString() const override {
                return _color->toString() + "++";
            }

            const ColorType* getColorType() const override {
                return _color->getColorType();
            }

            void getConstants(std::unordered_map<uint32_t, const Color*> &constantMap, uint32_t &index) const override {
                _color->getConstants(constantMap, index);
                for(auto& constIndexPair : constantMap){
                    constIndexPair.second = &constIndexPair.second->operator++();
                }
            }

            SuccessorExpression(ColorExpression_ptr&& color)
                    : _color(std::move(color)) {}
        };

        class PredecessorExpression : public ColorExpression {
        private:
            ColorExpression_ptr _color;

        public:
            const Color* eval(ExpressionContext& context) const override {
                return &--(*_color->eval(context));
            }

            void getVariables(std::set<const Variable*>& variables) const override {
                _color->getVariables(variables);
            }

            std::string toString() const override {
                return _color->toString() + "--";
            }

            const ColorType* getColorType() const override{
                return _color->getColorType();
            }

            void getConstants(std::unordered_map<uint32_t, const Color*> &constantMap, uint32_t &index) const override {
                _color->getConstants(constantMap, index);
                for(auto& constIndexPair : constantMap){
                    constIndexPair.second = &constIndexPair.second->operator--();
                }
            }

            PredecessorExpression(ColorExpression_ptr&& color)
                    : _color(std::move(color)) {}
        };

        class TupleExpression : public ColorExpression {
        private:
            std::vector<ColorExpression_ptr> _colors;
            const ColorType* _colorType;

        public:
            const Color* eval(ExpressionContext& context) const override {
                std::vector<const Color*> colors;
                std::vector<const ColorType*> types;
                for (auto& color : _colors) {
                    colors.push_back(color->eval(context));
                    types.push_back(colors.back()->getColorType());
                }

                const ProductType* pt = context.findProductColorType(types);
                if(pt == nullptr)
                    throw "Could not match color types during parsing";
                const Color* col = pt->getColor(colors);
                assert(col != nullptr);
                return col;
            }

            const ColorType* getColorType() const override {
                return _colorType;
            }

            void getConstants(std::unordered_map<uint32_t, const Color*> &constantMap, uint32_t &index) const override {
                for (auto& elem : _colors) {
                    elem->getConstants(constantMap, index);
                    index++;
                }
            }

            void getVariables(std::set<const Variable*>& variables) const override {
                for (auto& elem : _colors) {
                    elem->getVariables(variables);
                }
            }

            std::string toString() const override {
                std::string res = "(" + _colors[0]->toString();
                for (uint32_t i = 1; i < _colors.size(); ++i) {
                    res += "," + _colors[i]->toString();
                }
                res += ")";
                return res;
            }

            TupleExpression(std::vector<ColorExpression_ptr>&& colors, const ColorType* type)
                    : _colors(std::move(colors)), _colorType(type) {
                assert(dynamic_cast<const ProductType*>(_colorType) || type == nullptr);
            }
        };

        class GuardExpression : public Expression {
        private:
            const ColorType* _colorType = nullptr;

        public:
            GuardExpression() {}
            virtual ~GuardExpression() {}

            virtual bool eval(ExpressionContext& context) const = 0;

            void validateAndInferColorType() {
                std::set<const Variable*> variables;
                getVariables(variables);
                if (variables.empty()) {
                    throw std::invalid_argument("There must be at least one variable in the guard expression.");
                }

                _colorType = (*variables.begin())->colorType;
                for (const auto* variable : variables) {
                    if (!(*variable->colorType == *_colorType)) {
                        throw std::invalid_argument("All variables in a guard expression must have the same color type.");
                    }
                }
            }

            const ColorType* getColorType() const {
                return _colorType;
            }
        };

        typedef std::shared_ptr<GuardExpression> GuardExpression_ptr;

        class LessThanExpression : public GuardExpression {
        private:
            ColorExpression_ptr _left;
            ColorExpression_ptr _right;

        public:
            bool eval(ExpressionContext& context) const override {
                return _left->eval(context) < _right->eval(context);
            }

            void getVariables(std::set<const Variable*>& variables) const override {
                _left->getVariables(variables);
                _right->getVariables(variables);
            }

            LessThanExpression(ColorExpression_ptr&& left, ColorExpression_ptr&& right)
                    : _left(std::move(left)), _right(std::move(right)) {}
        };

        class GreaterThanExpression : public GuardExpression {
        private:
            ColorExpression_ptr _left;
            ColorExpression_ptr _right;

        public:
            bool eval(ExpressionContext& context) const override {
                return _left->eval(context) > _right->eval(context);
            }

            void getVariables(std::set<const Variable*>& variables) const override {
                _left->getVariables(variables);
                _right->getVariables(variables);
            }

            GreaterThanExpression(ColorExpression_ptr&& left, ColorExpression_ptr&& right)
                    : _left(std::move(left)), _right(std::move(right)) {}
        };

        class LessThanEqExpression : public GuardExpression {
        private:
            ColorExpression_ptr _left;
            ColorExpression_ptr _right;

        public:
            bool eval(ExpressionContext& context) const override {
                return _left->eval(context) <= _right->eval(context);
            }

            void getVariables(std::set<const Variable*>& variables) const override {
                _left->getVariables(variables);
                _right->getVariables(variables);
            }

            LessThanEqExpression(ColorExpression_ptr&& left, ColorExpression_ptr&& right)
                    : _left(std::move(left)), _right(std::move(right)) {}
        };

        class GreaterThanEqExpression : public GuardExpression {
        private:
            ColorExpression_ptr _left;
            ColorExpression_ptr _right;

        public:
            bool eval(ExpressionContext& context) const override {
                return _left->eval(context) >= _right->eval(context);
            }

            void getVariables(std::set<const Variable*>& variables) const override {
                _left->getVariables(variables);
                _right->getVariables(variables);
            }

            GreaterThanEqExpression(ColorExpression_ptr&& left, ColorExpression_ptr&& right)
                    : _left(std::move(left)), _right(std::move(right)) {}
        };

        class EqualityExpression : public GuardExpression {
        private:
            ColorExpression_ptr _left;
            ColorExpression_ptr _right;

        public:
            bool eval(ExpressionContext& context) const override {
                return _left->eval(context) == _right->eval(context);
            }

            void getVariables(std::set<const Variable*>& variables) const override {
                _left->getVariables(variables);
                _right->getVariables(variables);
            }

            EqualityExpression(ColorExpression_ptr&& left, ColorExpression_ptr&& right)
                    : _left(std::move(left)), _right(std::move(right)) {}
        };

        class InequalityExpression : public GuardExpression {
        private:
            ColorExpression_ptr _left;
            ColorExpression_ptr _right;

        public:
            bool eval(ExpressionContext& context) const override {
                return _left->eval(context) != _right->eval(context);
            }

            void getVariables(std::set<const Variable*>& variables) const override {
                _left->getVariables(variables);
                _right->getVariables(variables);
            }

            InequalityExpression(ColorExpression_ptr&& left, ColorExpression_ptr&& right)
                    : _left(std::move(left)), _right(std::move(right)) {}
        };

        class NotExpression : public GuardExpression {
        private:
            GuardExpression_ptr _expr;

        public:
            bool eval(ExpressionContext& context) const override {
                return !_expr->eval(context);
            }

            void getVariables(std::set<const Variable*>& variables) const override {
                _expr->getVariables(variables);
            }

            NotExpression(GuardExpression_ptr&& expr) : _expr(std::move(expr)) {}
        };

        class AndExpression : public GuardExpression {
        private:
            GuardExpression_ptr _left;
            GuardExpression_ptr _right;

        public:
            bool eval(ExpressionContext& context) const override {
                return _left->eval(context) && _right->eval(context);
            }

            void getVariables(std::set<const Variable*>& variables) const override {
                _left->getVariables(variables);
                _right->getVariables(variables);
            }

            AndExpression(GuardExpression_ptr&& left, GuardExpression_ptr&& right)
                    : _left(left), _right(right) {}
        };

        class OrExpression : public GuardExpression {
        private:
            GuardExpression_ptr _left;
            GuardExpression_ptr _right;

        public:
            bool eval(ExpressionContext& context) const override {
                return _left->eval(context) || _right->eval(context);
            }

            void getVariables(std::set<const Variable*>& variables) const override {
                _left->getVariables(variables);
                _right->getVariables(variables);
            }

            OrExpression(GuardExpression_ptr&& left, GuardExpression_ptr&& right)
                    : _left(std::move(left)), _right(std::move(right)) {}
        };

        class ArcExpression : public Expression {
        public:
            ArcExpression() {}
            virtual ~ArcExpression() {}

            virtual void getConstants(std::unordered_map<uint32_t, std::vector<const Color*>> &constantMap, uint32_t &index) const = 0;

            virtual Multiset eval(ExpressionContext& context) const = 0;

            virtual void expressionType() override {
                std::cout << "ArcExpression" << std::endl;
            }

            virtual uint32_t weight() const = 0;
            virtual bool isAll() const {
                return false;
            }
        };

        typedef std::shared_ptr<ArcExpression> ArcExpression_ptr;

        class AllExpression : public Expression {
        private:
            const ColorType* _sort;

        public:
            virtual ~AllExpression() {};
            std::vector<const Color*> eval(ExpressionContext& context) const {
                std::vector<const Color*> colors;
                assert(_sort != nullptr);
                for (size_t i = 0; i < _sort->size(); i++) {
                    colors.push_back(&(*_sort)[i]);
                }
                return colors;
            }

            size_t size() const {
                return  _sort->size();
            }

            void getConstants(std::unordered_map<uint32_t, std::vector<const Color*>> &constantMap, uint32_t &index) const {
                for (size_t i = 0; i < _sort->size(); i++) {
                    constantMap[index].push_back(&(*_sort)[i]);
                }
            }

            std::string toString() const override {
                return _sort->getName() + ".all";
            }

            AllExpression(const ColorType* sort) : _sort(sort)
            {
                assert(sort != nullptr);
            }
        };

        typedef std::shared_ptr<AllExpression> AllExpression_ptr;

        class NumberOfExpression : public ArcExpression {
        private:
            uint32_t _number;
            std::vector<ColorExpression_ptr> _color;
            AllExpression_ptr _all;

        public:
            Multiset eval(ExpressionContext& context) const override {
                std::vector<const Color*> colors;
                if (!_color.empty()) {
                    for (auto elem : _color) {
                        colors.push_back(elem->eval(context));
                    }
                } else if (_all != nullptr) {
                    colors = _all->eval(context);
                }
                std::vector<std::pair<const Color*,uint32_t>> col;
                for (auto elem : colors) {
                    col.push_back(std::make_pair(elem, _number));
                }
                return Multiset(col);
            }

            void getConstants(std::unordered_map<uint32_t, std::vector<const Color*>> &constantMap, uint32_t &index) const override {
                if (_all != nullptr)
                    _all->getConstants(constantMap, index);
                else for (auto elem : _color) {
                    std::unordered_map<uint32_t, const Color*> elemMap;
                    elem->getConstants(elemMap, index);
                    for(auto pair : elemMap){
                        constantMap[pair.first].push_back(pair.second);
                    }
                    index++;//not sure if index should be increased here, but no number expression have multiple elements
                }
            }

            void getVariables(std::set<const Variable*>& variables) const override {
                if (_all != nullptr)
                    return;
                for (auto elem : _color) {
                    elem->getVariables(variables);
                }
            }

            uint32_t weight() const override {
                if (_all == nullptr)
                    return _number * _color.size();
                else
                    return _number * _all->size();
            }

            bool isAll() const override {
                return static_cast<bool>(_all);
            }

            bool isSingleColor() const {
                return !isAll() && _color.size() == 1;
            }

            uint32_t number() const {
                return _number;
            }

            std::string toString() const override {
                if (isAll())
                    return std::to_string(_number) + "'(" + _all->toString() + ")";
                std::string res = std::to_string(_number) + "'(" + _color[0]->toString() + ")";
                for (uint32_t i = 1; i < _color.size(); ++i) {
                    res += " + ";
                    res += std::to_string(_number) + "'(" + _color[i]->toString() + ")";
                }
                return res;
            }

            NumberOfExpression(std::vector<ColorExpression_ptr>&& color, uint32_t number = 1)
                    : _number(number), _color(std::move(color)), _all(nullptr) {}
            NumberOfExpression(AllExpression_ptr&& all, uint32_t number = 1)
                    : _number(number), _color(), _all(std::move(all)) {}
        };

        typedef std::shared_ptr<NumberOfExpression> NumberOfExpression_ptr;

        class AddExpression : public ArcExpression {
        private:
            std::vector<ArcExpression_ptr> _constituents;

        public:
            Multiset eval(ExpressionContext& context) const override {
                Multiset ms;
                for (auto expr : _constituents) {
                    ms += expr->eval(context);
                }
                return ms;
            }

            void getConstants(std::unordered_map<uint32_t, std::vector<const Color*>> &constantMap, uint32_t &index) const override {
                uint32_t indexCopy = index;
                for (auto elem : _constituents) {
                    uint32_t localIndex = indexCopy;
                    elem->getConstants(constantMap, localIndex);
                }
            }

            void getVariables(std::set<const Variable*>& variables) const override {
                for (auto elem : _constituents) {
                    elem->getVariables(variables);
                }
            }

            uint32_t weight() const override {
                uint32_t res = 0;
                for (auto expr : _constituents) {
                    res += expr->weight();
                }
                return res;
            }

            std::string toString() const override {
                std::string res = _constituents[0]->toString();
                for (uint32_t i = 1; i < _constituents.size(); ++i) {
                    res += " + " + _constituents[i]->toString();
                }
                return res;
            }

            AddExpression(std::vector<ArcExpression_ptr>&& constituents)
                    : _constituents(std::move(constituents)) {}
        };

        class SubtractExpression : public ArcExpression {
        private:
            ArcExpression_ptr _left;
            ArcExpression_ptr _right;

        public:
            Multiset eval(ExpressionContext& context) const override {
                return _left->eval(context) - _right->eval(context);
            }

            void getConstants(std::unordered_map<uint32_t, std::vector<const Color*>> &constantMap, uint32_t &index) const override {
                uint32_t rIndex = index;
                _left->getConstants(constantMap, index);
                _right->getConstants(constantMap, rIndex);
            }

            void getVariables(std::set<const Variable*>& variables) const override {
                _left->getVariables(variables);
                _right->getVariables(variables);
            }

            uint32_t weight() const override {
                auto* left = dynamic_cast<NumberOfExpression*>(_left.get());
                if (!left || !left->isAll()) {
                    throw WeightException("Left constituent of subtract is not an all expression!");
                }
                auto* right = dynamic_cast<NumberOfExpression*>(_right.get());
                if (!right || !right->isSingleColor()) {
                    throw WeightException("Right constituent of subtract is not a single color number of expression!");
                }

                uint32_t val = std::min(left->number(), right->number());
                return _left->weight() - val;
            }

            std::string toString() const override {
                return _left->toString() + " - " + _right->toString();
            }

            SubtractExpression(ArcExpression_ptr&& left, ArcExpression_ptr&& right)
                    : _left(std::move(left)), _right(std::move(right)) {}
        };

        class ScalarProductExpression : public ArcExpression {
        private:
            uint32_t _scalar;
            ArcExpression_ptr _expr;

        public:
            Multiset eval(ExpressionContext& context) const override {
                return _expr->eval(context) * _scalar;
            }

            void getConstants(std::unordered_map<uint32_t, std::vector<const Color*>> &constantMap, uint32_t &index) const override {
                _expr->getConstants(constantMap, index);
            }

            void getVariables(std::set<const Variable*>& variables) const override {
                _expr->getVariables(variables);
            }

            uint32_t weight() const override {
                return _scalar * _expr->weight();
            }

            std::string toString() const override {
                return std::to_string(_scalar) + " * " + _expr->toString();
            }

            ScalarProductExpression(ArcExpression_ptr&& expr, uint32_t scalar)
                    : _scalar(std::move(scalar)), _expr(expr) {}
        };
    }
}

#endif /* COLORED_EXPRESSIONS_H */
