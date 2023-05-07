#pragma once

#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdlib>

template<typename S, typename T> // "S" is a scalar type; "T" is the type of data associated with each interval
class IITree {
    struct StackCell {
        size_t x; // node
        int w; // w: 0 if left child hasn't been processed
        StackCell() {};
        StackCell(size_t x_, int w_) : x(x_), w(w_) {};
    };
    struct Interval {
        S st, en, max;
        T data;
        Interval() = default;
        Interval(const S &s, const S &e, const T &d) : st(s), en(e), max(e), data(d) { }
    };
    struct IntervalLess {
        bool operator()(const Interval &intervalA, const Interval &intervalB) const { return intervalA.st < intervalB.st; }
    };
    std::vector<Interval> a;
    size_t layout_recur(Interval *b, size_t i = 0, size_t k = 0) { // see https://algorithmica.org/en/eytzinger
        if (k < a.size()) {
            i = layout_recur(b, i, (k<<1) + 1);
            b[k] = a[i++];
            i = layout_recur(b, i, (k<<1) + 2);
        }
        return i;
    }
    void index_BFS(Interval *interval, size_t n) { // set Interval::max
        int t = 0;
        StackCell stack[64];
        stack[t++] = StackCell(0, 0);
        while (t) {
            StackCell z = stack[--t];
            size_t k = z.x, l = k<<1|1, r = l + 1;
            if (z.w == 2) { // Interval::max for both children are computed
                interval[k].max = interval[k].en;
                if (l < n && interval[k].max < interval[l].max) interval[k].max = interval[l].max;
                if (r < n && interval[k].max < interval[r].max) interval[k].max = interval[r].max;
            } else { // go down into the two children
                stack[t++] = StackCell(k, z.w + 1);
                if (l + z.w < n)
                    stack[t++] = StackCell(l + z.w, 0);
            }
        }
    }
public:
    void add(const S &s, const S &e, const T &d) { a.push_back(Interval(s, e, d)); }
    void index() {
        std::sort(a.begin(), a.end(), IntervalLess());
        std::vector<Interval> b(a.size());
        layout_recur(b.data());
        a.clear();
        std::copy(b.begin(), b.end(), std::back_inserter(a));
        index_BFS(a.data(), a.size());
    }
    bool overlap(const S &st, const S &en, std::vector<size_t> &out) const {
        int t = 0;
        std::array<StackCell, 64> stack;
        out.clear();
        if (a.empty()) return false;
        stack[t++] = StackCell(0, 0); // push the root; this is a top down traversal
        while (t) { // the following guarantees that numbers in out[] are always sorted
            StackCell z = stack[--t];
            size_t l = (z.x<<1) + 1, r = l + 1;
            if (l >= a.size()) { // a leaf node
                if (st < a[z.x].en && a[z.x].st <= en) out.push_back(z.x);
            } else if (z.w == 0) { // if left child not processed
                stack[t++] = StackCell(z.x, 1); // re-add node z.x, but mark the left child having been processed
                if (l < a.size() && a[l].max > st)
                    stack[t++] = StackCell(l, 0);
            } else if (a[z.x].st <= en) { // need to push the right child
                if (st < a[z.x].en) out.push_back(z.x); // test if z.x overlaps the query; if yes, append to out[]
                if (r < a.size()) stack[t++] = StackCell(r, 0);
            }
        }
        return out.size() > 0? true : false;
    }
    size_t size(void) const { return a.size(); }
    const S &start(size_t i) const { return a[i].st; }
    const S &end(size_t i) const { return a[i].en; }
    const T &data(size_t i) const { return a[i].data; }
};