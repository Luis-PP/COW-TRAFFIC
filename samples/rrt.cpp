#include "rrt.h"
#include "sample.h"

#include "box2d/box2d.h"
#include "box2d/math_functions.h"

#include <assert.h>
#include <iostream>

RRT::RRT()
{
    root = nullptr; // Initialize root as nullptr
}

std::vector<b2Vec2> RRT::FindPath(b2Vec2 start, b2Vec2 goal, std::vector<b2AABB> obstacles, b2Vec2 max_barn_area, float step_size, float goal_threshold)
{
    this->start = start;
    this->goal = goal;
    this->obstacles = obstacles;
    this->max_barn_area = max_barn_area;
    this->step_size = step_size;
    this->goal_threshold = goal_threshold;
    root = new Node{start, nullptr};
    nodes.push_back(root);

    int i_try = 0;
    while (true)
    {
        ++i_try;
        b2Vec2 randPoint = SampleRandomPoint();
        Node *nearest = FindNearestNode(randPoint);
        Node *newNode = ExtendTree(nearest, randPoint);

        if (newNode && IsGoalReached(newNode))
        {
            return BuildPath(newNode);
        }
    }
}

b2Vec2 RRT::SampleRandomPoint()
{
    float x = static_cast<float>(rand()) / RAND_MAX * max_barn_area.x;
    float y = static_cast<float>(rand()) / RAND_MAX * max_barn_area.y;
    return b2Vec2{x, y};
}

Node *RRT::FindNearestNode(b2Vec2 point)
{
    Node *nearest = nullptr;
    float minDist = std::numeric_limits<float>::max();
    for (Node *node : nodes)
    {
        float dist = b2DistanceSquared(point, node->position); // Prefer squared distance
        if (dist < minDist)
        {
            nearest = node;
            minDist = dist;
        }
    }
    return nearest;
}

Node *RRT::ExtendTree(Node *nearest, b2Vec2 point)
{
    b2Vec2 direction = point - nearest->position;
    // direction.Normalize();
    float length = b2Length(direction); // Get the length of the vector
    if (length > 0.0f)
    {
        direction *= 1.0f / length; // Normalize the vector
    }
    b2Vec2 newPosition = nearest->position + direction * step_size;

    if (!ObstaclesInBetween(nearest->position, newPosition, obstacles))
    {
        Node *newNode = new Node{newPosition, nearest};
        nodes.push_back(newNode);
        return newNode;
    }
    return nullptr;
}

bool RRT::ObstaclesInBetween(b2Vec2 from, b2Vec2 to, std::vector<b2AABB> obstacles)
{
    for (const auto &obstacle : obstacles)
    {
        if (RayIntersectsAABB(obstacle, from, to))
        {
            return true;
        }
    }
    return false;
}

bool RRT::RayIntersectsAABB(b2AABB aabb, b2Vec2 from_, b2Vec2 to_)
{
    float tmin = (aabb.lowerBound.x - from_.x) / (to_.x - from_.x);
    float tmax = (aabb.upperBound.x - from_.x) / (to_.x - from_.x);

    if (tmin > tmax)
        std::swap(tmin, tmax);

    float tymin = (aabb.lowerBound.y - from_.y) / (to_.y - from_.y);
    float tymax = (aabb.upperBound.y - from_.y) / (to_.y - from_.y);

    if (tymin > tymax)
        std::swap(tymin, tymax);

    if ((tmin > tymax) || (tymin > tmax))
        return false;

    if (tymin > tmin)
        tmin = tymin;

    if (tymax < tmax)
        tmax = tymax;

    return (tmax >= 0);
}

bool RRT::IsGoalReached(Node *node)
{
    return b2Distance(node->position, goal) < goal_threshold;
}

std::vector<b2Vec2> RRT::BuildPath(Node *node)
{
    std::vector<b2Vec2> path;
    while (node != nullptr)
    {
        path.push_back(node->position);
        node = node->parent;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

// bool RRT::InObstace(b2Vec2 point, std::vector<b2AABB> obstacles, float inflate)
// {
//     for (const auto &obstacle : obstacles)
//     {
//         // std::cout<<"obstacle: "<<obstacle.lowerBound.x<<"|"<<obstacle.lowerBound.y<<"|"<<obstacle.upperBound.x<<"|"<<obstacle.upperBound.y<<std::endl;
//         if ((obstacle.lowerBound.x - inflate <= point.x <= obstacle.upperBound.x + inflate) &&
//             (obstacle.lowerBound.y - inflate <= point.y <= obstacle.upperBound.y + inflate))
//         {
//             return true;
//             break;
//         }
//     }
//     return false;
// }
