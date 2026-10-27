#pragma once
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// x/z 평면 격자 한 칸
struct AoiCell
{
    int32_t x = 0;
    int32_t z = 0;

    bool operator==(const AoiCell& o) const
    {
        return x == o.x && z == o.z;
    }
    bool operator!=(const AoiCell& o) const
    {
        return !(*this == o);
    }
};

// 시야 = 내 칸 + 주변 8칸 (3x3)
class AoiGrid
{
  public:
    explicit AoiGrid(float cellSize) : cellSize_(cellSize) {}

    AoiCell ToCell(float x, float z) const
    {
        return {static_cast<int32_t>(std::floor(x / cellSize_)), static_cast<int32_t>(std::floor(z / cellSize_))};
    }

    void Add(uint64_t id, AoiCell c)
    {
        cells_[Key(c)].insert(id);
    }

    void Remove(uint64_t id, AoiCell c)
    {
        auto it = cells_.find(Key(c));
        // id 없으면 그냥 무시
        if (it == cells_.end())
            return;

        // cells_에서 id 제거
        it->second.erase(id);
        if (it->second.empty())
            cells_.erase(it); // 빈 칸은 지운다
    }

    // 이전 칸에서 제거하고 새 칸에 추가
    void Move(uint64_t id, AoiCell from, AoiCell to)
    {
        if (from == to)
            return;

        Remove(id, from);
        Add(id, to);
    }

    void Clear()
    {
        cells_.clear();
    }

    // 콜백 안에서 Add/Remove/Move 하면 안 됨 (순회 중인 set이 바뀜)
    template <typename Fn>
    void ForEachInCell(AoiCell c, Fn&& fn) const
    {
        auto it = cells_.find(Key(c));
        if (it == cells_.end())
            return;
        for (uint64_t id : it->second)
            fn(id);
    }

    // c 기준 3x3칸
    static void NearCells(AoiCell c, std::vector<AoiCell>& out)
    {
        for (int32_t dz = -1; dz <= 1; ++dz)
            for (int32_t dx = -1; dx <= 1; ++dx)
                out.push_back({c.x + dx, c.z + dz});
    }

    static bool IsNear(AoiCell a, AoiCell b)
    {
        return std::abs(a.x - b.x) <= 1 && std::abs(a.z - b.z) <= 1;
    }

    // from -> to 이동 시 새로 보이게 된 칸(entered) / 안 보이게 된 칸(left)
    static void DiffNear(AoiCell from, AoiCell to, std::vector<AoiCell>& entered, std::vector<AoiCell>& left)
    {
        std::vector<AoiCell> toNear, fromNear;

        // 이동 후 칸에서 체크
        NearCells(to, toNear);
        // 이동 전 칸에서 체크
        NearCells(from, fromNear);

        // 이동 후 칸 근처 중 이동 전 칸 근처에 없는 칸 = 새로 보이게 된 칸
        for (const AoiCell& c : toNear)
            if (!IsNear(c, from))
                entered.push_back(c);
        
        // 이동 전 칸 근처 중 이동 후 칸 근처에 없는 칸 = 안 보이게 된 칸
        for (const AoiCell& c : fromNear)
            if (!IsNear(c, to))
                left.push_back(c);
    }

  private:
    static uint64_t Key(AoiCell c)
    {
        // 음수 좌표도 안전하게: uint32로 바꾼 뒤 합침
        return (static_cast<uint64_t>(static_cast<uint32_t>(c.x)) << 32) | static_cast<uint32_t>(c.z);
    }

  private:
    float cellSize_;
    std::unordered_map<uint64_t, std::unordered_set<uint64_t>> cells_;
};