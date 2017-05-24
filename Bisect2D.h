#include "xVect.h"
#include <stdexcept>

struct BisectNode {

	double x, y, f, g;
	int xNext;
	int yNext;
	bool free;

};

template <class UserData, double(*func_f)(double, double) = nullptr, double(*func_g)(double, double) = nullptr>
struct Bisect2 {

	UserData userData;

	// Input data and functions.
	int level, maxLevel;
	double tolerance;
	double xRoot, yRoot;

	// Fixed storage to avoid stack depletion during recursion.
	double x0, xm, x1, y0, ym, y1;
	double f00, f10, f01, f11, f0m, f1m, fm0, fm1, fmm;
	double g00, g10, g01, g11, g0m, g1m, gm0, gm1, gmm;
	int netSign;

	aVect<BisectNode> nodes;
	aVect<int> freeNodes;

	// The graph and recursion routine for building it.
	int graph;

	virtual double f(double x, double y) {
		MY_ERROR("missing function f");
		return 0;
	}
	
	virtual double g(double x, double y) {
		MY_ERROR("missing function g");
		return 0;
	}

	virtual void IterationCallback() {}
	virtual ~Bisect2() {}

	void Init(int maxLevel, double tolerance) {
		this->level = 0;
		this->maxLevel = maxLevel;
		this->tolerance = tolerance;
		this->nodes.Erase();
		this->freeNodes.Erase();
	}

	Bisect2(int maxLevel = 500, double tolerance = 1e-6) {
		this->Init(maxLevel, tolerance);
	}

	template <bool dbg = false, int minLevel = 0>
	bool Bisect(double x0, double x1, double y0, double y1, double& xRoot, double& yRoot) {

		// Test the four corner values.
		if (this->ZeroTest(x0, y0, this->f00, this->g00, xRoot, yRoot)) return true;
		if (this->ZeroTest(x1, y0, this->f10, this->g10, xRoot, yRoot)) return true;
		if (this->ZeroTest(x0, y1, this->f01, this->g01, xRoot, yRoot)) return true;
		if (this->ZeroTest(x1, y1, this->f11, this->g11, xRoot, yRoot)) return true;

		// Build the initial quad.

		auto&& nodes = this->nodes;

		// Add N00.
		this->graph = (int)nodes.Push().Count() - 1;
		nodes[this->graph].x = x0;
		nodes[this->graph].y = y0;
		nodes[this->graph].f = this->f00;
		nodes[this->graph].g = this->g00;

		// Add N10.
		BisectNode* temp = this->AddNode(x1, y0, this->f10, this->g10);
		temp->xNext = -1;
		nodes[this->graph].xNext = (int)nodes.Index(temp);

		// Add N01.
		temp = this->AddNode(x0, y1, this->f01, this->g01);
		temp->yNext = -1;
		nodes[this->graph].yNext = (int)nodes.Index(temp);

		// Add N11.
		temp = this->AddNode(x1, y1, this->f11, this->g11);
		temp->xNext = -1;
		temp->yNext = -1;
		nodes[nodes[this->graph].xNext].yNext = (int)nodes.Index(temp);
		nodes[nodes[this->graph].yNext].xNext = (int)nodes.Index(temp);

		this->level = 0;
		bool result = this->BisectRecurse<dbg, minLevel>(this->graph);
		
		if (result) {
			xRoot = this->xRoot;
			yRoot = this->yRoot;
		}

		this->nodes.Erase();
		this->freeNodes.Erase();

		return result;
	}

	bool ZeroTest(double x, double y, double& f, double& g, double& xRoot, double& yRoot) {

		try {
			if (func_f) f = func_f(x, y);
			else f = this->f(x, y);

			if (func_g) f = func_g(x, y);
			else g = this->g(x, y);

			if (abs(f) <= this->tolerance && abs(g) <= this->tolerance) {
				xRoot = x;
				yRoot = y;
				--this->level;
				return true;
			}
		}
		catch (const std::runtime_error&) {
			/* do nothing */
		}

		return false;
	}

	void DeleteNode(int& node) {

		if (this->nodes[node].free) MY_ERROR("gotcha");
		this->nodes[node].free = true;

		this->freeNodes.Push(node);
		node = -1;
	}

	BisectNode * AddNode(double x, double y, double f, double g) {

		BisectNode * node;
		if (this->freeNodes) {
			node = &this->nodes[this->freeNodes.Pop()];
			if (!node->free) MY_ERROR("gotcha");
		} else {
			node = &this->nodes.Push().Last();
		}
		
		node->free = false;
		node->x = x;
		node->y = y;
		node->f = f;
		node->g = g;

		return node;
	}
	
