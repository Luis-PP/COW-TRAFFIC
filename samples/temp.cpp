#include <vector>
#include <algorithm>
#include <box2d/box2d.h>

struct Obstacle {
    float x, y;
    int type;
    int orientation;
};

// Function to convert grid coordinates to world coordinates
b2AABB ConvertToAABB(const Obstacle& obs) {
    float x = obs.x * 24.0f + 12.0f;
    float y = obs.y * 24.0f + 12.0f;

    b2Vec2 lowerBound, upperBound;

    if (obs.orientation == 0) { // Square
        lowerBound.Set(x - 12.0f, y - 12.0f);
        upperBound.Set(x + 12.0f, y + 12.0f);
    } else if (obs.orientation == 1) { // Vertical rectangle
        lowerBound.Set(x - 12.0f, y - 24.0f);
        upperBound.Set(x + 12.0f, y);
    } else if (obs.orientation == 2) { // Horizontal rectangle
        lowerBound.Set(x, y - 12.0f);
        upperBound.Set(x + 24.0f, y + 12.0f);
    }

    b2AABB aabb;
    aabb.lowerBound = lowerBound;
    aabb.upperBound = upperBound;

    return aabb;
}

// Function to check if two AABBs can be merged into one
bool CanMerge(const b2AABB& a, const b2AABB& b) {
    if (a.upperBound.x == b.lowerBound.x || b.upperBound.x == a.lowerBound.x) {
        return a.lowerBound.y == b.lowerBound.y && a.upperBound.y == b.upperBound.y;
    }
    if (a.upperBound.y == b.lowerBound.y || b.upperBound.y == a.lowerBound.y) {
        return a.lowerBound.x == b.lowerBound.x && a.upperBound.x == b.upperBound.x;
    }
    return false;
}

// Function to merge two AABBs
b2AABB Merge(const b2AABB& a, const b2AABB& b) {
    b2AABB merged;
    merged.lowerBound.x = std::min(a.lowerBound.x, b.lowerBound.x);
    merged.lowerBound.y = std::min(a.lowerBound.y, b.lowerBound.y);
    merged.upperBound.x = std::max(a.upperBound.x, b.upperBound.x);
    merged.upperBound.y = std::max(a.upperBound.y, b.upperBound.y);
    return merged;
}

// Function to group and merge adjacent obstacles, including map borders
std::vector<b2AABB> GroupAndMergeObstaclesWithBorders(const std::vector<Obstacle>& obstacles, float mapWidth, float mapHeight) {
    std::vector<b2AABB> aabbs;

    // Convert obstacles to AABBs
    for (const auto& obs : obstacles) {
        aabbs.push_back(ConvertToAABB(obs));
    }

    // Add map borders as obstacles
    b2AABB leftBorder, rightBorder, topBorder, bottomBorder;

    // Left border
    leftBorder.lowerBound.Set(0.0f, 0.0f);
    leftBorder.upperBound.Set(12.0f, mapHeight);
    aabbs.push_back(leftBorder);

    // Right border
    rightBorder.lowerBound.Set(mapWidth - 12.0f, 0.0f);
    rightBorder.upperBound.Set(mapWidth, mapHeight);
    aabbs.push_back(rightBorder);

    // Bottom border
    bottomBorder.lowerBound.Set(0.0f, 0.0f);
    bottomBorder.upperBound.Set(mapWidth, 12.0f);
    aabbs.push_back(bottomBorder);

    // Top border
    topBorder.lowerBound.Set(0.0f, mapHeight - 12.0f);
    topBorder.upperBound.Set(mapWidth, mapHeight);
    aabbs.push_back(topBorder);

    // Sort the AABBs by their lower bound to facilitate merging
    std::sort(aabbs.begin(), aabbs.end(), [](const b2AABB& a, const b2AABB& b) {
        return a.lowerBound.x < b.lowerBound.x || (a.lowerBound.x == b.lowerBound.x && a.lowerBound.y < b.lowerBound.y);
    });

    // Result vector for merged AABBs
    std::vector<b2AABB> mergedAABBs;

    for (size_t i = 0; i < aabbs.size(); ++i) {
        b2AABB current = aabbs[i];
        bool merged = false;

        for (size_t j = i + 1; j < aabbs.size(); ++j) {
            if (CanMerge(current, aabbs[j])) {
                current = Merge(current, aabbs[j]);
                aabbs.erase(aabbs.begin() + j);
                --j;
                merged = true;
            }
        }

        if (!merged || i == aabbs.size() - 1) {
            mergedAABBs.push_back(current);
        }
    }

    return mergedAABBs;
}

