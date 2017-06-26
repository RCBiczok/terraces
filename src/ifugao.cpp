#include "ifugao.h"
#include "debug.h"

#include <assert.h>
#include <terraces.h>
#include <map>
#include <sstream>
#include <iomanip>

std::tuple<std::shared_ptr<std::set<leaf_label> >,
        std::shared_ptr<std::set<leaf_label> > > get_nth_partition_tuple(
        std::vector<std::shared_ptr<std::set<leaf_label> > > &partitions,
        size_t n) {

    auto part_one = std::make_shared<std::set<leaf_label> >();
    auto part_two = std::make_shared<std::set<leaf_label> >();

    assert(n > 0 && n <= number_partition_tuples(partitions));

    for (size_t i = 0; i < partitions.size(); i++) {
        if (is_bit_set(n, i)) {
            part_one->insert(partitions[i]->begin(), partitions[i]->end());
        } else {
            part_two->insert(partitions[i]->begin(), partitions[i]->end());
        }
    }

    return std::make_tuple(part_one, part_two);
}

static std::shared_ptr<Tree> root(std::shared_ptr<Tree> t) {
    if (t->parent == nullptr) {
        return t;
    }

    return root(t->parent);
}

static std::vector<std::shared_ptr<Tree> > add_leaf_to_tree(
        std::shared_ptr<Tree> current_tree, const leaf_label leaf) {

    std::vector<std::shared_ptr<Tree> > result;

    if (!current_tree->is_leaf()) {
        auto result_left = add_leaf_to_tree(current_tree->left, leaf);
        result.insert(result.end(), result_left.begin(), result_left.end());
        auto result_right = add_leaf_to_tree(current_tree->right, leaf);
        result.insert(result.end(), result_right.begin(), result_right.end());
    }

    auto tree_copy = deep_copy(current_tree);

    auto to_insert = std::make_shared<Tree>(leaf);
    auto new_tree = std::make_shared<Tree>(tree_copy, to_insert);
    new_tree->parent = tree_copy->parent;
    if (tree_copy->parent != nullptr) {
        if (tree_copy->parent->left == tree_copy) {
            tree_copy->parent->left = new_tree;
        } else {
            tree_copy->parent->right = new_tree;
        }
    }
    tree_copy->parent = new_tree;
    to_insert->parent = new_tree;

    result.push_back(root(new_tree));

    return result;
}

std::vector<std::shared_ptr<Tree> > get_all_binary_trees(
        const std::set<leaf_label> &leafs) {

    std::vector<std::shared_ptr<Tree> > result;
    if (leafs.size() == 0) {
        return result;
    }

    auto itr = leafs.begin();
    leaf_label next_leaf = *itr;
    itr++;
    std::set<leaf_label> rest_leafs(itr, leafs.end());

    if (leafs.size() == 1) {
        auto t = std::make_shared<Tree>(next_leaf);
        result.push_back(t);
        return result;
    }

    for (auto t : get_all_binary_trees(rest_leafs)) {
        auto new_trees = add_leaf_to_tree(t, next_leaf);
        result.insert(result.end(), new_trees.begin(), new_trees.end());
    }

    return result;
}

std::vector<std::shared_ptr<UnrootedTree> > find_all_unrooted_trees(
        const std::set<leaf_label> &leafs,
        const std::vector<constraint> &constraints,
        const leaf_label &root_species_name) {

    assert(leafs.count(root_species_name) > 0);

    auto part_left = std::make_shared<std::set<leaf_label> >();
    auto part_right = std::make_shared<std::set<leaf_label> >();

    part_left->insert(root_species_name);
    part_right->insert(leafs.begin(), leafs.end());
    part_right->erase(root_species_name);

    auto constraints_left = find_constraints(*part_left, constraints);
    auto constraints_right = find_constraints(*part_right, constraints);

    auto subtrees_left = find_all_rooted_trees(*part_left, constraints_left);
    auto subtrees_right = find_all_rooted_trees(*part_right, constraints_right);
    auto trees = merge_subtrees(subtrees_left, subtrees_right);

    std::vector<std::shared_ptr<UnrootedTree> > result;
    result.reserve(trees.size());
    for (auto &t : trees) {
        result.push_back(std::make_shared<UnrootedTree>(t));
    }

    return result;
}

