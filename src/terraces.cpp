/**
 @file
 @brief terraces framework and interface definition by Alexandros Stamatakis released under GNU GPL
 Alexandros.Stamatakis@gmail.com
 */

#include "terraces.h"

#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <gmp.h>

int terraceAnalysis(missingData *m,
                    const char *newickTreeString,
                    const int ta_outspec,
                    FILE *allTreesOnTerrace,
                    mpz_t *terraceSize) {
    size_t i = 0;
    size_t j = 0;

    mpz_set_ui(*terraceSize, 0); // gmp library call

    //some debugging print outs
    d_printf("read_tree = %s\n", newickTreeString);

    // variables to figure out what we are supposed to calculate
    //TODO Unused variables: countTrees, treeIsOnTerrace
    //const bool countTrees = (ta_outspec & TA_COUNT) != 0;
    //const bool treeIsOnTerrace = (ta_outspec & TA_DETECT) != 0;
    const bool enumerateTrees = (ta_outspec & TA_ENUMERATE) != 0;
    const bool enumerateCompressedTrees = (ta_outspec & TA_ENUMERATE_COMPRESS)
                                          != 0;

    {// some basic error checking
        //TODO extend this, see error codes at the end of this function
        if(m->numberOfSpecies < 4) {
            return TERRACE_NOT_ENOUGH_SPECIES;
        }
        if(m->numberOfPartitions > 1) {
        
            return TERRACE_NUM_PARTITIONS_ERROR;
        }
        for (i = 0; i < m->numberOfSpecies; i++) {
            for (j = 0; j < m->numberOfPartitions; j++) {
                if (!(getDataMatrix(m, i, j) == static_cast<unsigned char>(0)
                      || getDataMatrix(m, i, j) == static_cast<unsigned char>(1))) {
                    return TERRACE_MATRIX_ERROR;
                }
            }
        }

        if ((enumerateTrees || enumerateCompressedTrees)
            && allTreesOnTerrace == nullptr) {
            return TERRACE_OUTPUT_FILE_ERROR;
        }
    }

    // mapping of leaf IDs to their labels (availably static)
    LeafLabelMapper::species_names = m->speciesNames;
    // mapping of labels to leaf IDs
    for (size_t i = 0; i < m->numberOfSpecies; i++) {
        LeafLabelMapper::label_to_leaf_id[m->speciesNames[i]] = i;
    }
    
    ntree_t *nwk_tree = get_newk_tree_from_string(newickTreeString);
    assert(nwk_tree != nullptr);

    size_t root_species_id;
    //TODO bool return value, rtree as out parameter
    Tree rtree = root_tree(nwk_tree, m, root_species_id);
    if(rtree == nullptr) {
        return TERRACE_NO_SPECIES_WITH_FULL_SET;
    }
    // nwk_tree is no longer needed
    ntree_destroy(nwk_tree);
    
    std::vector<constraint> constraints =
            extract_constraints_from_supertree(rtree, m);
    
    BitLeafSet leaf_set(m->numberOfSpecies);
    
    size_t num_trees = list_trees(constraints, root_species_id, leaf_set,
                                  allTreesOnTerrace);
    mpz_set_ui(*terraceSize, num_trees);

    /* e.g., include an error check to make sure the Newick tree you have parsed contains as many species as indicated by numberOfSpecies */

    /*
     the function shall write to variable terraceSize
     1. the number of UNROOTED trees on the terrace
     2. or just some value > 1 in terraceSize, if we only wanted to know if the tree is on a terrace
     */

    /*
     the return value is an error code
     we will define these together as the project proceeds, e.g.
     0:  succesfull completion
     -1: problem parsing Newick file
     -2: #species in Newick file does not correspond to number of species in data matrix
     -3: entries in data matrix not either 0 or 1
     -4: less than 4 spcies in input tree
     -5: only one partition in data matrix
     -6: reserved for something you must think about anyway (tree can't be rooted)
     -7: no output file specified
     */
    return TERRACE_SUCCESS;
}

/************************************************************************************************************************/

/* functions for handling the missing data data structure */

/* allocate and initialize */

