/**
 @file
 @brief terraces framework and interface definition by Alexandros Stamatakis released under GNU GPL
 Alexandros.Stamatakis@gmail.com
 */

#include "input_parser.h"
#include "terraces.h"
#include "util.h"
#include "debug.h"

#include <assert.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <gmp.h>
#include <terraces.h>

int terraceAnalysis(missingData *m,
                    const char *newickTreeString,
                    const int ta_outspec,
                    FILE *allTreesOnTerrace,
                    mpz_t *terraceSize) {
    mpz_set_ui(*terraceSize, 0);

    //some debugging print outs

    //dout("read_tree = " << newickTreeString << std::endl);
    //dout("missing_data = " << std::endl << *m);


    //dout("\n" << missing_data_to_nexus(m) << "\n");

    // figure out what we are supposed to calculate

    const bool countTrees = (ta_outspec & TA_COUNT) != 0;
    const bool enumerateTrees = (ta_outspec & TA_ENUMERATE) != 0;
    const bool treeIsOnTerrace = (ta_outspec & TA_DETECT) != 0;
    const bool enumerateCompressedTrees = (ta_outspec & TA_ENUMERATE_COMPRESS)
                                          != 0;
    /* some basic error checking, students, please extend this, see error codes at the end of this function */

    assert(m->numberOfSpecies > 3 && m->numberOfPartitions > 1);

    for (size_t i = 0; i < m->numberOfSpecies; i++) {
        for (size_t j = 0; j < m->numberOfPartitions; j++) {
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

    ntree_t *tree = get_newk_tree_from_string(newickTreeString);

    assert(tree != nullptr);

    std::string root_species_name;
    std::vector<std::string> id_to_lable;
    std::shared_ptr<Tree> rtree = root_tree(tree, m, root_species_name, id_to_lable);

    //dout(newickTreeString << "\n");

    assert(rtree != nullptr);
    //dout("rooted_tree = " << rtree->to_newick_string(id_to_lable) << "\n");

    std::set<std::string> m_labels;
    for (size_t k = 0; k < m->numberOfSpecies; k++) {
        bool noData = true;
        for (size_t j = 0; j < m->numberOfPartitions; j++) {
            if (getDataMatrix(m, k, j) == static_cast<unsigned char>(1)) {
                noData = false;
                break;
            }
        }
        if(noData) {
            dout("Species with no data in any partition: " << m->speciesNames[k] << "\n");
            assert(0);
        }
        m_labels.insert(std::string(m->speciesNames[k]));
    }

    std::map<std::string, bool> in_data_file;
    for (size_t i = 0; i < id_to_lable.size(); i++) {
        if (m_labels.count(std::string(id_to_lable[i])) == 0) {
            dout("Species appears in newick file, but not in missing data file:" << id_to_lable[i] << "\n");
            assert(0);
        } else {
            in_data_file[*m_labels.find(std::string(id_to_lable[i]))] = true;
        }
    }

    in_data_file[root_species_name] = true;
    for (auto &m_label : m_labels) {
        if(!in_data_file[m_label]) {
            dout("Species appears in missing data file, but not in newick file:" << m_label << "\n");
            assert(0);
        }
    }

    auto constraints = extract_constraints_from_supertree(rtree, m, id_to_lable);
    //dout(constraints << "\n");
    //dout(extract_constraints_from_supertree(__convert(tree), m) << "\n");

    //auto r = find_all_rooted_trees(leafs,
    //                               extract_constraints_from_supertree(rtree, m));
    //dout("===== TREES: " << r.size() << "\n");

    auto leaves = LeafSet::create(id_to_lable.size());

    size_t count = 0;
    if(countTrees) {
        CountAllRootedTrees algo;
        count = algo.scan_terrace(leaves, constraints);
    } else if(treeIsOnTerrace) {
        CheckIfTerrace algo;
        count = algo.scan_terrace(leaves, constraints) ? 2 : 0;
    } else if (enumerateTrees) {
        FindAllRootedTrees algo;
        auto all_trees = algo.scan_terrace(leaves, constraints);
        count = all_trees.size();
        for (std::shared_ptr<Tree> t : all_trees) {
            fprintf(allTreesOnTerrace, "%s\n", t->to_newick_string(id_to_lable, root_species_name).c_str());
        }
    }

    mpz_set_ui(*terraceSize, count);

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

    ntree_destroy(tree);
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

    if (speciesNames != nullptr) {
        m->speciesNames = const_cast<char **>(speciesNames);
        m->allocatedNameArray = 0;
    }
        //otherwise, we assume that species names are just indexes
    else {
    //if the species names have been passed by the application programmer just set a pointer to them
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
    delete [] m->missingDataMatrix;

    if (m->allocatedNameArray == 1) {
        size_t i;
        for (i = 0; i < m->numberOfSpecies; i++)
            delete [] m->speciesNames[i];
        delete [] m->speciesNames;
    }

    delete m;
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

/* get an element from the missing data matrix */

unsigned char getDataMatrix(const missingData *m, size_t speciesNumber,
                            size_t partitionNumber) {
    assert(speciesNumber < m->numberOfSpecies);
    assert(speciesNumber >= 0);

    assert(partitionNumber < m->numberOfPartitions);
    assert(partitionNumber >= 0);

    unsigned char value = m->missingDataMatrix[speciesNumber
                                               * m->numberOfPartitions + partitionNumber];
    assert(value == 0 || value == 1);

    return value;
}

std::vector<constraint> extract_constraints_from_supertree(
        const std::shared_ptr<Tree> supertree,
        const missingData *missing_data,
        const std::vector<std::string> &id_to_label) {

    std::map<std::string, leaf_number> species_map;
    for (leaf_number i = 0; i < missing_data->numberOfSpecies; i++) {
        species_map[std::string(missing_data->speciesNames[i])] = i;
    }

    std::map<std::string, constraint> constraint_map;

    for (size_t i = 0; i < missing_data->numberOfPartitions; i++) {
        auto partition = generate_induced_tree(supertree, missing_data,
                                               species_map, id_to_label, i);

        auto constraints = extract_constraints_from_tree(partition);
        //dout(partition << "\n");
        //dout(constraints << "\n");

        for (auto &c : constraints) {
            //avoid duplications
            std::string key = std::to_string(c.smaller_left) + std::to_string(c.smaller_right) + "__" +
                              std::to_string(c.bigger_left) + std::to_string(c.bigger_right);
            if (constraint_map.count(key) == 0) {
                constraint_map[key] = c;
            }
        }
    }

    std::vector<constraint> result;
    for (auto &e : constraint_map) {
        result.push_back(e.second);
    }
    //dout(result << "\n");

    return result;
}

std::ostream& operator<<(std::ostream &strm, const missingData& m) {
    for (size_t i = 0; i < m.numberOfSpecies; i++) {
        for (size_t j = 0; j < m.numberOfPartitions; j++) {
            strm << +getDataMatrix(&m, i, j) << " ";
        }
        strm << m.speciesNames[i];
        strm << std::endl;
    }
    return strm;
}
