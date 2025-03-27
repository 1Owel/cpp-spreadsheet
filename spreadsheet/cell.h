#pragma once

#include "common.h"
#include "formula.h"

#include <optional>
#include <functional>
#include <unordered_set>

class Sheet;

class Cell : public CellInterface {
public:
    Cell(SheetInterface& sheet);
    ~Cell();

    void Set(std::string text);
    void Clear();

    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;
    void InvalidateCache();

    bool IsReferenced() const;

private:
    SheetInterface& sheet_;

    class Impl;
    class EmptyImpl;
    class TextImpl;
    class FormulaImpl;

    std::unique_ptr<Impl> impl_;

    std::vector<Position> referenced_cells_;
    mutable std::optional<Value> cached_value_;
    mutable bool is_valid_ = false;
};