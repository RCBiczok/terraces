#pragma once
#include "types.h"

#include <iostream>
#include <iterator>
#include <gmpxx.h>

#define THRESHOLD 10

/* TODO UNUSED
template<typename T>
std::ostream& operator<<(std::ostream &strm, const std::set<T>& set) {
    strm << "{";
    bool first = true;
    for(const T &elem : set) {
        if(first) {
            first = false;
        } else {
            strm << ",";
        }
        strm << elem;
    }
    return strm << "}";
}

template<typename T>
std::ostream& operator<<(std::ostream &strm, const std::vector<T>& set) {
    strm << "[";
    bool first = true;
    for(const T &elem : set) {
        if(first) {
            first = false;
        } else {
            strm << ",";
        }
        strm << elem;
    }
    return strm << "]";
}

std::ostream& operator<<(std::ostream &strm, const constraint& tree);

std::ostream& operator<<(std::ostream &strm,
                         const std::vector<std::shared_ptr<std::set<leaf_number>> >& set);
*/

/**
 * Returns a vector containing all constraints that still are valid for the given set of leaves.
 *
 * @param leaves Leaves to search valid constraints for.
 * @param constraints All constraints that could still be valid.
 * @return All constraints that are still valid.
 */
std::vector<constraint> find_constraints(const LeafSet &leaves,
                                         const std::vector<constraint> &constraints);

static size_t binary_search(const std::vector<leaf_number> &vec, size_t start, size_t end, const leaf_number &key) {
        // Termination condition: start index greater than end index
        if(start > end) {
            return -1;
        }

        // Find the middle element of the vector and use that for splitting
        // the array into two pieces.
        const int middle = start + ((end - start) / 2);

        if(vec[middle] == key) {
            return middle;
        } else if(vec[middle] > key) {
            return binary_search(vec, start, middle - 1, key);
        }
        return binary_search(vec, middle + 1, end, key);
    }

static leaf_number map_to_new_leaf_number(const std::vector<leaf_number> &mapping, const leaf_number &leaf) {
    return binary_search(mapping, 0, mapping.size() - 1, leaf);
}

static std::vector<constraint> update_constraints(const std::vector<leaf_number> &mapping, const std::vector<constraint> &constraints) {
    std::vector<constraint> updated_constraints;
    // TODO: can mapping to new constraints be done more efficiently??
    for (size_t i = 0; i < constraints.size(); i++) {
        constraint new_constraint (map_to_new_leaf_number(mapping, constraints[i].smaller_left),
                                    map_to_new_leaf_number(mapping, constraints[i].smaller_right),
                                    map_to_new_leaf_number(mapping, constraints[i].bigger));
        updated_constraints.push_back(new_constraint);
    }
    return updated_constraints;
}

template <typename ResultType, typename CollectType = ResultType>
class TerraceAlgorithm {
public:
    virtual ~TerraceAlgorithm() = default;

    ResultType scan_terrace(LeafSet &leaves,
                            const std::vector<constraint> &constraints,
                            bool unrooted = false) {
        if (constraints.empty()) {
            return scan_unconstraint_leaves(leaves, unrooted);
        } else {
            leaves.apply_constraints(constraints);
            return traverse_partitions(constraints, leaves, unrooted);
        }
    }

protected:
    inline
    virtual ResultType traverse_partitions(const std::vector<constraint> &constraints,
                                           LeafSet &leaves,
                                           bool unrooted = false) {
        CollectType aggregation = initialize_collect_type();
        bool cont = true; // variable indicating whether to continue or not
        
        for (size_t i = 1; cont && i <= leaves.number_partition_tuples(); i++) {
            std::shared_ptr<LeafSet> part_left;
            std::shared_ptr<LeafSet> part_right;
            std::tie(part_left, part_right) = leaves.get_nth_partition_tuple(i);

            auto constraints_left = find_constraints(*part_left, constraints);
            auto constraints_right = find_constraints(*part_right, constraints);

            // compress left part?
            if(part_left->compressing_worth() && (constraints_left.size() > THRESHOLD)) {
                std::vector<leaf_number> mapping = part_left->compress();
                constraints_left = update_constraints(mapping, constraints_left);
            }
            // compress right part?
            if(part_right->compressing_worth() && (constraints_right.size() > THRESHOLD)) {
                std::vector<leaf_number> mapping = part_right->compress();
                constraints_right = update_constraints(mapping, constraints_right);
            }

            auto subtrees_left = scan_terrace(*part_left,
                                                       constraints_left);
            auto subtrees_right = scan_terrace(*part_right,
                                                        constraints_right);
            auto trees = combine_part_results(subtrees_left, subtrees_right);

            cont = combine_bipartition_results(aggregation, trees);
        }
        
        return finalize_collect_type(aggregation, unrooted);
    }

    virtual CollectType initialize_collect_type() = 0;
    virtual ResultType finalize_collect_type(CollectType &aggregation,
                                             bool unrooted = false) = 0;

    virtual ResultType scan_unconstraint_leaves(LeafSet &leaves,
                                                bool unrooted = false) = 0;

    virtual ResultType combine_part_results(const ResultType &left_part,
                                            const ResultType &right_part) = 0;

    virtual bool combine_bipartition_results(CollectType &aggregation,
                                             const ResultType &new_result) = 0;
};

typedef std::vector<InnerNodePtr> InnerNodeList;
class FindCompressedTree : public TerraceAlgorithm<Tree, InnerNodeList> {
protected:
    inline
    InnerNodeList initialize_collect_type() override {
        InnerNodeList aggregation;
        return aggregation;
    }