//TODO: structure is more or less duplicated. from find_all_rooted_trees
size_t count_all_rooted_trees(const std::set<leaf_label> &leaves,
                              const std::vector<constraint> &constraints) {
    if (constraints.empty()) {
        size_t result = 1;
        for(size_t i = 4; i <= (leaves.size() + 1); i++) {
            result *= (2*i-5);
        }
        return result;
    }

    auto partitions = apply_constraints(leaves, constraints);
    size_t result = 0;

    //std::cerr << "<itr>\n";
    //std::cerr << "<leafs val=\"" << leafs << "\"/>\n";
    //std::cerr << "<constraints val=\"" << constraints << "\"/>\n";
    //std::cerr << "<partitions val=\"" << partitions << "\"/>\n";

    for (size_t i = 1; i <= number_partition_tuples(partitions); i++) {
        std::shared_ptr<std::set<leaf_label> > part_left;
        std::shared_ptr<std::set<leaf_label> > part_right;
        std::tie(part_left, part_right) = get_nth_partition_tuple(partitions, i);

        //std::cerr << "<bipartition l=\"" << *part_left << "\" r=\"" << *part_right << "\">\n";

        auto constraints_left = find_constraints(*part_left, constraints);
        auto constraints_right = find_constraints(*part_right, constraints);

        auto subtrees_left = count_all_rooted_trees(*part_left, constraints_left);
        auto subtrees_right = count_all_rooted_trees(*part_right, constraints_right);
        auto trees = subtrees_left * subtrees_right;

        //std::cerr << "<result count=\"" << trees.size() << "\"/>\n";
        //std::cerr << "</bipartition>\n";

        result += trees;
    }

    //std::cerr << "<result count=\"" << result.size() << "\"/>\n";
    //std::cerr << "</itr>\n";

    return result;
}

std::vector<std::shared_ptr<Tree> > find_all_rooted_trees(
        const std::set<leaf_label> &leafs,
        const std::vector<constraint> &constraints) {

    if (constraints.empty()) {
        auto result = get_all_binary_trees(leafs);
        //std::cerr << "<itr>\n";
        //std::cerr << "<leafs val=\"" << leafs << "\"/>\n";
        //std::cerr << "<result count=\"" << result.size() << "\"/>\n";
        //std::cerr << "</itr>\n";
        return result;
    }

    auto partitions = apply_constraints(leafs, constraints);
    std::vector<std::shared_ptr<Tree> > result;

    //std::cerr << "<itr>\n";
    //std::cerr << "<leafs val=\"" << leafs << "\"/>\n";
    //std::cerr << "<constraints val=\"" << constraints << "\"/>\n";
    //std::cerr << "<partitions val=\"" << partitions << "\"/>\n";

    for (size_t i = 1; i <= number_partition_tuples(partitions); i++) {
        std::shared_ptr<std::set<leaf_label> > part_left;
        std::shared_ptr<std::set<leaf_label> > part_right;
        std::tie(part_left, part_right) = get_nth_partition_tuple(partitions, i);

        //std::cerr << "<bipartition l=\"" << *part_left << "\" r=\"" << *part_right << "\">\n";

        auto constraints_left = find_constraints(*part_left, constraints);
        auto constraints_right = find_constraints(*part_right, constraints);

        auto subtrees_left = find_all_rooted_trees(*part_left, constraints_left);
        auto subtrees_right = find_all_rooted_trees(*part_right, constraints_right);
        auto trees = merge_subtrees(subtrees_left, subtrees_right);

        //std::cerr << "<result count=\"" << trees.size() << "\"/>\n";
        //std::cerr << "</bipartition>\n";

        result.insert(result.end(), trees.begin(), trees.end());
    }

    //std::cerr << "<result count=\"" << result.size() << "\"/>\n";
    //std::cerr << "</itr>\n";

    return result;
}

