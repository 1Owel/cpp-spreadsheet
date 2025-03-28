#include "cell.h"
#include "formula.h"
#include "sheet.h"

#include <stdexcept>
#include <string>
#include <vector>

// Базовый класс для всех реализаций
class Cell::Impl {
public:
    virtual ~Impl() = default;
    virtual Value GetValue() const = 0;
    virtual std::string GetText() const = 0;
};

// Пустая ячейка
class Cell::EmptyImpl : public Impl {
public:
    Value GetValue() const override { return ""; }
    std::string GetText() const override { return ""; }
};

// Текстовая ячейка
class Cell::TextImpl : public Impl {
public:
    explicit TextImpl(std::string text, bool escaped)
        : text_(std::move(text)), escaped_(escaped) {}

    Value GetValue() const override { 
        return text_;
    }

    std::string GetText() const override { 
        return escaped_ ? ESCAPE_SIGN + text_ : text_;
    }

private:
    std::string text_;
    bool escaped_;
};

// Формульная ячейка
class Cell::FormulaImpl : public Impl {
public:
    FormulaImpl(std::string expr, std::unique_ptr<FormulaInterface> formula, const SheetInterface& sheet)
    : expr_(std::move(expr)), formula_(std::move(formula)), sheet_(sheet) {}

    Value GetValue() const override {
        auto result = formula_->Evaluate(sheet_);
        return std::visit([](auto&& arg) -> Value { return arg; }, result);
    }

    std::string GetText() const override { 
        return FORMULA_SIGN + formula_->GetExpression(); 
    }

    std::vector<Position> GetReferencedCells() const {
        return formula_->GetReferencedCells();
    }

private:
    std::string expr_;
    std::unique_ptr<FormulaInterface> formula_;
    const SheetInterface& sheet_;
};

// Реализация методов Cell
Cell::Cell(SheetInterface& sheet) 
    : sheet_(sheet), 
      impl_(std::make_unique<EmptyImpl>()) {}

Cell::~Cell() = default;

void Cell::Set(Position pos, std::string text) {
    std::unique_ptr<Impl> new_impl;

    try {
        if (text.empty()) {
            new_impl = std::make_unique<EmptyImpl>();
        }
        // Формула: начинается с '=' и имеет длину > 1
        else if (text[0] == FORMULA_SIGN) {
            std::string expr = text.substr(1);
            auto formula = ParseFormula(expr);
            auto deps = formula->GetReferencedCells();
            Sheet& sh = reinterpret_cast<Sheet&>(sheet_);

            // Проверка валидности зависимостей
            for (const auto& dep : deps) {
                if (!dep.IsValid()) {
                    throw FormulaException("Invalid cell position: " + dep.ToString());
                }
            }

            // Проверка циклических зависимостей
            sh.CheckCircularDependency(pos, deps);

            // Обновление зависимостей через Sheet
            sh.UpdateDependencies(pos, deps);

            new_impl = std::make_unique<FormulaImpl>(std::move(expr), std::move(formula), sheet_);
            // SetCell выполнит проверки и очистку ячейки
        }
        // Текст с экранированием (начинается с апострофа)
        else if (text[0] == ESCAPE_SIGN) {
            new_impl = std::make_unique<TextImpl>(text.substr(1), true);
        }
        // Обычный текст (включая строки с '=' внутри)
        else {
            new_impl = std::make_unique<TextImpl>(std::move(text), false);
        }
    } catch (const FormulaException&) {
        throw; // Исключение, не меняя состояние ячейки
    }

    // Обновляем состояние только при успешном создании
    impl_ = std::move(new_impl);
}

std::string Cell::GetText() const {
    return impl_->GetText();
}

void Cell::InvalidateCache() {
    is_valid_ = false;
    cached_value_.reset();
}

Cell::Value Cell::GetValue() const {
    if (!is_valid_) {
        try {
            cached_value_ = impl_->GetValue();
        } catch (const FormulaError& e) {
            cached_value_ = e;
        }
        is_valid_ = true;
    }
    return *cached_value_;
}

std::vector<Position> Cell::GetReferencedCells() const {
    if (auto* f = dynamic_cast<FormulaImpl*>(impl_.get())) {
        return f->GetReferencedCells();
    }
    return {};
}