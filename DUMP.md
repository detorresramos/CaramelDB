take a look through the codebase and do some research. I want you to understand how things work. We are creating a CSF
  data structure. A compressed key value store with fast O(1) lookups. We can even forget about the keys in this
  construction, with the tradeoff being we have to know the keys upfront and there is construction time. We do this
  through solving a binary system of equations in a highly optimized way, combined with huffman coding the values.

We also have this notion of adding a "prefilter". This is specifically for the case where we have a dominant token
occuring as some "alpha" percentage of the values. We can apply some generic filter like a Bloom filter, XOR filter, or
binary fuse filter to act as a quick check for this value before querying the CSF. The conditions for construction of
this filter allow us to further reduce overall space and query latency. However, I'm having trouble determining the
optimal decision criteria for applying this filter. The existing error rate stuff is a little flawed and you can feel
free to rip it apart. What we want, is to determine some fast decision criteria for calculating the optimal parameters
for setting a filter and if the optimal parameters help us in terms of memory of the combined structure, then we should
construct the pre filter. Otherwise we shouldn't.

There are some implications to the combined size when using a filter. Namely that the huffman distribution will change a
bit. Also, the typical "optimal error rate" for these respective filters is not necessarily the same formula here,
because we don't necessarily care about minimizing error rate at all, we really only care about the overall size of the
combined structure.

Look through the code deeply and understand what's going on. Think of some ways that we might be able to make the
decision criteria more idiomatic. Right now I'm not sure that the "calculateErrorRate" or the "shouldSkipFiltering"
conditions, nor the individual "make autotuned" or otherwise parameter tuning is accurate. Additionally, we have to
consider the range of practical values. As in, some theory we think of might imply an optimal num hashes as 0.3 but we
can't set it as that, we have to use positive integers. So our theory/decision criteria should take that into account.

Also, think of some ways that we might be able to essentially write some code that exposes options in python so that we
may more easily run scripts and tests to essentially learn more about the underlying truth by playing with these
options. We might be able to learn the decision criteria from these python scripts then apply it. But theory +
experimentation are probably both useful so we should think of both.

If you run something like @experiments/quick_alpha_scan.py with the old code we get that xor better than bloom and
binary fuse is better
    than both. This should be the case and should be a sanity check that we are doing things correctly.

We should ideally try to improve the threshold beyond alpha = 0.79 and try to get it to 0.6 ish with the best filter
possible.

This can be done with both theory and practice and you should validate your findings.

Some of this progress was already done so continue and definitely question decisions that are made that seem weird .

Come up with a plan