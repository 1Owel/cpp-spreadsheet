#include "formula.h"

#include "FormulaAST.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>

using namespace std::literals;

std::ostream& operator<<(std::ostream& output, FormulaError fe) {
    return output << "#ARITHM!";
}

namespace {
class Formula : public FormulaInterface {
public:
    explicit Formula(std::string expression) 
        : ast_(ParseFormulaAST(expression)) 
    {
    }

    Value Evaluate(const SheetInterface& sheet) const override {
        try {
            return ast_.Execute(sheet);
        } catch (const FormulaError& error) {
            return error;
        }
    }

    std::string GetExpression() const override {
        std::ostringstream out;
        ast_.PrintFormula(out);
        return out.str();
    }

    std::vector<Position> GetReferencedCells() const override {
        auto cells = ast_.GetCells();
        cells.sort();
        cells.unique();
        return {cells.begin(), cells.end()};
    }

private:
    FormulaAST ast_;
};
}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    try {
        return std::make_unique<Formula>(std::move(expression));
    } catch (const FormulaException& e) {
        throw; // Пробрасываем FormulaException напрямую
    } catch (const std::exception& e) {
        throw FormulaException(e.what()); // Оборачиваем остальные исключения
    }
}