#include "mapmaker.h"
#include "sample.h"

#include "box2d/box2d.h"
#include "box2d/math_functions.h"

#include <assert.h>
#include <iostream>

//

MapMaker::MapMaker()
{
    std::vector<SampleFunctionalArea> layout;
    std::vector<b2AABB> cow_map;
    std::vector<b2AABB> cow_aabbs;
}

std::vector<std::vector<bool>> MapMaker::LayoutToGrid()
{
    std::vector<std::vector<bool>> grid_map(corner_layout.first, std::vector<bool>(corner_layout.second, false));
    for (int i = 0; i < layout.size(); i++)
    {
        if (layout[i].orientation == 0)
        {

            grid_map[layout[i].x][layout[i].y] = true;
        }
        else if (layout[i].orientation == 1)
        {

            grid_map[layout[i].x][layout[i].y] = true;
            grid_map[layout[i].x][layout[i].y + 1] = true;
        }
        else if (layout[i].orientation == 2)
        {

            grid_map[layout[i].x][layout[i].y] = true;
            grid_map[layout[i].x + 1][layout[i].y] = true;
        }
    }

    // for (auto v1 : grid_map)
    // {
    //     for (auto v2 : v1)
    //     {
    //         std::cout << "Obstacle? " << v2 << std::endl;
    //     }
    // }

    return grid_map;
}

std::pair<int, int> MapMaker::WorldToGrid(const b2Vec2 point)
{
    int x = static_cast<int>(point.x / corner_layout.first);
    int y = static_cast<int>(point.y / corner_layout.second);
    return {x, y};
}

b2AABB MapMaker::ConvertToABB(float x, float y, int orientation)
{
    b2Vec2 lower_bound, upper_bound;
    b2AABB aabb;

    if (orientation == 0)
    {

        lower_bound = {x - orient_map_arr[orientation].x, y - orient_map_arr[orientation].y};
        upper_bound = {x + orient_map_arr[orientation].x, y + orient_map_arr[orientation].y};
    }
    else if (orientation == 1)
    {
        lower_bound = {x - orient_map_arr[orientation].x, y - orient_map_arr[orientation].y};
        upper_bound = {x + orient_map_arr[orientation].x, y + orient_map_arr[orientation].y};
    }
    else if (orientation == 2)
    {
        lower_bound = {x - orient_map_arr[orientation].x, y - orient_map_arr[orientation].y};
        upper_bound = {x + orient_map_arr[orientation].x, y + orient_map_arr[orientation].y};
    }

    aabb.lowerBound = lower_bound;
    aabb.upperBound = upper_bound;
    return aabb;
}

bool MapMaker::CanMerge(const b2AABB &a, const b2AABB &b)
{
    // std::cout << "In CanMerge: " << std::endl;

    if (a.upperBound.x == b.lowerBound.x || b.upperBound.x == a.lowerBound.x)
    {
        return a.lowerBound.y == b.lowerBound.y && a.upperBound.y == b.upperBound.y;
    }
    if (a.upperBound.y == b.lowerBound.y || b.upperBound.y == a.lowerBound.y)
    {
        return a.lowerBound.x == b.lowerBound.x && a.upperBound.x == b.upperBound.x;
    }
    return false;
}

b2AABB MapMaker::Merge(const b2AABB &a, const b2AABB &b)
{
    // std::cout << "In Merge: " << std::endl;

    b2AABB merged;
    merged.lowerBound.x = std::min(a.lowerBound.x, b.lowerBound.x);
    merged.lowerBound.y = std::min(a.lowerBound.y, b.lowerBound.y);
    merged.upperBound.x = std::max(a.upperBound.x, b.upperBound.x);
    merged.upperBound.y = std::max(a.upperBound.y, b.upperBound.y);
    return merged;
}

void MapMaker::CreateCowMap()
{
    std::vector<b2AABB> aabbs = cow_aabbs;
    // std::cout << "Before merge: " << aabbs.size() << std::endl;

    // Sort the AABBs by their lower bound to facilitate merging
    std::sort(aabbs.begin(), aabbs.end(), [](const b2AABB &a, const b2AABB &b)
              { return a.lowerBound.x < b.lowerBound.x || (a.lowerBound.x == b.lowerBound.x && a.lowerBound.y < b.lowerBound.y); });

    // for (auto rec : aabbs)
    // {
    //     std::cout << rec.lowerBound.x << "|" << rec.lowerBound.y << "|" << rec.upperBound.x << "|" << rec.upperBound.y << std::endl;
    // }

    std::vector<b2AABB> mergedAABBs;

    // Loop over each rectangle and attempt to merge it
    while (!aabbs.empty())
    {
        b2AABB current = aabbs[0];
        aabbs.erase(aabbs.begin());

        bool merged = true;
        while (merged)
        {
            merged = false;

            for (size_t i = 0; i < aabbs.size(); ++i)
            {
                if (CanMerge(current, aabbs[i]))
                {
                    current = Merge(current, aabbs[i]);
                    aabbs.erase(aabbs.begin() + i);
                    merged = true;
                    break; // Restart the merge process after a merge
                }
            }
        }

        mergedAABBs.push_back(current);
    }

    // std::cout << "After merge: " << mergedAABBs.size() << std::endl;

    // for (auto rec : mergedAABBs)
    // {
    //     std::cout << rec.lowerBound.x << "|" << rec.lowerBound.y << "|" << rec.upperBound.x << "|" << rec.upperBound.y << std::endl;
    // }
    cow_map = mergedAABBs;
}

void MapMaker::DestroyMaps()
{
    cow_aabbs.clear();
    cow_map.clear();
}