// Usage
int main() {
    std::vector<Obstacle> obstacles = {
        {0, 0, 0, 0},  // Square at (0,0)
        {1, 0, 0, 0},  // Square at (1,0), adjacent to the first square
        {2, 0, 0, 0},  // Square at (2,0), forming a line with the first two squares
        {4, 0, 0, 0},  // Square at (4,0), separate from the others
        {0, 1, 0, 0},  // Square at (0,1), forming a square with the first two squares
        {0, 3, 0, 1},  // Vertical rectangle starting at (0,3)
        {2, 3, 0, 2},  // Horizontal rectangle starting at (2,3)
    };

    float mapWidth = 100.0f;
    float mapHeight = 100.0f;

    std::vector<b2AABB> mergedObstacles = GroupAndMergeObstaclesWithBorders(obstacles, mapWidth, mapHeight);

    // Print merged obstacles for debugging
    for (const auto& aabb : mergedObstacles) {
        std::cout << "Obstacle: [(" << aabb.lowerBound.x << ", " << aabb.lowerBound.y << "), ("
                  << aabb.upperBound.x << ", " << aabb.upperBound.y << ")]" << std::endl;
    }

    return 0;
}


//----------------------------------------
struct Node {
    b2Vec2 position;
    Node* parent;
};

class RRT {
public:
    RRT(b2World* world, const b2Vec2& start, const b2Vec2& goal)
        : world(world), start(start), goal(goal) {
        root = new Node{start, nullptr};
        nodes.push_back(root);
    }

    std::vector<b2Vec2> FindPath() {
        while (true) {
            b2Vec2 randPoint = SampleRandomPoint();
            Node* nearest = FindNearestNode(randPoint);
            Node* newNode = ExtendTree(nearest, randPoint);

            if (newNode && IsGoalReached(newNode)) {
                return BuildPath(newNode);
            }
        }
    }

private:
    b2World* world;
    Node* root;
    std::vector<Node*> nodes;
    b2Vec2 start;
    b2Vec2 goal;

    b2Vec2 SampleRandomPoint() {
        // Random sampling in the environment
        float x = static_cast<float>(rand()) / RAND_MAX * 100.0f; // Assuming a 100x100 area
        float y = static_cast<float>(rand()) / RAND_MAX * 100.0f;
        return b2Vec2(x, y);
    }

    Node* FindNearestNode(const b2Vec2& point) {
        Node* nearest = nullptr;
        float minDist = std::numeric_limits<float>::max();
        for (Node* node : nodes) {
            float dist = b2Distance(point, node->position);
            if (dist < minDist) {
                nearest = node;
                minDist = dist;
            }
        }
        return nearest;
    }

    Node* ExtendTree(Node* nearest, const b2Vec2& point) {
        b2Vec2 direction = point - nearest->position;
        direction.Normalize();
        b2Vec2 newPosition = nearest->position + direction * step_size;

        if (!CollidesWithObstacles(nearest->position, newPosition)) {
            Node* newNode = new Node{newPosition, nearest};
            nodes.push_back(newNode);
            return newNode;
        }
        return nullptr;
    }

    bool CollidesWithObstacles(const b2Vec2& from, const b2Vec2& to) {
        // Perform collision checking between `from` and `to`
        // Use Box2D ray casting or manually check against obstacles
        return false; // Placeholder; Implement collision logic
    }

    bool IsGoalReached(Node* node) {
        return b2Distance(node->position, goal) < goal_threshold;
    }

    std::vector<b2Vec2> BuildPath(Node* node) {
        std::vector<b2Vec2> path;
        while (node != nullptr) {
            path.push_back(node->position);
            node = node->parent;
        }
        std::reverse(path.begin(), path.end());
        return path;
    }

    const float step_size = 5.0f;
    const float goal_threshold = 2.0f;
};

// ----------------
#include <math.h>
#include <cmath>
#include <random>
#include "rrt.hpp"

using namespace std;

RRT::RRT(const float start_[],
         const float goal_[], 
         const float min_max_points_[], 
         const float max_len_, 
         const int goal_sample_rate_, 
         const int max_iter_){
    
    start.x = start_[0];
    start.y = start_[1];
    start.path_x = {start_[0]};
    start.path_y = {start_[1]};

    goal.x = goal_[0];
    goal.y = goal_[1];

    min_point = min_max_points_[0];
    max_point = min_max_points_[1];

    max_len = max_len_;
    goal_sample_rate = goal_sample_rate_;
    max_iter = max_iter_;    
}

bool RRT::findPath(vector< vector<float> > &path_, vector<Node> &nodes_, MatrixXd obstacle_list){
    node_list.push_back(start);

    for(int i = 0; i < max_iter; i++){
        // generating a random node
        Node random_node = getRandomNode();
        // getting index of the nearest node
        int nearest_node_index = getNearestNodeIndex(random_node);
        // getting the node at the corresponding index
        Node nearest_node = node_list[nearest_node_index];
        // moving in the direction of the random node
        Node new_node = move(nearest_node, random_node);
        float dist = calcDist(random_node, nearest_node);

        if (dist > max_len){
            continue;
        }

        // check if collision occurs while moving
        if (checkCollision(new_node, obstacle_list)){
            // if not, add the node to the node list
            node_list.push_back(new_node);
            // check if the node is near the goal node
            if (calcDistToGoal(node_list[node_list.size() - 1].x, node_list[node_list.size() - 1].y) <= max_len){
                // if yes move towards the goal node
                Node final_node = move(node_list[node_list.size() - 1], goal);
                // check if collision occurs while moving
                if (checkCollision(final_node, obstacle_list)){
                    node_list.push_back(final_node);
                    // if not, get the final path and the nodes
                    path_ = generateFinalCourse(final_node);
                    nodes_ = node_list;

                    return true;
                }
            } 
        }
    }
    return false;
}