	template <bool dbg, int minLevel>
	bool BisectRecurse(int n00) {

		this->IterationCallback();

		if (++this->level == this->maxLevel) {
			--this->level;
			return false;
		}

		auto&& nodes = this->nodes;

		auto pn00 = &nodes[n00];

		int n10 = pn00->xNext;
		auto pn10 = &nodes[n10];

		int n11 = pn10->yNext;
		int n01 = pn00->yNext;

		auto pn01 = &nodes[n01];
		auto pn11 = &nodes[n11];

		if (dbg) {
			if (pn00->free) MY_ERROR("gotcha");
			if (pn10->free) MY_ERROR("gotcha");
			if (pn01->free) MY_ERROR("gotcha");
			if (pn11->free) MY_ERROR("gotcha");
		}

		if (this->level >= minLevel) {
			this->netSign = (int)(sign(pn00->f) + sign(pn01->f) + sign(pn10->f) + sign(pn11->f));

			if (abs(this->netSign) == 4) {
				if (dbg) {
					DbgStr("F: found box to be eliminated\n");
					DbgStr("x in [%g, %g]\n", pn00->x, pn11->x);
					DbgStr("y in [%g, %g]\n", pn00->y, pn11->y);
				}
				// F has the same sign at corners.
				--this->level;
				return false;
			}

			this->netSign = (int)(sign(pn00->g) + sign(pn01->g) + sign(pn10->g) + sign(pn11->g));

			if (abs(this->netSign) == 4) {
				if (dbg) {
					DbgStr("G: found box to be eliminated\n");
					DbgStr("x in [%g, %g]\n", pn00->x, pn11->x);
					DbgStr("y in [%g, %g]\n", pn00->y, pn11->y);
				}
				// G has the same sign at corners.
				--this->level;
				return false;
			}
		}

		// Bisect the quad.
		this->x0 = pn00->x;
		this->y0 = pn00->y;
		this->x1 = pn11->x;
		this->y1 = pn11->y;

		this->xm = ((double)0.5)*(this->x0 + this->x1);
		this->ym = ((double)0.5)*(this->y0 + this->y1);

		if (IsEqualWithPrec(this->x0, this->x1, 0.1*this->tolerance) && IsEqualWithPrec(this->y0, this->y1, 0.1*this->tolerance)) {
			this->xRoot = this->xm;
			this->yRoot = this->ym;
			return true;
		}

		// right edge 10,11
		if (ZeroTest(this->x1, this->ym, this->f1m, this->g1m, this->xRoot, this->yRoot)) return true;

		// bottom edge 01,11
		if (ZeroTest(this->xm, this->y1, this->fm1, this->gm1, this->xRoot, this->yRoot)) return true;

		// top edge 00,10
		if (ZeroTest(this->xm, this->y0, this->fm0, this->gm0, this->xRoot, this->yRoot)) return true;

		// left edge 00,01
		if (ZeroTest(this->x0, this->ym, this->f0m, this->g0m, this->xRoot, this->yRoot)) return true;

		// center
		if (ZeroTest(this->xm, this->ym, this->fmm, this->gmm, this->xRoot, this->yRoot)) return true;

		// right bisector
		BisectNode * temp = this->AddNode(this->x1, this->ym, this->f1m, this->g1m);
		temp->xNext = -1;
		temp->yNext = n11;
		nodes[n10].yNext = (int)nodes.Index(temp);

		// bottom bisector
		temp = this->AddNode(this->xm, this->y1, this->fm1, this->gm1);
		temp->xNext = n11;
		temp->yNext = -1;
		nodes[n01].xNext = (int)nodes.Index(temp);

		// top bisector
		temp = this->AddNode(this->xm, this->y0, this->fm0, this->gm0);
		temp->xNext = n10;
		nodes[n00].xNext = (int)nodes.Index(temp);

		// left bisector
		temp = this->AddNode(this->x0, this->ym, this->f0m, this->g0m);
		temp->yNext = n01;
		nodes[n00].yNext = (int)nodes.Index(temp);

		// middle bisector
		temp = this->AddNode(this->xm, this->ym, this->fmm, this->gmm);
		temp->xNext = nodes[n10].yNext;
		temp->yNext = nodes[n01].xNext;
		nodes[nodes[n00].xNext].yNext = (int)nodes.Index(temp);
		nodes[nodes[n00].yNext].xNext = (int)nodes.Index(temp);

		// Search the subquads for roots.
		bool result =
			BisectRecurse<dbg, minLevel>(n00) ||
			BisectRecurse<dbg, minLevel>(nodes[n00].xNext) ||
			BisectRecurse<dbg, minLevel>(nodes[n00].yNext) ||
			BisectRecurse<dbg, minLevel>(nodes[nodes[n00].xNext].yNext);

		if (result) return result;
		
		// The entire subquad check failed, remove the nodes that were added.

		// center
		this->DeleteNode(nodes[nodes[n00].xNext].yNext);

		// edges
		this->DeleteNode(nodes[n00].xNext);
		nodes[n00].xNext = n10;
		this->DeleteNode(nodes[n00].yNext);
		nodes[n00].yNext = n01;
		this->DeleteNode(nodes[n01].xNext);
		nodes[n01].xNext = n11;
		this->DeleteNode(nodes[n10].yNext);
		nodes[n10].yNext = n11;

		--this->level;
		return result;
	}
};
