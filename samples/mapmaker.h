#pragma once
#include "sample.h"

#include "box2d/types.h"

#include <vector>

const std::vector<b2Vec2> orient_map_arr = {{12.0f, 12.0f}, {12.0f, 24.0f}, {24.0f, 12.0f}}; //

class MapMaker
{
public:
    MapMaker();

    std::vector<SampleFunctionalArea> layout;
    std::pair<int, int> corner_layout;
    std::vector<std::vector<bool>> grid_map;

    std::vector<b2AABB> cow_map;
    std::vector<b2AABB> cow_aabbs;
    b2Vec2 max_map_area;

    std::vector<std::vector<bool>> LayoutToGrid();
    std::pair<int, int> WorldToGrid(b2Vec2 point);

    b2AABB ConvertToABB(float x, float y, int orientation);
    bool CanMerge(const b2AABB &a, const b2AABB &b);
    b2AABB Merge(const b2AABB &a, const b2AABB &b);
    void CreateCowMap();
    void DestroyMaps();
};
