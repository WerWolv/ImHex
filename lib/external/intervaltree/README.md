## Introduction

cgranges is a small C library for genomic interval overlap queries: given a
genomic region *r* and a set of regions *R*, finding all regions in *R* that
overlaps *r*. Although this library is based on [interval tree][itree], a well
known data structure, the core algorithm of cgranges is distinct from all
existing implementations to the best of our knowledge.  Specifically, the
interval tree in cgranges is implicitly encoded as a plain sorted array
(similar to [binary heap][bheap] but packed differently). Tree
traversal is achieved by jumping between array indices. This treatment makes
cgranges very efficient and compact in memory. The core algorithm can be
implemented in ~50 lines of C++ code, much shorter than others as well. Please
see the code comments in [cpp/IITree.h](cpp/IITree.h) for details.

## Usage

### Test with BED coverage

For testing purposes, this repo implements the [bedtools coverage][bedcov] tool
with cgranges. The source code is located in the [test/](test) directory. You
can compile and run the test with:
```sh
cd test && make
./bedcov-cr test1.bed test2.bed
```
The first BED file is loaded into RAM and indexed. The depth and the breadth of
coverage of each region in the second file is computed by query against the
index of the first file.

The [test/](test) directory also contains a few other implementations based on
[IntervalTree.h][ekg-itree] in C++, [quicksect][quicksect] in Cython and
[ncls][ncls] in Cython. The table below shows timing and peak memory on two
test BEDs available in the release page. The first BED contains GenCode
annotations with ~1.2 million lines, mixing all types of features. The second
contains ~10 million direct-RNA mappings. Time1a/Mem1a indexes the GenCode BED
into memory. Time1b adds whole chromosome intervals to the GenCode BED when
indexing. Time2/Mem2 indexes the RNA-mapping BED into memory. Numbers are
averaged over 5 runs.

|Algo.   |Lang. |Cov|Program         |Time1a|Time1b|Mem1a   |Time2 |Mem2    |
|:-------|:-----|:-:|:---------------|-----:|-----:|-------:|-----:|-------:|
|IAITree |C     |Y  |cgranges        |9.0s  |13.9s |19.1MB  |4.6s  |138.4MB |
|IAITree |C++   |Y  |cpp/iitree.h    |11.1s |24.5s |22.4MB  |5.8s  |160.4MB |
|CITree  |C++   |Y  |IntervalTree.h  |17.4s |17.4s |27.2MB  |10.5s |179.5MB |
|IAITree |C     |N  |cgranges        |7.6s  |13.0s |19.1MB  |4.1s  |138.4MB |
|AIList  |C     |N  |3rd-party/AIList|7.9s  |8.1s  |14.4MB  |6.5s  |104.8MB |
|NCList  |C     |N  |3rd-party/NCList|13.0s |13.4s |21.4MB  |10.6s |183.0MB |
|AITree  |C     |N  |3rd-party/AITree|16.8s |18.4s |73.4MB  |27.3s |546.4MB |
|IAITree |Cython|N  |cgranges        |56.6s |63.9s |23.4MB  |43.9s |143.1MB |
|binning |C++   |Y  |bedtools        |201.9s|280.4s|478.5MB |149.1s|3438.1MB|

Here, IAITree = implicit augmented interval tree, used by cgranges;
CITree = centered interval tree, used by [Erik Garrison's
IntervalTree][itree]; AIList = augmented interval list, by [Feng et
al][ailist]; NCList = nested containment list, taken from [ncls][ncls] by Feng
et al; AITree = augmented interval tree, from [kerneltree][kerneltree].
"Cov" indicates whether the program calculates breadth of coverage.
Comments:

* AIList keeps start and end only. IAITree and CITree addtionally store a
  4-byte "ID" field per interval to reference the source of interval. This is
  partly why AIList uses the least memory.

* IAITree is more sensitive to the worse case: the presence of an interval
  spanning the whole chromosome.

* IAITree uses an efficient radix sort. CITree uses std::sort from STL, which
  is ok. AIList and NCList use qsort from libc, which is slow. Faster sorting
  leads to faster indexing.

* IAITree in C++ uses identical core algorithm to the C version, but limited by
  its APIs, it wastes time on memory locality and management. CITree has a
  similar issue.

* Computing coverage is better done when the returned list of intervals are
  start sorted. IAITree returns sorted list. CITree doesn't. Not sure about
  others. Computing coverage takes a couple of seconds. Sorting will be slower.

* Printing intervals also takes a noticeable fraction of time. Custom printf
  equivalent would be faster.

* IAITree+Cython is a wrapper around the C version of cgranges. Cython adds
  significant overhead.

* Bedtools is designed for a variety of applications in addition to computing
  coverage. It may keep other information in its internal data structure. This
  micro-benchmark may be unfair to bedtools.

* In general, the performance is affected a lot by subtle implementation
  details. CITree, IAITree, NCList and AIList are all broadly comparable in
  performance. AITree is not recommended when indexed intervals are immutable.

### Use cgranges as a C library

```c
cgranges_t *cr = cr_init(); // initialize a cgranges_t object
cr_add(cr, "chr1", 20, 30, 0); // add a genomic interval
cr_add(cr, "chr2", 10, 30, 1);
cr_add(cr, "chr1", 10, 25, 2);
cr_index(cr); // index

int64_t i, n, *b = 0, max_b = 0;
n = cr_overlap(cr, "chr1", 15, 22, &b, &max_b); // overlap query; output array b[] can be reused
for (i = 0; i < n; ++i) // traverse overlapping intervals
	printf("%d\t%d\t%d\n", cr_start(cr, b[i]), cr_end(cr, b[i]), cr_label(cr, b[i]));
free(b); // b[] is allocated by malloc() inside cr_overlap(), so needs to be freed with free()

cr_destroy(cr);
```

### Use IITree as a C++ library

```cpp
IITree<int, int> tree;
tree.add(12, 34, 0); // add an interval
tree.add(0, 23, 1);
tree.add(34, 56, 2);
tree.index(); // index
std::vector<size_t> a;
tree.overlap(22, 25, a); // retrieve overlaps
for (size_t i = 0; i < a.size(); ++i)
	printf("%d\t%d\t%d\n", tree.start(a[i]), tree.end(a[i]), tree.data(a[i]));
```

[bedcov]: https://bedtools.readthedocs.io/en/latest/content/tools/coverage.html
[ekg-itree]: https://github.com/ekg/intervaltree
[quicksect]: https://github.com/brentp/quicksect
[ncls]: https://github.com/hunt-genes/ncls
[citree]: https://en.wikipedia.org/wiki/Interval_tree#Centered_interval_tree
[itree]: https://en.wikipedia.org/wiki/Interval_tree
[bheap]: https://en.wikipedia.org/wiki/Binary_heap
[ailist]: https://www.biorxiv.org/content/10.1101/593657v1
[kerneltree]: https://github.com/biocore-ntnu/kerneltree