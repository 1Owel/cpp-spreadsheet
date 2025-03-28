#pragma once

#include "cell.h"
#include "common.h"

#include <functional>
#include <set>
#include <unordered_map>

class Sheet : public SheetInterface {
public:
    friend class Cell;
    ~Sheet() override = default;

    void SetCell(Position pos, std::string text) override;
    const CellInterface* GetCell(Position pos) const override;
    CellInterface* GetCell(Position pos) override;
    void ClearCell(Position pos) override;
    Size GetPrintableSize() const override;
    void PrintValues(std::ostream& output) const override;
    void PrintTexts(std::ostream& output) const override;

private:
    struct PositionHash {
        size_t operator()(const Position& pos) const {
            return std::hash<int>()(pos.row) ^ (std::hash<int>()(pos.col) << 1);
        }
    };

    std::unordered_map<Position, std::unique_ptr<Cell>, PositionHash> cells_;
    std::set<int> rows_;
    std::set<int> cols_;
    std::unordered_map<int, int> row_counts_;
    std::unordered_map<int, int> col_counts_;

    int GetMaxRow() const;
    int GetMaxCol() const;

    void CheckCircularDependency(const Position& pos, const std::vector<Position>& deps);
    void UpdateDependencies(const Position& pos, const std::vector<Position>& deps);
    void InvalidateCacheForDependents(const Position& pos);

    std::unordered_map<Position, std::vector<Position>, PositionHash> dependencies_;
    std::unordered_map<Position, std::vector<Position>, PositionHash> reverse_dependencies_;

    void UpdateSheetSize(Position pos);
};