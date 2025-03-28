#include "sheet.h"
#include "cell.h"
#include "common.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <sstream>
#include <unordered_map> 
#include <unordered_set>

using namespace std;

inline void InvalidPositionCheck(Position pos) const {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position");
    }
}

void Sheet::UpdateSheetSize(Position pos) {
    int row = pos.row;
    int col = pos.col;

    if (row_counts_[row]++ == 0) {
        rows_.insert(row);
    }

    if (col_counts_[col]++ == 0) {
        cols_.insert(col);
    }
}

void Sheet::SetCell(Position pos, std::string text) {
    InvalidPositionCheck(pos);

    if (text.empty()) {
        ClearCell(pos);
        return;
    }

    auto cell = make_unique<Cell>(*this);
    cell->Set(pos, text); // Передаём позицию в Cell::Set

    // Удаляем старую ячейку
    if (cells_.count(pos)) {
        ClearCell(pos);
    }

    cells_[pos] = move(cell);
    InvalidateCacheForDependents(pos); // Инвалидация кэша

    // Обновляем строки и столбцы
    UpdateSheetSize(pos);
}

const CellInterface* Sheet::GetCell(Position pos) const {
    InvalidPositionCheck(pos);
    auto it = cells_.find(pos);
    return it != cells_.end() ? it->second.get() : nullptr;
}

CellInterface* Sheet::GetCell(Position pos) {
    return const_cast<CellInterface*>(static_cast<const Sheet*>(this)->GetCell(pos));
}

void Sheet::ClearCell(Position pos) {
    InvalidPositionCheck(pos);

    auto it = cells_.find(pos);
    if (it == cells_.end()) {
        return;
    }

    cells_.erase(it);
    int row = pos.row;
    int col = pos.col;

    // Обновляем строки
    if (--row_counts_[row] == 0) {
        rows_.erase(row);
        row_counts_.erase(row);
    }

    // Обновляем столбцы
    if (--col_counts_[col] == 0) {
        cols_.erase(col);
        col_counts_.erase(col);
    }
}

Size Sheet::GetPrintableSize() const {
    if (rows_.empty() || cols_.empty()) {
        return {0, 0};
    }
    int max_row = *rows_.rbegin();
    int max_col = *cols_.rbegin();
    return {max_row + 1, max_col + 1};
}

void Sheet::PrintValues(ostream& output) const {
    Size size = GetPrintableSize();
    int max_row = size.rows - 1;
    int max_col = size.cols - 1;

    for (int row = 0; row <= max_row; ++row) {
        for (int col = 0; col <= max_col; ++col) {
            if (col > 0) {
                output << '\t';
            }
            Position pos{row, col};
            auto it = cells_.find(pos);
            if (it != cells_.end()) {
                visit([&](const auto& value) { output << value; }, it->second->GetValue());
            }
        }
        output << '\n';
    }
}

void Sheet::PrintTexts(ostream& output) const {
    Size size = GetPrintableSize();
    int max_row = size.rows - 1;
    int max_col = size.cols - 1;

    for (int row = 0; row <= max_row; ++row) {
        for (int col = 0; col <= max_col; ++col) {
            if (col > 0) {
                output << '\t';
            }
            Position pos{row, col};
            auto it = cells_.find(pos);
            if (it != cells_.end()) {
                output << it->second->GetText();
            }
        }
        output << '\n';
    }
}

unique_ptr<SheetInterface> CreateSheet() {
    return make_unique<Sheet>();
}

void Sheet::CheckCircularDependency(const Position& pos, const std::vector<Position>& deps) {
    std::unordered_set<Position, PositionHash> visited;
    std::function<bool(const Position&)> hasCycle = [&](const Position& current) {
        if (current == pos) return true;
        if (visited.count(current)) return false;
        visited.insert(current);
        for (const auto& dep : dependencies_[current]) {
            if (hasCycle(dep)) return true;
        }
        return false;
    };
    for (const auto& dep : deps) {
        if (hasCycle(dep)) throw CircularDependencyException("Cycle detected");
    }
}

void Sheet::UpdateDependencies(const Position& pos, const std::vector<Position>& new_deps) {
    // Удалить старые зависимости
    for (const auto& old_dep : dependencies_[pos]) {
        auto& rev = reverse_dependencies_[old_dep];
        rev.erase(remove(rev.begin(), rev.end(), pos), rev.end());
    }
    dependencies_[pos] = new_deps;
    // Добавить новые зависимости
    for (const auto& dep : new_deps) {
        reverse_dependencies_[dep].push_back(pos);
    }
}

void Sheet::InvalidateCacheForDependents(const Position& pos) {
    std::unordered_set<Position, PositionHash> visited;
    std::function<void(const Position&)> invalidate = [&](const Position& current) {
        if (visited.count(current)) return;
        visited.insert(current);
        if (CellInterface* cell = GetCell(current)) {
            static_cast<Cell*>(cell)->InvalidateCache();
        }
        for (const auto& dep : reverse_dependencies_[current]) {
            invalidate(dep);
        }
    };
    invalidate(pos);
}