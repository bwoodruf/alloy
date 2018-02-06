/*
 * Copyright(C) 2018, Blake C. Lucas, Ph.D. (img.science@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
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

#ifndef MAXFLOW_EX_H_
#define MAXFLOW_EX_H_

#include <AlloyLocator.h>
#include <AlloyMaxFlow.h>
#include "AlloyApplication.h"
#include "AlloyVector.h"
class MaxFlowEx: public aly::Application {
protected:
	aly::float2 cursor;
	aly::Vector2f samples;
	aly::DrawPtr drawRegion;
	std::vector<aly::int2> edges;
	std::vector<float> weights;
	std::vector<bool> saturated;
	std::vector<int> nodeLabels;
	std::vector<int> edgeLabels;
	std::vector<aly::Color> edgeColors;
	std::vector<std::shared_ptr<aly::MaxFlow::Edge>> edgePtrs;
	int vertexCount;
	int srcId,sinkId;
	aly::MaxFlow flow;
	void updateRender();
public:
	MaxFlowEx();
	bool init(aly::Composite& rootNode);
};

#endif 