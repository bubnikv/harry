#pragma once

#include <stack>
#include "../../assert.h"

#define DFS

namespace cbm {

template <typename T>
struct DataTpl : T {
	int idx;

	DataTpl() : idx(-1)
	{}
	template <typename ...Params>
	DataTpl(int i, Params &&...params) : idx(i), T(std::forward<Params>(params)...)
	{}

	bool isUndefined()
	{
		return idx == -1;
	}
};
template <typename T>
std::ostream &operator<<(std::ostream &os, const DataTpl<T> &d)
{
	return os << d.idx << " [" << (T)d << "]";
}

template <typename T>
struct ElementTpl {
	ElementTpl<T> *prev, *next;
	DataTpl<T> data;

	bool isEdgeBegin;

	ElementTpl() : isEdgeBegin(true)
	{}
	ElementTpl(const DataTpl<T> &d) : data(d), isEdgeBegin(true)
	{}

	void set_prev(ElementTpl<T> *e)
	{
		prev = e;
		e->next = this;
	}
	void set_next(ElementTpl<T> *e)
	{
		next = e;
		e->prev = this;
	}
};

template <typename T>
struct PartTpl {
	ElementTpl<T> *rootElement;
	int nrVertices, nrEdges;

	PartTpl() : nrVertices(0), nrEdges(0)
	{}
};

struct CutBorderBase {
	enum INITOP {
		INIT,
		TRI100, TRI010, TRI001,
		TRI110, TRI101, TRI011,
		TRI111,
		EOM, IFIRST = INIT, ILAST = EOM
	};
	enum OP { BORDER, CONNBWD, SPLIT, UNION, NM, ADDVTX, CONNFWD, CLOSEBWD, CLOSEFWD, FIRST = BORDER, LAST = CONNFWD }; // close are meta operations; never transmitted
	static const char* op2str(OP op)
	{
		//"\xE2\x96\xB3", "\xE2\x96\xB3\xC2\xB9", "\xE2\x96\xB3\xC2\xB2", "\xE2\x96\xB3\xC2\xB3", 
		static const char *lut[] = { "_", "<", "\xE2\x88\x9E", "\xE2\x88\xAA", "~", "*", ">", "?" };
		return lut[op];
	}
	static const char* iop2str(INITOP iop)
	{
		//"\xE2\x96\xB3", "\xE2\x96\xB3\xC2\xB9", "\xE2\x96\xB3\xC2\xB2", "\xE2\x96\xB3\xC2\xB3", 
		static const char *lut[] = { "\xE2\x96\xB3", "\xE2\x96\xB3\xC2\xB9", "\xE2\x96\xB3\xC2\xB9", "\xE2\x96\xB3\xC2\xB9", "\xE2\x96\xB3\xC2\xB2", "\xE2\x96\xB3\xC2\xB2", "\xE2\x96\xB3\xC2\xB2", "\xE2\x96\xB3\xC2\xB3", "/" };
		return lut[iop];
	}
};

template <typename T>
struct CutBorder : CutBorderBase {
	typedef ElementTpl<T> Element;
	typedef PartTpl<T> Part;
	typedef DataTpl<T> Data;
	using CutBorderBase::INITOP;
	using CutBorderBase::OP;
	using CutBorderBase::op2str;

	Part *parts, *part;
	Element *elements, *element;
	Element *emptyElements;
	int max_elements, max_parts;

	Data *last; // last added data

// 	std::stack<int> swapped;
	int swapped;
	bool have_swap;

	// acceleration structure for fast lookup if a vertex is currently on the cutboder
	std::vector<int> vertices;

	int _maxParts, _maxElems;

	CutBorder(int maxParts, int maxElems, int vertcnthint = 0) : max_elements(0), max_parts(1), vertices(vertcnthint, 0), have_swap(false)
	{
		_maxParts = maxParts; _maxElems = maxElems;

		parts = new Part[maxParts]();
		++maxElems;
		elements = new Element[maxElems]();
		element = elements;
		emptyElements = elements;

		// initialize links
		for (int i = 0; i < maxElems; ++i) {
			elements[i].next = i + 1 == maxElems ? NULL : elements + (i + 1);
			elements[i].prev = i - 1 <  0        ? NULL : elements + (i - 1);
		}
	}

	~CutBorder()
	{
		delete[] parts;
		delete[] elements;
	}

	bool atEnd()
	{
		return part == NULL && element == NULL;
	}

	Element *traverseStep(Data &v0, Data &v1)
	{
		v0 = element->data;
		v1 = element->next->data;
// 		return element->data;
		return element;
	}