std::vector<std::shared_ptr<Tree> > merge_subtrees(
        std::vector<std::shared_ptr<Tree> > &left,
        std::vector<std::shared_ptr<Tree> > &right) {

    std::vector<std::shared_ptr<Tree> > merged_trees;

    for (std::shared_ptr<Tree> &l : left) {
        for (std::shared_ptr<Tree> &r : right) {
            auto new_tree = std::make_shared<Tree>(l, r);
            l->parent = new_tree;
            r->parent = new_tree;
            merged_trees.push_back(new_tree);
        }
    }

    assert(merged_trees.size() == left.size() * right.size());
    return merged_trees;
}

std::vector<std::shared_ptr<std::set<leaf_label> > > apply_constraints(
        const std::set<leaf_label> &leaves,
        const std::vector<constraint> &constraints) {

    std::vector<std::shared_ptr<std::set<leaf_label> > > sets;

    for (leaf_label l : leaves) {
        // create an empty set for each leave
        std::shared_ptr<std::set<leaf_label>> set(new std::set<leaf_label>);
        set->insert(l);
        sets.push_back(set);
    }

    bool found_left_constraint = false;
    bool found_right_constraint = false;
    size_t index_containing_left_constraint = 0;
    size_t index_containing_right_constraint = 0;

    for (constraint cons : constraints) {
        for (size_t i = 0; i < sets.size(); i++) {
            if (sets[i]->find(cons.smaller_left) != sets[i]->end()) {
                // set contains the left constraint
                found_left_constraint = true;
                index_containing_left_constraint = i;
            }
            if (sets[i]->find(cons.smaller_right) != sets[i]->end()) {
                // set contains the right constraint
                found_right_constraint = true;
                index_containing_right_constraint = i;
            }
        }
        assert(
                found_left_constraint
                && index_containing_left_constraint < sets.size());
        assert(
                found_right_constraint
                && index_containing_right_constraint < sets.size());
        if (index_containing_left_constraint
            != index_containing_right_constraint) {
            // sets need to be merged
            sets[index_containing_left_constraint]->insert(
                    sets[index_containing_right_constraint]->begin(),
                    sets[index_containing_right_constraint]->end());
            sets.erase(sets.begin() + static_cast<decltype(sets)::difference_type>(index_containing_right_constraint));
        }
    }
    return sets;
}

static std::tuple<leaf_label, leaf_label> extract_constraints_from_tree_rec(
        const std::shared_ptr<Tree> node,
        std::vector<constraint> &constraints) {

    assert(node != nullptr);

    if (node->is_leaf()) {
        auto this_leaf = node->label;
        return std::make_tuple(this_leaf, this_leaf);
    }

    // (l,r) of the left child node
    leaf_label l_left_most;
    leaf_label l_right_most;
    std::tie(l_left_most, l_right_most) = extract_constraints_from_tree_rec(
            node->left, constraints);

    // (l,r) of the right child node
    leaf_label r_left_most;
    leaf_label r_right_most;
    std::tie(r_left_most, r_right_most) = extract_constraints_from_tree_rec(
            node->right, constraints);

    if (l_left_most != l_right_most) {
        constraint c;
        c.smaller_left = l_left_most;
        c.smaller_right = l_right_most;
        c.bigger_left = l_left_most;
        c.bigger_right = r_right_most;
        constraints.push_back(c);
    }

    if (r_left_most != r_right_most) {
        constraint c;
        c.smaller_left = r_left_most;
        c.smaller_right = r_right_most;
        c.bigger_left = l_left_most;
        c.bigger_right = r_right_most;
        constraints.push_back(c);
    }

    return std::make_tuple(l_left_most, r_right_most);
}