    inline
    Tree finalize_collect_type(InnerNodeList &aggregation,
                               bool unrooted = false) override {
        assert(aggregation.size() > 0);
        
        if (aggregation.size() == 1) {
            return aggregation[0];
            // One leaf can not be a root, no unrooted check required
            // (scan_unconstraint_leaves() would have been called)
        } else {
            AllTreeCombinationsNodePtr combi_node =
                    std::make_shared<AllTreeCombinationsNode>(aggregation);
            if (unrooted) {
                return std::make_shared<UnrootedNode>(combi_node);
            } else {
                return combi_node;
            }
        }
    }

    inline
    Tree scan_unconstraint_leaves(LeafSet &leaves,
                                  bool unrooted = false) override {
        // TODO reconsider the whole std::set usage vs std::vector
        auto set = leaves.to_set();
        std::vector<leaf_number> leaves_vec(set.begin(), set.end());
        const auto result =
                std::make_shared<AllLeafCombinationsNode>(leaves_vec);
        if(unrooted) {
            return std::make_shared<UnrootedNode>(result);
        } else {
            return result;
        }
    }

    inline
    Tree combine_part_results(const Tree &left_part,
                              const Tree &right_part) override {
        return std::make_shared<InnerNode>(left_part, right_part);
    }

    inline
    bool combine_bipartition_results(InnerNodeList &aggregation,
                                     const Tree &new_result) override {
        aggregation.push_back(std::static_pointer_cast<InnerNode>(new_result));
        return true;
    }
};

typedef std::vector<Tree> TreeList;
class FindAllRootedTrees : public TerraceAlgorithm<TreeList> {
protected:
    inline
    TreeList initialize_collect_type() override {
        return TreeList();
    }

    inline
    TreeList finalize_collect_type(TreeList &aggregation,
                                   bool unrooted = false) override {
        if(unrooted) {
            for (size_t i = 0; i < aggregation.size(); ++i) {
                // Guaranteed to work, otherwise no constraints would exist and
                // this method wouldn't get called.
                auto node = std::static_pointer_cast<InnerNode>(aggregation[i]);
                aggregation[i] = std::make_shared<UnrootedNode>(node);
            }
        }
        return aggregation;
    }

    inline
    TreeList scan_unconstraint_leaves(
            LeafSet &leaves, bool unrooted = false) override {
        TreeList binary_trees = get_all_binary_trees(leaves);

        if(unrooted) {
            for (size_t i = 0; i < binary_trees.size() ; ++i) {
                const auto inner =
                    std::static_pointer_cast<InnerNode>(binary_trees[i]);
                binary_trees[i] = std::make_shared<UnrootedNode>(inner);
            }
        }

        return binary_trees;
    }

    inline
    TreeList combine_part_results(const TreeList &left_part,
                                  const TreeList &right_part) override {
        return merge_subtrees(left_part, right_part);
    }

    inline
    bool combine_bipartition_results(TreeList &aggregation,
                                     const TreeList &new_result) override {
        aggregation.insert(aggregation.end(), new_result.begin(),
                           new_result.end());
        return true;
    }
public:
    /**
     * Returns all possible binary trees that can be combined by using the
     * leaves in the LeafSet.
     */
    static TreeList get_all_binary_trees(LeafSet &leaves);

    /**
     * Adds a leaf to a tree at all possible positions, leading to a list of
     * resulting trees.
     */
    static TreeList add_leaf_to_tree(const Tree &current_tree,
                                     const LeafPtr &leaf);

    /** merges two sub-trees */
    static TreeList merge_subtrees(const TreeList &left, const TreeList &right);
};

class CountAllRootedTrees : public TerraceAlgorithm<mpz_class> {
protected:
    inline
    mpz_class initialize_collect_type() override {
        return mpz_class(0);
    }

    inline
    mpz_class finalize_collect_type(mpz_class &aggregation,
                                    bool unrooted = false) override {
        return aggregation;
    }

    inline
    mpz_class scan_unconstraint_leaves(LeafSet &leaves,
                                       bool unrooted = false) override {
        return Node::number_of_binary_trees(leaves.size());
    }

    inline
    mpz_class combine_part_results(const mpz_class &left_part,
                                   const mpz_class &right_part) override {
        return left_part * right_part;
    }

    inline
    bool combine_bipartition_results(mpz_class &aggregation,
                                     const mpz_class &new_result) override {
        aggregation += new_result;
        return true;
    }
};

class CheckIfTerrace : public TerraceAlgorithm<bool> {
protected:
    inline
    bool traverse_partitions(const std::vector<constraint> &constraints,
                             LeafSet &leaves,
                             bool unrooted = false) override {
        if(leaves.number_partition_tuples() > 1) {
            return true;
        }
        return TerraceAlgorithm<bool>::traverse_partitions(constraints, leaves);
    }

    inline
    bool initialize_collect_type() override {
        return false;
    }

    inline
    bool finalize_collect_type(bool &aggregation,
                               bool unrooted = false) override {
        return aggregation;
    }

    inline
    bool scan_unconstraint_leaves(LeafSet &leaves, bool unrooted = false) override {
        return leaves.size() >= 3;
    }

    inline
    bool combine_part_results(const bool &left_part, const bool &right_part) override {
        return left_part || right_part;
    }

    inline
    bool combine_bipartition_results(bool &aggregation,
                                     const bool &new_result) override {
        aggregation |= new_result;
        return aggregation;
    }
};