	Element *traversalorder(Element *bfs, Element *dfs)
	{
#ifdef DFS
		return dfs;
#else
		return bfs;
#endif
	}
	void next(Element *bfs, Element *dfs)
	{
		Element *nxt = traversalorder(bfs, dfs);

		// TODO: return on DFS
		Element *beg = nxt;
		while (!nxt->isEdgeBegin) {
			nxt = nxt->next;
			assert_ne(beg, nxt);
		}
		element = nxt;
	}

	void activate_vertex(int i)
	{
		if (i >= vertices.size()) vertices.resize(i + 1, 0);
		++vertices[i];
	}
	void deactivate_vertex(int i)
	{
		--vertices[i];
	}

	Element *new_element(Data v)
	{
		activate_vertex(v.idx);
		assert_lt(emptyElements, elements + _maxElems);
		Element *e = new (emptyElements) Element(v);
		assert_eq(e->isEdgeBegin, true);
		emptyElements = emptyElements->next;
		max_elements = std::max(max_elements, ++part->nrVertices);
		return e;
	}

	void del_element(Element *e, int n = 1)
	{
		if (n == 0) return;
		deactivate_vertex(e->data.idx);

		Element *nxt = e->next;
		emptyElements->set_prev(e);
		emptyElements = e;
		--part->nrVertices;
		assert_ge(part->nrVertices, 0);

		del_element(nxt, n - 1);
	}

	Element *get_element(int &edgecnt, int i, int p = 0)
	{
		edgecnt = 0;
		Element *e1;
		if (p != 0) e1 = (part - p)->rootElement;
		else e1 = element;

		if (i > 0) {
// 			e1 = e1->next;
			for (int j = 0; j < i; e1 = e1->next, ++j) {
				edgecnt += j && e1->isEdgeBegin ? 1 : 0;
			}
		} else {
			for (int j = 0; j < -i; e1 = e1->prev, ++j) {
				edgecnt += e1->prev->isEdgeBegin ? 1 : 0;
			}
		}

		return e1;
	}
	void find_element(Data v, int &i, int &p)
	{
		// this fn walks into both directions on the cutborder
		Element *l = element;
		Element *r = element->next;

		i = 0; p = 0;
		while (1) {
			if (r->data.idx == v.idx) {
				++i;
				return;
			} else if (l->data.idx == v.idx) {
				i = -i;
				return;
			}

			if (l == r || l->prev == r) {
				++p;
				assert_ge(part - p, parts);
				i = 0;
				l = (part - p)->rootElement;
				r = l->next;
			} else {
				l = l->prev;
				r = r->next;
				++i;
			}
		}
	}

	void new_part(Element *root)
	{
		++part;
		assert_lt(part, parts + _maxParts);
		part->rootElement = root;
		max_parts = std::max(max_parts, (int)(part - parts) + 1);
	}
	void del_part()
	{
		assert_eq(part->nrVertices, 0);
		if (part != parts) {
			--part;
			next(part->rootElement, part->rootElement);
		} else {
			part = NULL;
			element = NULL;
		}
	}

	void initial(Data v0, Data v1, Data v2)
	{
		part = parts;
		Element *e0 = new_element(v0), *e1 = new_element(v1), *e2 = new_element(v2);
		e0->set_next(e1); e1->set_next(e2); e2->set_next(e0);

		part->nrEdges = 3;

		next(e0, e2);

		part->rootElement = element;
	}
	void newVertex(Data v)
	{
		Element *v0 = element;
		Element *v1 = new_element(v);
		last = &v1->data;
		Element *v2 = element->next;

		++part->nrEdges; // -1 + 2

		v0->set_next(v1);
		v2->set_prev(v1);

		next(v2, v1);
	}
	Data connectForward(OP &op) // TODO: add border to realop
	{
		Data d = !element->next->isEdgeBegin ? Data(-1) : element->next->next->data;
		if (istri()) {
			// destroy
			del_element(element, 3);
			part->nrEdges = 0;

			del_part();
			op = CLOSEFWD;
		} else {
			element->isEdgeBegin = element->next->isEdgeBegin;
			Element *e0 = element;
			Element *e1 = element->next->next;
			--part->nrEdges; // -2 + 1
			del_element(element->next);
			e0->set_next(e1);

			next(e1, e0);
			op = CONNFWD;
		}
		return d;
	}
	Data connectBackward(OP &op) // TODO: add border to realop
	{
		Data d = !element->prev->isEdgeBegin ? Data(-1) : element->prev->data;
		if (istri()) {
			// destroy
			del_element(element, 3);
			part->nrEdges = 0;

			del_part();
			op = CLOSEBWD;
		} else {
			std::swap(element->data, element->prev->data);
			element->isEdgeBegin = element->prev->isEdgeBegin;
			Element *e0 = element->prev->prev;
			Element *e1 = element;
			--part->nrEdges; // -2 + 1
			del_element(element->prev);
			e0->set_next(e1);

			next(e1->next, e1);
			op = CONNBWD;
		}
		return d;
	}