std::set<leaf_label> extract_leaf_labels_from_supertree(
        std::shared_ptr<Tree> tree) {

    std::set<leaf_label> result;

    if (tree == nullptr) {
        return result;
    }

    auto result_left = extract_leaf_labels_from_supertree(tree->left);
    result.insert(result_left.begin(), result_left.end());
    auto result_right = extract_leaf_labels_from_supertree(tree->right);
    result.insert(result_right.begin(), result_right.end());

    if (tree->is_leaf()) {
        result.insert(tree->label);
    }

    return result;
}

std::vector<constraint> extract_constraints_from_tree(
        const std::shared_ptr<Tree> supertree) {
    std::vector<constraint> constraints;

    extract_constraints_from_tree_rec(supertree, constraints);

    return constraints;
}

std::vector<constraint> find_constraints(const std::set<leaf_label> &leaves,
                                         const std::vector<constraint> &constraints) {

    std::vector<constraint> valid_constraints;

    for (constraint cons : constraints) {
        if (cons.smaller_left == cons.bigger_left) {
            if (leaves.find(cons.smaller_left) != leaves.end()
                && leaves.find(cons.smaller_right) != leaves.end()
                && leaves.find(cons.bigger_right) != leaves.end()) {
                // constraint is valid on leaf set
                valid_constraints.push_back(cons);
            }
        } else {
            // smaller_right == bigger_right
            if (leaves.find(cons.smaller_left) != leaves.end()
                && leaves.find(cons.smaller_right) != leaves.end()
                && leaves.find(cons.bigger_left) != leaves.end()) {
                // constraint is valid on leaf set
                valid_constraints.push_back(cons);
            }
        }
    }
    return valid_constraints;
}

std::string missing_data_to_nexus(const missingData* m) {

    std::stringstream ss;
    size_t max_len = 0;
    for (size_t i = 0; i < m->numberOfSpecies; i++) {
        if(strlen(m->speciesNames[i]) > max_len) {
            max_len = strlen(m->speciesNames[i]);
        }
    }

    ss << "#NEXUS\n";
    ss << "Begin data;\n";
    ss << "    Dimensions ntax=" << m->numberOfSpecies << " nchar=" << m->numberOfPartitions << ";\n";
    ss << "    Format datatype=dna gap=-;\n";
    ss << "    Matrix\n";
    for (size_t i = 0; i < m->numberOfSpecies; i++) {
        ss << std::left << std::setw(static_cast<int>(max_len+1)) << m->speciesNames[i];
        for (size_t j = 0; j < m->numberOfPartitions; j++) {
            ss << (getDataMatrix(m, i, j)>0 ? 'A' : '-');
        }
        ss << std::endl;
    }
    ss << "\t;\n";
    ss << "End;\n";
    ss << "\n";

    ss << "BEGIN SETS;\n";
    for (size_t j = 0; j < m->numberOfPartitions; j++) {
        ss << "\tCHARSET  P" << std::to_string(j+1) << " = "
             << std::to_string(j+1) << "-" << std::to_string(j+1) << ";\n";
    }
    ss << "END;\n";
    return ss.str();
}

std::ostream& operator<<(std::ostream &strm,
                         const std::vector<std::shared_ptr<std::set<leaf_label>> >& set) {
    strm << "[";
    bool first = true;
    for(size_t i = 0; i < set.size(); i++) {
        if(first) {
            first = false;
        } else {
            strm << ",";
        }

        strm << "{";
        bool first2 = true;
        for(const auto &e : *set[i]) {
            if(first2) {
                first2 = false;
            } else {
                strm << ",";
            }
            strm << e;
        }
        strm << "}";
    }
    return strm << "]";
}

std::ostream& operator<<(std::ostream &strm, const constraint& c) {
    return strm << "lca(" << c.smaller_left << "," << c.smaller_right
                << ") < lca(" << c.bigger_left << "," << c.bigger_right << ")";
}
