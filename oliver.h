/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Yichao Cheng
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * The single GPU version of Olive
 *
 * Author: Yichao Cheng (onesuperclark@gmail.com)
 * Created on: 2015-02-05
 * Last Modified: 2015-02-05
 */

#ifndef OLIVER_H
#define OLIVER_H

#include "common.h"
#include "csrGraph.h"
#include "logging.h"
#include "timer.h"
#include "utils.h"
#include "commandLine.h"
#include "grd.h"
#include "oliverKernel.h"

template<typename VertexValue, typename AccumValue>
class Oliver {
public:

    template<typename F>
    void edgeMap(F f) {
        // Clear the accumulator before the gather phase starts
        accumulators.allTo(0);
        // Transfer the queue size back to config the kernel
        CUDA_CHECK(D2H(workqueueSize, workqueueSizeDevice, sizeof(VertexId)));

        auto c = util::kernelConfig(*workqueueSize);
        edgeMapKernel<VertexValue, AccumValue, F> <<< c.first, c.second>>>(
            workqueue.elemsDevice,
            workqueueSizeDevice,
            srcVertices.elemsDevice,
            outgoingEdges.elemsDevice,
            vertexValues.elemsDevice,
            accumulators.elemsDevice,
            workset.elemsDevice,
            f);
        CUDA_CHECK(cudaThreadSynchronize());
    }

    template<typename F>
    void vertexMap(F f) {
        auto c = util::kernelConfig(vertexCount);
        vertexMapKernel<VertexValue, AccumValue, F> <<< c.first, c.second>>>(
            vertexCount,
            vertexValues.elemsDevice,
            f);
        CUDA_CHECK(cudaThreadSynchronize());
    }

    template<typename F>
    void vertexFilter(F f) {
        // Clear the workqueue before generating it
        *workqueueSize = 0;
        CUDA_CHECK(H2D(workqueueSizeDevice, workqueueSize, sizeof(VertexId)));

        auto c = util::kernelConfig(vertexCount);
        vertexFilterKernel<VertexValue, AccumValue, F> <<< c.first, c.second>>>(
            workset.elemsDevice,
            vertexCount,
            vertexValues.elemsDevice,
            accumulators.elemsDevice,
            workqueue.elemsDevice,
            workqueueSizeDevice,
            f);
        CUDA_CHECK(cudaThreadSynchronize());
    }


    void readGraph(const char *path) {
        CsrGraph<int, int> graph;
        graph.fromEdgeListFile(path);

        vertexCount = graph.vertexCount;
        edgeCount = graph.edgeCount;

        srcVertices.reserve(vertexCount + 1);
        memcpy(srcVertices.elemsHost, graph.srcVertices, sizeof(EdgeId) * (vertexCount + 1));
        srcVertices.cache();

        outgoingEdges.reserve(edgeCount);
        memcpy(outgoingEdges.elemsHost, graph.outgoingEdges, sizeof(VertexId) * edgeCount);
        outgoingEdges.cache();

        vertexValues.reserve(vertexCount);
        accumulators.reserve(vertexCount);

        workset.reserve(vertexCount);
        workset.allTo(1); // All activated at the firsr place

        workqueue.reserve(vertexCount);

        workqueueSize = (VertexId *) malloc(sizeof(VertexId));
        *workqueueSize = 0;
        CUDA_CHECK(cudaMalloc((void **) &workqueueSizeDevice, sizeof(VertexId)));
        CUDA_CHECK(H2D(workqueueSizeDevice, workqueueSize, sizeof(VertexId)));

        allVerticesInactive = (bool *) malloc(sizeof(bool));
        CUDA_CHECK(cudaMalloc((void **) &allVerticesInactiveDevice, sizeof(bool)));
    }

    /**
     * Transfer all the `workqueueSize` back and sum them up.
     */
    inline VertexId getWorkqueueSize() {
        CUDA_CHECK(D2H(workqueueSize, workqueueSizeDevice, sizeof(VertexId)));
        return *workqueueSize;
    }

    void print() {
        CUDA_CHECK(D2H(workqueueSize, workqueueSizeDevice, sizeof(VertexId)));
        workqueue.print(*workqueueSize);
        // workset.print();
        vertexValues.print();
    }

private:
    /** Record the edge and vertex number of each partition. */
    VertexId         vertexCount;
    EdgeId           edgeCount;

    /**
     * CSR related data structure.
     */
    GRD<EdgeId>      srcVertices;
    // GRD<EdgeId>      dstVertices;
    GRD<VertexId>    outgoingEdges;
    // GRD<VertexId>    incomingEdges;

    /**
     * Vertex-wise state.
     */
    GRD<VertexValue> vertexValues;
    GRD<AccumValue>  accumulators;

    /**
     * Use a bitmap to represent the working set.
     */
    GRD<int> workset;

    /**
     * Use a queue to keep the work complexity low
     */
    GRD<VertexId>  workqueue;
    VertexId *workqueueSize;
    VertexId *workqueueSizeDevice;

    /**
     * A single variable to indicate the activeness of all vertices
     * in the partition.
     */
    bool *allVerticesInactive;
    bool *allVerticesInactiveDevice;


    // cudaStream_t     streams[2];
    // cudaEvent_t      startEvents[4];
    // cudaEvent_t      endEvents[4];

};

#endif // OLIVER_H