	bool istri()
	{
		return part->nrEdges == 3 && part->nrVertices == 3;
	}

	OP border()
	{
		--part->nrEdges;
		if (part->nrEdges == 0) {
			element->isEdgeBegin = false;
			del_element(element, part->nrVertices);
			del_part();
		} else {
			if (part->nrVertices >= 1 && (part->nrVertices < 2 || element->prev->isEdgeBegin != element->next->isEdgeBegin)) {
				++part->nrEdges;
				OP dummy;
				if (!element->prev->isEdgeBegin) {
					connectBackward(dummy);
					return CONNBWD;
				} else if (!element->next->isEdgeBegin) {
					connectForward(dummy);
					return CONNFWD;
				}
			} else if (part->nrVertices >= 2 && !element->prev->isEdgeBegin && !element->next->isEdgeBegin) {
				element->isEdgeBegin = false;
				Element *n = element->next->next;
				element->prev->set_next(n);
				del_element(element, 2);
				element = n;
			} else
				element->isEdgeBegin = false;

			next(element->next, element->next);
		}

		return BORDER;
	}
	void preserveOrder()
	{
		return; // TODO
// 		if (!swapped.empty()) {
		if (have_swap) {
			Part *swapwith;
// 			do {
				swapwith = parts + swapped/*.top()*/;
				if (swapwith < part) {
// 					std::cout << "SWAP: " << (part - parts) << " " << swapped << std::endl;
					part->rootElement = element;
					std::swap(*part, *swapwith);
// 				swapped.pop();
					next(part->rootElement, part->rootElement);
				}
				have_swap = false;
// 			} while (swapwith >= part && !swapped.empty());
		}
	}

	Data splitCutBorder(int i)
	{
		int edgecnt;
		Element *e0 = element, *e1 = get_element(edgecnt, i);
		Element *newroot, *newtail;

		// setup connections
		newroot = e0->next;
		newtail = e1->prev;
		e0->set_next(e1);

		Element *split = new_element(e1->data);
		last = &split->data;
		newtail->set_next(split);
		split->set_next(newroot);

		// add part
		if (i > 0) {
			--i;
			part->rootElement = traversalorder(e1, e0);
			part->nrVertices -= i + 1;
			part->nrEdges -= edgecnt;
			new_part(newroot); // root unimportant -> will be overwritten; TODO: remove
			part->nrVertices += i + 1;
			part->nrEdges += edgecnt + 1;

			next(newroot, split);
		} else {
			i = -i;

			part->rootElement = traversalorder(newroot, split);
			part->nrVertices -= i + 1;
			part->nrEdges -= edgecnt;
			new_part(traversalorder(e1, e0)); // root unimportant -> will be overwritten; TODO: remove
			part->nrVertices += i + 1;
			part->nrEdges += edgecnt + 1;

			std::swap(*part, *(part - 1));
			swapped = (part - 1) - parts;
			have_swap = true;

// 			next(e1, e0);
			next(newroot, split);
		}

		return e1->data;
	}
	Data cutBorderUnion(int i, int p)
	{
		int edgecnt;
		Element *e0 = element, *e1 = get_element(edgecnt, i, p);

		Element *newroot = element->next;
		Element *newtail = e1->prev;

		e0->set_next(e1);

		Element *un = new_element(e1->data);
		last = &un->data;
		newtail->set_next(un);
		un->set_next(newroot);

		(part - p)->nrVertices += part->nrVertices; part->nrVertices = 0;
		(part - p)->nrEdges += part->nrEdges + 1; part->nrEdges = 0;
		(part - p)->rootElement = traversalorder(newroot, un); // sometimes it's more efficient to remove this line
		std::swap(*(part - p), *(part - 1)); // process the parts in correct traversal order
		del_part();

// 		next(newroot, newtail);

		return e1->data;
	}

	bool on_cut_border(int i)
	{
		return !!vertices[i];
	}

	bool findAndUpdate(Data v, int &i, int &p, OP &op)
	{
		if (!on_cut_border(v.idx)) return false;
		find_element(v, i, p);
		bigassert(int tr; assert_eq(get_element(tr, i, p)->data.idx, v.idx);)

		if (p > 0) {
			op = UNION;
			Data res = cutBorderUnion(i, p);
			assert_eq(res.idx, v.idx);
		} else {
			if (element->next->isEdgeBegin && element->next->next->data.idx == v.idx) {
// 				op = CONNFWD;
				connectForward(op);
			} else if (element->prev->isEdgeBegin && element->prev->data.idx == v.idx) {
// 				op = CONNBWD;
				connectBackward(op);
			} else if (i == 0) {
				// this can not happen
				assert_fail;
				return false;
			} else {
				op = SPLIT;
				Data res = splitCutBorder(i);
				assert_eq(res.idx, v.idx);
			}
		}
		return true;
	}
};

}