Node RRT::move(Node from_node, Node to_node){
    Node new_node(to_node.x, to_node.y);

    new_node.path_x = from_node.path_x;
    new_node.path_y = from_node.path_y;

    new_node.path_x.push_back(new_node.x);
    new_node.path_y.push_back(new_node.y);

    return new_node;
}

vector< vector<float> > RRT::generateFinalCourse(Node final_node){    
    vector< vector<float> > path;

    path.push_back(final_node.path_x);
    path.push_back(final_node.path_y);

    return path;
}

Node RRT::getRandomNode(){
    float a = min_point + (static_cast <float>(rand()) / (static_cast <float>(RAND_MAX) / (max_point - min_point)));
    float b = min_point + (static_cast <float>(rand()) / (static_cast <float>(RAND_MAX) / (max_point - min_point)));
    Node node(a, b);
    return node;
}

int RRT::getNearestNodeIndex(Node rnd){
    float val;
    float min_val = 200000.0;
    int min_ind;
    for (int i = 0; i < node_list.size(); i++){
        val = pow(node_list[i].x - rnd.x, 2) + pow(node_list[i].y - rnd.y, 2);
        if (min_val > val){
            min_val = val;
            min_ind = i; 
        }
    }
    return min_ind;
}

bool RRT::checkCollision(Node node, MatrixXd obstacle_list){
    float dx, dy, val, min_val = 2000000.0;

    // find the minimum squared distance of each to the obstacle center
    for (int i = 0; i < obstacle_list.col(0).size(); i++){
        for (int j = 0; j < node.path_x.size(); j++){
            dx = obstacle_list(i, 0) - node.path_x[j];
            dy = obstacle_list(i, 1) - node.path_y[j];
            val = pow(dx , 2) + pow(dy, 2);
            if (min_val > val){
                min_val = val;
            }
        }
    }

    // if the min distance sqaured is less than the sqaured of the radius of any
    // obstacle, then there is collision
    for (int i = 0; i < obstacle_list.col(0).size(); i++){
        if (min_val <= pow(obstacle_list(i, 2), 2)){
            return false;  // collision
        }
    }
    return true;  // no collision
}

float RRT::calcDistToGoal(float x, float y){
    float dx = x - goal.x;
    float dy = y - goal.y;

    return hypot(dx, dy);
}

float RRT::calcDist(Node from_node, Node to_node){
    float dx = to_node.x - from_node.x;
    float dy = to_node.y - from_node.y;

    return hypot(dx, dy);
}

float RRT::calcAngle(Node from_node, Node to_node){
    float dx = to_node.x - from_node.x;
    float dy = to_node.y - from_node.y;

    return atan2(dx, dy);
}


// -------------------------
#include <vector>
#include <box2d/box2d.h>

// Function to check if there is an obstacle between two points
bool IsObstacleInPath(const std::vector<b2AABB>& obstacles, const b2Vec2& from_, const b2Vec2& to_) {
    for (const auto& obstacle : obstacles) {
        // Check if the line segment intersects the AABB
        if (RayIntersectsAABB(obstacle, from_, to_)) {
            return true; // Obstacle found
        }
    }
    return false; // No obstacle in the path
}

// Helper function to check if a ray (line segment) intersects an AABB
bool RayIntersectsAABB(const b2AABB& aabb, const b2Vec2& from_, const b2Vec2& to_) {
    float tmin = (aabb.lowerBound.x - from_.x) / (to_.x - from_.x);
    float tmax = (aabb.upperBound.x - from_.x) / (to_.x - from_.x);

    if (tmin > tmax) std::swap(tmin, tmax);

    float tymin = (aabb.lowerBound.y - from_.y) / (to_.y - from_.y);
    float tymax = (aabb.upperBound.y - from_.y) / (to_.y - from_.y);

    if (tymin > tymax) std::swap(tymin, tymax);

    if ((tmin > tymax) || (tymin > tmax))
        return false;

    if (tymin > tmin)
        tmin = tymin;

    if (tymax < tmax)
        tmax = tymax;

    return (tmax >= 0);
}


//---------------------------
// A helper function to select an index based on weighted probabilities
int select_weighted_random(const std::vector<double>& weights) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::discrete_distribution<> dist(weights.begin(), weights.end());
    return dist(gen);
}

// Function to get the next manner based on the current temp_manner
std::string next_manner_from_TM(const std::string& temp_manner, const std::vector<std::string>& available_manners) {
    int index = select_weighted_random(TRANSITION_MATRIX[temp_manner]);
    std::string next_manner = ACTIVITY_NAMES[index];

    // Check if the selected manner is in the available manners
    if (std::find(available_manners.begin(), available_manners.end(), next_manner) == available_manners.end()) {
        // If not found, recursively call the function
        return next_manner_from_TM(temp_manner, available_manners);
    }

    return next_manner;
}