missingData *initializeMissingData(size_t numberOfSpecies,
                                   size_t numberOfPartitions, const char **speciesNames) {
    missingData *m = new missingData();

    m->numberOfSpecies = numberOfSpecies;

    m->numberOfPartitions = numberOfPartitions;

    //if the species names have been passed by the application programmer just set a pointer to them
    if (speciesNames != nullptr) {
        m->speciesNames = const_cast<char **>(speciesNames);
        m->allocatedNameArray = 0;
    }
    //otherwise, we assume that species names are just indexes
    else {
        m->allocatedNameArray = 1;
        m->speciesNames = new char*[numberOfSpecies];

        size_t i;

        char buffer[1024];

        for (i = 0; i < numberOfSpecies; i++) {
            sprintf(buffer, "%zu", i);

            size_t n = strlen(buffer);

            m->speciesNames[i] = new char[n + 1];

            strcpy(m->speciesNames[i], buffer);
        }
    }

    m->missingDataMatrix = new unsigned char [
             numberOfSpecies * numberOfPartitions];

    return m;
}

/* free */

void freeMissingData(missingData *m) {
    free(m->missingDataMatrix);

    if (m->allocatedNameArray == 1) {
        size_t i;
        for (i = 0; i < m->numberOfSpecies; i++)
            free(m->speciesNames[i]);
        free(m->speciesNames);
    }

    free(m);
}

/* set an element in the missing data matrix */

void setDataMatrix(missingData *m, size_t speciesNumber, size_t partitionNumber,
                   unsigned char value) {
    assert(speciesNumber < m->numberOfSpecies);
    assert(speciesNumber >= 0);

    assert(partitionNumber < m->numberOfPartitions);
    assert(partitionNumber >= 0);

    assert(value == 0 || value == 1);

    m->missingDataMatrix[speciesNumber * m->numberOfPartitions + partitionNumber] =
            value;
}

void copyDataMatrix(const unsigned char *matrix, missingData *m) {
    memcpy(m->missingDataMatrix, matrix,
           m->numberOfPartitions * m->numberOfSpecies);
}

std::vector<constraint> extract_constraints_from_supertree(
        const Tree supertree,
        const missingData *missing_data) {
    // map all found constraints, so there will be no duplicates
    typedef std::tuple<size_t, size_t, size_t, size_t> constraint_key;
    std::map<constraint_key, constraint> constraint_map;
    
    // foreach partition
    for (size_t i = 0; i < missing_data->numberOfPartitions; i++) {
        auto tree = generate_induced_tree(supertree, missing_data, i);
        auto constraints = tree->extract_constraints();
        for (auto &c : constraints) {
            //avoid duplications
            constraint_key key(c.smaller_left, c.smaller_right, c.bigger_left,
                               c.bigger_right);
            if (constraint_map.count(key) == 0) {
                constraint_map[key] = c;
            }
        }
    }
    
    // TODO faster method?
    std::vector<constraint> result;
    for (auto &e : constraint_map) {
        result.push_back(e.second);
    }

    return result;
}
#ifndef DEBUG

static void d_print_tree_rec(const ntree_t *tree, int depth) {
    for (int j = 0; j < depth * 4; j++) {
        fprintf(stderr, " ");
    }
    fprintf(stderr, "<node label=\"%s\">", tree->label);
    if(tree->children_count > 0) {
        fprintf(stderr, "\n");
    }
    for (int i = 0; i < tree->children_count; i++) {
        d_print_tree_rec(tree->children[i], depth + 1);
    }
    if(tree->children_count > 0) {
        for (int j = 0; j < depth * 4; j++) {
            fprintf(stderr, " ");
        }
    }
    fprintf(stderr, "<node/>\n");
}

void d_print_tree_impl(const char* file, const int line, const ntree_t *tree) {
    fprintf(stderr, "DEBUG(%s, %d): Dump ntree_t:\n", file, line);
    d_print_tree_rec(tree, 1);
}
#endif /* DEBUG */

/* TODO rewrite if still desired
static void d_print_tree_rec(const Tree tree, int depth) {
    fprintf(stderr, "Label: %s\n", tree->label.c_str());
    assert(depth == 1
           || (tree->parent != nullptr
                && (tree->parent->left == tree
                    || tree->parent->right == tree)));
    if (tree->left != nullptr) {
        for (int j = 0; j < depth * 4; j++) {
            fprintf(stderr, " ");
        }
        fprintf(stderr, "L:");
        d_print_tree_rec(tree->left, depth + 1);
    }
    if (tree->right != nullptr) {
        for (int j = 0; j < depth * 4; j++) {
            fprintf(stderr, " ");
        }
        fprintf(stderr, "R:");
        d_print_tree_rec(tree->right, depth + 1);
    }
}

void d_print_tree_impl(const char* file, const int line, const Tree tree) {
    fprintf(stderr, "DEBUG(%s, %d): Dump Tree:\n", file, line);
    d_print_tree_rec(tree, 1);
}
*/
