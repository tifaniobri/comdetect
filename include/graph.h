#define _GNU_SOURCE

/********************************************************************/
/*                            INCLUDES                               */
/********************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <search.h>

#include "queue.h"
#include "vector.h"
#include "util.h"
#include "edges.h"
#include "wqupc.h"

/********************************************************************/
/*                       STRUCTS AND PROTOTYPES                     */
/********************************************************************/

// struct to hold user input arguments
typedef struct {

    char infile[200];
    int num_clusters;
    char outfile[200];
    float sample_rate;

} InputArgs;

///////////////////////////////////////
// GRAPH FUNCTIONS

// Sparse undirected graph.
// Utilizes a non-symmetric CSR format for compression In other words,
// maintain a duplicate copy of each edge in order to facilitate quick
// lookup of neighbors.
//
// Since an undirected graph has symmetric edges, we normally only store
// each once. However, storing each edge as (i, j) and (j, i) allows
// faster lookup of neighbors. If we stored each edge once, and we ask
// who are the neighbors of i, we must look at i's edgelist, and also
// every other node's edgelist to see if i is present in it. If we
// duplicate the edges, then we must only check i's edgelist.
typedef struct {

    int n;          // number of nodes: |V|
    int m;          // number of edges: |E|
    int n_s;        // number of nodes in sample

    int *id;          // size = |V|; ids for all nodes
    int *index;       // size = |V| + 1
    int *edges;       // size = 2|E|
    int *edge_id;     // size = 2|E|
    float *edge_bet;  // size = |E|; index corresponds to edge id

    int *degree;      // size = |V|
    int *node_id;     // size = |V|
    int *sample;      // size = user specified at run time

    IdmapStorage idmap;                // hash table entries for node id map
    IdmapStorage eidmap_store;
    struct hsearch_data edge_idmap;    // map from "i j" pair to edge id

} SparseUGraph;


// Compress edges from edge list into a compressed row storage (CRS) format
void rowCompressEdges(EdgeList *elist, SparseUGraph *graph);

// read a sparse undirected graph from an edgelist file
void readSparseUGraph(InputArgs *args, SparseUGraph *graph);

// free all memory allocated for sparse undirected graph
void freeSparseUGraph(SparseUGraph *graph);

// store the id list for the graph and free the space it occupied
void storeAndFreeNodeIds(SparseUGraph *graph);

// calculate the degree of all nodes in the graph
void calculateDegreeAndSort(SparseUGraph *graph);

// sorts nodes based on degree
void sortDegree(SparseUGraph *graph);

// populate sample array in SparseUGraph with a percentage of the highest degree nodes
void sampleNodes(SparseUGraph *graph, float samp_rate);

// print out node degrees
void printDegree(SparseUGraph *graph);

// free the sparse undirected graph
void freeSparseUGraph(SparseUGraph *graph);

// write an edgelist for the sparse undirected graph
void writeSparseUGraph(FILE *outfile);

// return 1 if there is an edge from a to b, else 0
int hasEdge(SparseUGraph *graph, int a, int b);

// look up the id of the edge (src, dest)
int findEdgeId(SparseUGraph *graph, int src, int dest);

// return the degree of the node
int degree(SparseUGraph *graph, int node);

// print the graph, up to `num_nodes`
void printSparseUGraph(SparseUGraph *graph, int num_nodes);

// convert a graph to an edgelist format; this loses the id list,
// so be sure to save it first if you want to use it later
void graphToEdgeList(SparseUGraph *graph, EdgeList *elist);


///////////////////////////////////////
// BFS STUFF

// Holds information discovered while performing a BFS.
// This will be useful when finding centrality measures.
// It also allows one to trace back the shortest paths
// that were found, by using the distance and parent info.
typedef struct {

    int *parent;        // index represents node; value is index of parent
    int *distance;      // distance from node n to src
    int src;            // the root node of the search
    int n;              // number of nodes in the graph searched
    int *sigma;         // number of shortest paths from src through each node
    Vector *pred;       // predecessors (all possible parents, not just left-most).
    Vector stack;       // popping should return nodes in order of
                        // non-increasing distance from src

} BFSInfo;

// assume distance is initialized with all -1
#define discovered(info, node) (info->distance[node] >= 0)

// Zero out all BFS info data, to prepare for new run
// this assumes the grpah size has not changed.
void resetBFSInfo(BFSInfo *info);

// allocate new BFSInfo struct, setting `src` node for search root
void newBFSInfo(BFSInfo *info, int n);

// free BFSInfo struct
void freeBFSInfo(BFSInfo *info);

// Perform a BFS on the sparse undirected graph and return
// the information discovered. The src node is passed with
// the info struct.
void bfs(SparseUGraph *graph, BFSInfo *info);

// print out the path from the target node to the src node
void printShortestPath(BFSInfo *info, int dest);

// print out number of shortest paths
void printShortestPathCounts(BFSInfo *info);

// print out stack of vertices accumulated during BFS
// should return vertices in order of non-increasing distance from src
void printBFSStack(BFSInfo *info);

// print out predecessor info from BFS
void printPredecessors(BFSInfo *info);

// calculate modularity score for a graph
float modularity(SparseUGraph *graph, Vector *communities, int num_comm);

// returns actual number of edges for a node in a community
int getEdgesInComm(SparseUGraph *graph, int node);

// returns the expected number of random edges
int getDegreeInNetwork(SparseUGraph *graph, int node);

// Calculate edge betweenness centrality using sampling
// note that multiple calls calculate multiple times.
// The Vector will be returned with the indices of the edges
// with the largest betweenness centrality.
void calculateEdgeBetweenness(SparseUGraph *graph, Vector *largest);

// print out edge betweenness per edge
void printEdgeBetweenness(SparseUGraph *graph);

/************ K-MEDOID ***********/
#define LABELED -1
typedef struct {

    int k;
    int z; // num partitions
    
    int *seed_nodes; // labeled
    int *unlabeled;  // labeled nodes are still in this array, but they are marked with a -1

} kMedoidInfo;

typedef struct {

    int n; // node #
    int label; //zone #, -1 for unlabeled

    int *distances; //distances to other zones
} DTZ;

// graph partitioning by graph k medoids method
void graphKMedoid(SparseUGraph *graph, kMedoidInfo *k_med);

// generate dtz idx
void genDTZidx(SparseUGraph *graph, kMedoidInfo *k_med, DTZ *dtz_idx);

// generate initial k random seeds for k-medoids
void genKSeeds(SparseUGraph *graph, kMedoidInfo *k_med, DTZ *dtz_idx);

void labelDTZidx(SparseUGraph *graph, kMedoidInfo *k_med, DTZ *dtz_idx);


// Use the Girvan Newman (2004) algorithm to divisely
// cluster the graph into k partitions.
// Returns the number of communities found (may not be k).
int girvanNewman(SparseUGraph *graph, int k, float sample_rate, Vector **comms);

// Build up the communities from the divided graph
// using a union-find data structure
int labelCommunities(SparseUGraph *graph, Vector **comms);

// print out node community membership to outfile
void writeCommunities(int *idmap, Vector *comms, int k, char *outfile);

// Cut an edge from the graph by marking it with the negative
// of the iteration number in which it was cut.
void cutEdge(SparseUGraph *graph, int src, int dest, int iteration);
