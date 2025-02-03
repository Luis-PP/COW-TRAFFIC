#pragma once

#include "box2d/types.h"

#include <vector>

struct Node
{
    b2Vec2 position;
    Node *parent; // Node* to avoid infinite recursion
};

class RRT
{
public:
    RRT();
    std::vector<b2Vec2> FindPath(b2Vec2 start, b2Vec2 goal, std::vector<b2AABB> obstacles, b2Vec2 max_barn_area, float step_size, float goal_threshold);

    Node *root;                // Node* to match dynamic allocation in cpp
    std::vector<Node *> nodes; // Store pointers to Nodes
    b2Vec2 start;
    b2Vec2 goal;
    b2Vec2 max_barn_area;
    float step_size;
    float goal_threshold;
    std::vector<b2AABB> obstacles;

    b2Vec2 SampleRandomPoint();
    Node *FindNearestNode(b2Vec2 point);           // Return pointer
    Node *ExtendTree(Node *nearest, b2Vec2 point); // Return pointer
    bool ObstaclesInBetween(b2Vec2 from, b2Vec2 to, std::vector<b2AABB> obstacles);
    bool IsGoalReached(Node *node);            // Parameter is a pointer
    std::vector<b2Vec2> BuildPath(Node *node); // Parameter is a pointer
    bool RayIntersectsAABB(b2AABB aabb, b2Vec2 from_, b2Vec2 to_);
    // bool InObstace(b2Vec2 point, std::vector<b2AABB> obstacles,  float inflate);
};
