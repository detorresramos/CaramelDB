
% VLDB template version of 2020-08-03 enhances the ACM template, version 1.7.0:
% https://www.acm.org/publications/proceedings-template
% The ACM Latex guide provides further information about the ACM template

\documentclass[sigconf, nonacm]{acmart}

%% The following content must be adapted for the final version
% paper-specific
\newcommand\vldbdoi{XX.XX/XXX.XX}
\newcommand\vldbpages{XXX-XXX}
\usepackage{algorithm}
\usepackage{algorithmicx}
\usepackage{algpseudocode}
\usepackage{dblfloatfix}
% Define the argmin operator
\DeclareMathOperator*{\argmin}{arg\,min}

\usepackage{amsthm}
\newtheorem{theorem}{Theorem}[section]
\newtheorem{lemma}[theorem]{Lemma}
\newtheorem{definition}[theorem]{Definition}
\newtheorem{proposition}[theorem]{Proposition}
\newtheorem{corollary}[theorem]{Corollary}
\newtheorem{fact}[theorem]{Fact}
\newtheorem{remark}[theorem]{Remark}
\DeclareMathOperator*{\argmax}{arg\,max}
%\DeclareMathOperator{\mp}{mp}
\newcommand{\R}{\mathbb{R}}
\DeclareMathOperator*{\E}{{\mathbb{E}}}
\DeclareMathOperator*{\Var}{{\bf {Var}}}
\newcommand{\memmap}{\mathsf{MemMap}}
\newcommand{\onehot}{\mathsf{OneHot}}
\newcommand{\softmax}{\mathsf{softmax}}
\newcommand{\gelu}{\mathsf{GELU}}
\renewcommand{\S}{\mathcal{S}}
\newcommand{\mlp}{\mathsf{MLP}}
\newcommand{\U}{\mathcal{U}}
\newcommand{\id}{\mathrm{id}}
\newcommand{\vect}{\mathrm{vec}}
\newcommand{\dist}{\mathrm{dist}}

% issue-specific
\newcommand\vldbvolume{14}
\newcommand\vldbissue{1}
\newcommand\vldbyear{2020}
% should be fine as it is
\newcommand\vldbauthors{\authors}
\newcommand\vldbtitle{\shorttitle} 
% leave empty if no availability url should be set
\newcommand\vldbavailabilityurl{URL_TO_YOUR_ARTIFACTS}
% whether page numbers should be shown or not, use 'plain' for review versions, 'empty' for camera ready
\newcommand\vldbpagestyle{plain} 

\begin{document}
\title{AutoCSF: Provably Space-Efficient Indexing of Skewed Key-Value Workloads via Filter-Augmented Compressed Static Functions [Flavor: Foundations and Algorithms]}

%%
%% The "author" command and its associated commands are used to define the authors and their affiliations.
\author{David Torres Ramos}
\affiliation{%
  \institution{Distill}
  \city{New York}
  \country{USA}
}
\email{detorresramos1@gmail.com}

\author{Vihan Lakshman}
%\orcid{0000-0002-1825-0097}
\affiliation{%
  \institution{MIT CSAIL}
  \city{Cambridge}
  \country{USA}
}
\email{vihan@mit.edu}


\author{Chen Luo}
% \orcid{0000-0001-5109-3700}
\affiliation{%
  \institution{Amazon}
  \city{Palo Alto}
  \country{USA}
}
\email{cheluo@amazon.com}

\author{Todd Treangen}
% \orcid{0000-0001-5109-3700}
\affiliation{%
  \institution{Rice University}
  \city{Houston}
  \country{USA}
}
\email{treangen@rice.edu}


\author{Benjamin Coleman}
% \orcid{0000-0001-5109-3700}
\affiliation{%
  \institution{Google DeepMind}
  \city{Mountain View}
  \country{USA}
}
\email{colemanben@google.com}

%%
%% The abstract is a short summary of the work to be presented in the
%% article.
\begin{abstract}
We study the problem of building space-efficient, in-memory indexes for massive key-value datasets with highly skewed value distributions. This challenge arises in many data-intensive domains and is particularly acute in computational genomics, where $k$-mer count tables can contain billions of entries dominated by a single frequent value. While recent work has proposed to address this problem by augmenting \emph{compressed static functions} (CSFs) with pre-filters, existing approaches rely on complex heuristics and lack formal guarantees. In this paper, we introduce a principled algorithm, called AutoCSF, for combining CSFs with pre-filtering to provably handle skewed distributions with near-optimal space usage. We improve upon prior CSF pre-filtering constructions by (1) deriving a mathematically rigorous decision criterion for when filter augmentation is beneficial; (2) presenting a general algorithmic framework for integrating CSFs with modern set membership data structures beyond the classic Bloom filter; and (3) establishing theoretical guarantees on the overall space usage of the resulting indexes. Our open-source implementation of AutoCSF demonstrates space savings over baseline methods while maintaining low query latency.
\end{abstract}

\maketitle

%%% do not modify the following VLDB block %%
%%% VLDB block start %%%
\pagestyle{\vldbpagestyle}
\begingroup\small\noindent\raggedright\textbf{PVLDB Reference Format:}\\
\vldbauthors. \vldbtitle. PVLDB, \vldbvolume(\vldbissue): \vldbpages, \vldbyear.\\
\href{https://doi.org/\vldbdoi}{doi:\vldbdoi}
\endgroup
\begingroup
\renewcommand\thefootnote{}\footnote{\noindent
This work is licensed under the Creative Commons BY-NC-ND 4.0 International License. Visit \url{https://creativecommons.org/licenses/by-nc-nd/4.0/} to view a copy of this license. For any use beyond those covered by this license, obtain permission by emailing \href{mailto:info@vldb.org}{info@vldb.org}. Copyright is held by the owner/author(s). Publication rights licensed to the VLDB Endowment. \\
\raggedright Proceedings of the VLDB Endowment, Vol. \vldbvolume, No. \vldbissue\ %
ISSN 2150-8097. \\
\href{https://doi.org/\vldbdoi}{doi:\vldbdoi} \\
}\addtocounter{footnote}{-1}\endgroup
%%% VLDB block end %%%

%%% do not modify the following VLDB block %%
%%% VLDB block start %%%
\ifdefempty{\vldbavailabilityurl}{}{
\vspace{.3cm}
\begingroup\small\noindent\raggedright\textbf{PVLDB Artifact Availability:}\\
The source code, data, and/or other artifacts have been made available at \url{\vldbavailabilityurl}.
\endgroup
}
%%% VLDB block end %%%

\section{Introduction}

% Introduce the problem, motivate why it's important, explain how we solve it. Key idea: Formulating a CSF cost model is challenging, but our insight is that modeling the difference in space usage is tractable. 

Key-value stores are indispensable components of modern data infrastructure. As data volumes continue to balloon in size, there is increasingly a need to design new data structures for key-value lookups with small memory footprints while still enabling fast queries. When the set of key-value pairs is fixed (or changing infrequently), \emph{succinct retrieval} data structures have emerged as a state-of-the-art class of techniques for indexing data with minimal space overhead. These retrieval structures offer the guarantee of returning the correct value when queried with any key in the index, but may return arbitrary outputs for any key not in the set. 

Amongst these succinct retrieval methods, \emph{compressed static functions} (CSFs) achieve space usage proportional to the empirical entropy of the value set. Thus, when the set of values in the index is highly repetitive, CSFs can achieve significantly improved compression rates compared to distribution-agnostic methods, typically within a constant factor of the information-theoretic lower bound.

However, one limitation of CSFs is that, by construction, the index must use a lower bound of at least 1 bit per key. Consequently, if the value distribution is highly skewed, namely when a single entity is associated with the majority of the keys, CSFs fall short of achieving optimal compression rates. This barrier is especially prominent in computational genomics where long read $k$-mer count tables, which map keys of $k$-length nucleotide substrings to their frequency of occurrence in a given genome, often exhibit extreme skew with over 90\% of $k$-mers having a value of 0. This challenge was previously identified in the literature by \citet{shibuya2022space} who proposed a new index called a ``Bloom-enhanced Compressed Static Function" (BCSF) which augments a CSF with a Bloom filter to handle the dominating value. In particular, a BCSF index first inserts all keys not associated with the dominating value into a Bloom filter.  Additionally, since Bloom filters admit false positives, the BCSF index will insert the false positive keys associated with the dominating value into the CSF. At query time, given a key $k$, a BCSF first checks if $k$ is in the Bloom filter. If the filter returns false, then we can conclude that the associated value must be the dominating entity and simply return that value. If the filter returns true, we query the CSF and return the output. This construction guarantees correctness while eliminating the need to index the dominating value directly. Inspired in part by this work, \citet{hermann2025learned} also leverage this idea of filter augmentation to develop a \emph{learned} CSF index that allows for improved compression via machine learning.

Despite the effectiveness of filter-augmented CSFs, as demonstrated in these prior works in the literature, these data structure compositions introduce additional complexity. In particular, it is not completely clear which key-value distributions might benefit from filter-augmented CSFs or how to set the false positive rate of the filter to minimize the overall space usage. Unlike standalone approximate set membership data structures, which maintain a monotonic relationship between the false positive rate and the size of the resulting data structure, the relationship between the false positive rate and the overall index size in filter-augmented CSFs is complex to model. As an illustrative example, increasing the false positive rate of a filter might allow more values to pass through to the CSF, causing the value frequency distribution to be more closely-aligned to powers of 2 and thus more highly compressible by Huffman codes. While the work of \cite{shibuya2022space} present a set of criteria for making these parameter choices in a BCSF, we note that they are heuristic-driven as opposed to mathematically principled and, as we will demonstrate in this paper, can be misleading and result in suboptimal decisions. Motivated by these shortcomings, we ask the following research questions:

\begin{enumerate}
\item Can we develop a mathematically principled decision criterion to determine when a set of key-value pairs would benefit from filter-augmentation to save space versus simply using only a CSF? 

\item Given a filter-augmented CSF, can we design an efficient algorithm for determining the optimal parameters for the pre-filter that minimizes the total space usage of the index? 

\item Can we design a general framework for Questions (1) and (2) that can be applied to all set membership data structures in the literature, as opposed to just the classical Bloom filter? 
\end{enumerate}

In this paper, we affirmatively answer all three of these research questions through a \emph{single} algorithm, which we call AutoCSF. The key insight behind AutoCSF is that, while modeling the exact memory cost of a filter-augmented CSF appears to be intractable -- or at least unwieldy to analyze -- we can derive analytically clean upper and lower bounds on the \emph{difference} in space usage between a CSF and filter-augmented CSF. In other words, instead of attempting to develop separate cost models for the space usage of a CSF and filter-augmented CSF, we bound the difference in bits per key between the two data structures \emph{without ever having to explicitly construct either index}. This observation drives the development of our AutoCSF algorithm, which first determines if filter-augmentation is beneficial and then, if a filter is helpful, provides near-optimal settings for the filter parameters. The AutoCSF framework can be applied to any approximate set membership data structure with a known relationship between the memory usage and false positive rate, allowing our results to easily extend to the large number of membership filters that have been developed over the last 50 years~\cite{bloom1970space,mitzenmacher2001compressed,almeida2007scalable,porat2009optimal,fan2014cuckoo,pellow2017improving,mitzenmacher2018model,vaidya2020partitioned,liu2020stable,graf2020xor,mitzenmacher2020adaptive,BuRR2022,sato2023fast}. We implement AutoCSFs in practice and verify that our theoretical results align closely with empirical results (i.e. the bounds are tight), thus providing a principled algorithm to effectively compose CSFs with prefilters. 

% \section{Background: Compressed Static Functions}

\section{Related Work}

Succinct retrieval data structures, including CSFs, have garnered intense research interest in the theoretical computer science community for decades. The original CSF constructions were formed by using a minimal-perfect hash function to index a set of constant-size values, followed by algorithms that store Huffman codes of the values and algorithms that \textit{compute} the Huffman codes of the values through the solution of a randomized binary linear system~\cite{majewski1996family,dietzfelbinger2008succinct,belazzougui2013compressed}.

This binary, XOR-based system undergoes a phase transition as we reduce the space, and there has been considerable theoretical work to migrate from the easier, larger-overhead regime~\cite{botelho2013practical} (where hypergraph peeling solves the system in linear time with high probability) to the minimum-space regime where the system is not easily solvable~\cite{dietzfelbinger2008succinct}.
This minimum-space regime was primarily considered a theoretical curiosity and not feasible in practice due to a construction time cubic in the number of keys. The work of \cite{genuzio2020fast} refuted this conventional wisdom by presenting a practical CSF construction algorithm that leverages both sophisticated algorithmic techniques, such as lazy Gaussian elimination and hypergraph peeling, as well as implementation-level optimizations like broadword programming. 

Following the pioneering work of \citet{genuzio2020fast}, which provided the first practically implementable CSF construction, several works have proposed building data systems with CSFs as the foundational indexing primitive \cite{coleman2023caramel, shibuya2022space, hermann2025learned}. The most relevant prior work to our contributions in this paper is that of \citet{shibuya2022space} who proposed the idea of Bloom-enhanced CSFs to handle skewed value distributions, motivated by the problem of indexing skewed $k$mer count datasets in computational genomics. Our contributions in this paper directly build off the ideas in \cite{shibuya2022space}. Specifically, as we discuss further in the next section, the heuristic-driven decision criteria used in \cite{shibuya2022space} to determine when to apply a Bloom filter and how to set the false positive rate can lead to suboptimal configurations in practice. We improve upon this construction with our AutoCSF algorithm. 

In addition to the work of \citet{shibuya2022space}, several other recent works have studied CSFs from a practical lens. CARAMEL \cite{coleman2023caramel} explores indexing a multi-set of values for each key via an array of CSFs. More recently, \citet{hermann2025learned} proposed a learned CSF (LCSF) that combines tools from the rich literature on learned index structures \cite{kraska2018case} as well as Bloom filter augmentations to reduce the memory footprint of CSFs even further, though at the cost of increased query latency and long model training times. In this paper, we include an experimental comparison of our proposed AutoCSF framework with LCSF on skewed distributions where we find that we can in fact achieve comparable space savings without the overhead of training a learned model. This result may be of independent interest in demonstrating that learned indexes, despite their power, may provide limited utility in settings where we can analytically model the cost of an index directly. 

\section{Warmup: The BCSF Algorithm}

In this section, we review the BCSF heuristic introduced by \cite{shibuya2022space} as the design of this algorithm will motivate our efforts to improve upon it in this paper. Let $K$ denote the set of all items in the dataset and let $K_0$ denote the set of items with the most common frequency. We have that $|K_0| = \alpha |K|$ for some $0 \le \alpha \le 1$. Our goal is to minimize space by storing the set of non-dominating values in a Bloom filter and the associated counts of these non-dominating values in a CSF (plus the false positives).

Assume that the Bloom filter takes $C_{BF} \frac{1}{\varepsilon}$ bits per key where $\varepsilon$ denotes the false positive rate. Also assume the CSF takes $C_{CSF}$ bits per key. The total space usage of this Bloom-enhanced CSF data structure is thus $$C_{BF} (1-\alpha) |K| \log \frac{1}{\varepsilon} + C_{CSF} |K| ((1-\alpha) + \varepsilon \alpha)$$ where we assume the logarithm is base 2. \\

To determine if we need a Bloom filter, BCSF needs to verify if the following inequality holds $$C_{BF} (1-\alpha) |K| \log \frac{1}{\varepsilon} + C_{CSF} |K| ((1-\alpha) + \varepsilon \alpha) < C_{CSF} |K|$$

We can rewrite this inequality as $$\frac{C_{BF}}{C_{CSF}} \left(\frac{1-\alpha}{\alpha}\right) \log \frac{1}{\varepsilon} + \varepsilon < 1$$

The left-hand side of the previous inequality reaches its minimum at $$\varepsilon^* = \frac{C_{BF}}{C_{CSF}} \left(\frac{1-\alpha}{\alpha}\right) \log e$$

This gives us our decision criteria. We conclude that the Bloom filter helps reduce space if $\varepsilon^* < 1$ and $$\alpha > \frac{C_{BF} \log e}{C_{CSF} + C_{BF}\log e}$$

If $\alpha$ is sufficiently large, we can then construct a Bloom filter with false positive rate $\varepsilon^*$. 

The question remains: how do we compute $C_{BF}$ and $C_{CSF}$, the bits-per-key cost of the Bloom filter and the CSF? The BCSF algorithm estimates $C_{BF}$ by the theoretical space coefficient of idealized Bloom filters of 1.44. However, this idealized model is not realistic because it assumes the number of hash functions used by filter is continuous as opposed to a discrete parameter, which is the case in practice. In this paper, we explore whether we can do better by having a more accurate Bloom filter cost model. Secondly, the BCSF algorithm, as described in \cite{shibuya2022space}, estimates $C_{CSF}$ empirically in terms of the empirical entropy $H_0$, producing the piecewise equation 

\[
C_{CSF} = 
\begin{cases} 
0.22H_0^2 + 0.18H_0 + 1.16, \quad \text{if } H_0 < 2 \\

1.1H_0 + 0.2 \quad \text{otherwise}
\end{cases}
\]

However, this CSF cost model is a heuristic and not theoretically principled. In Figure \ref{fig:shibuya_bpk} we demonstrate that this heuristic can provide misleading guidance in practice, which motivates our work to design a more principled algorithm with provable guarantees with AutoCSF. The key challenge in designing a principled algorithm is that the cost of the CSF space usage, particularly when coupled with random false positives from the filter, is difficult to model analytically, which is why a data-driven heuristic as in BCSF is a natural choice. However, as we demonstrate in the next section, we can circumvent this difficulty by bounding the \emph{difference} in cost between a CSF and filter-enhanced CSF as opposed to modeling the cost of each index individually. This insight allows us to derive tight, usable bounds on the bit per key. 

\section{The AutoCSF Algorithm}

We describe AutoCSF, our new algorithm for filter-enhanced CSFs in Algorithm \ref{alg:autocsf}. The algorithm is responsible for making two key design decisions: (1) first determine if a given set of key-value pairs would achieve more compression with filter-augmentation as opposed to just a standard CSF; and (2) if filter-augmentation helps, set the filter parameters near-optimally. We note that Algorithm \ref{alg:autocsf} is parameterized by a filter bits-per-key cost function $b(\varepsilon)$ and can thus be applied to any approximate set membership data structure, including recent powerful techniques like XOR filters \cite{graf2020xor} and binary fuse filters \cite{graf2022binary} that provide a closed-form cost function $b(\varepsilon)$. The crucial step in Algorithm \ref{alg:autocsf} is to optimize a lower bound on the difference in cost between a CSF and filter-enhanced CSF (which we denote as FSF). 

By maximizing this lower bound in terms of the filter false positive rate $\varepsilon$, Algorithm \ref{alg:autocsf} aims to find a parameter settings that maximizes the space savings of using a filter-augmented CSF over a regular CSF. If these savings, even after maximization, are negative, then we conclude that a filter does not help. Otherwise, we build a filter-augmented CSF using the optimal false positive rate $\epsilon^*$. In the next subsection, we formally prove this lower bound on the bits per key difference in cost between the respective indexes. Then, in the following subsection, we prove an analogous upper bound. Although we do not use the upper bound directly in the AutoCSF algorithm, it is nevertheless a useful result in predicting the maximum possible space savings, which can be used for making cost-effective decisions on infrastructure to serve these indexes. 

\begin{algorithm}
\caption{Selecting Between CSF and CSF+Filter}
\begin{algorithmic}[1]
\Require A set of key--value pairs $\{(k_i, v_i)\}_{i=1}^n$ with most frequent token ratio $\alpha$ and a filter cost function $b(\varepsilon)$
\Ensure A chosen data structure (CSF or CSF+Filter) built on the full dataset

\State Compute the optimal $\varepsilon$ by maximizing the equation
\[
\varepsilon^* = \arg\max_{\varepsilon}
\bigl(\alpha \delta (1 - \varepsilon) - (1 - \alpha)b(\varepsilon)\bigr)
\]

\If{$\alpha \delta (1 - \varepsilon^*) - (1 - \alpha)b(\varepsilon^*) - n / N > 0$}
    \State Build a CSF+Filter using the optimal false positive rate $\varepsilon^\star$
\Else
    \State Build a CSF without a filter
\EndIf

\end{algorithmic}
\label{alg:autocsf}
\end{algorithm} 

\subsection{Proving the Lower Bound}
\begin{theorem}
\label{thm:HuffmanOptimality}
Given frequencies $F = \{f_1, f_2, ... f_{z}\}$ of values $V = \{v_1, v_2, ... v_{z}\}$ and a Huffman code $H(F)$ with lengths $\{l_1, l_2, ... l_{z}\}$,
$$ \sum_{i = 1}^{z} f_i l_i \leq \sum_{i = 1}^{z} f_i l'_i$$
for any other uniquely decodable code with lengths $l'_i$.
\end{theorem}


\begin{theorem}
Let $C_{\mathrm{FSF}}$ and $C_{CSF}$ denote the memory costs of filtered and non-filtered CSFs, where the filter is a set membership oracle with false positive rate $\epsilon$ that requires $b(\epsilon)$ bits per key.
$$ \left(C_{\mathrm{CSF}} - \mathbb{E}[C_{\mathrm{FSF}}]\right) / N \geq \alpha \delta (1 - \epsilon) - (1 - \alpha)b(\epsilon) - n / N$$
Note that, for practical distributions, the vocabulary size $n$ is typically $o(N)$, making $n / N = o(1)$.
\end{theorem}

\begin{proof}

We suppose that there are $n$ unique values, sorted in order of decreasing frequency $V = \{v_0, ... v_{n}\}$. We use $e$ to denote the Huffman encoding, $l_i$ for the length of the encoding $e(v_i)$ and $f_i$ for the frequency of $v_i$.

\textbf{Cost of the CSF:} The cost to store a compressed static function of $n = |V|$ distinct values is:

$$ C_{\mathrm{CSF}} = \delta \sum_{i = 0}^{n} f_i l_i + D_V + D_e$$

where $\delta$ is a constant that depends on whether $3$ or $4$ hash functions are used to construct the linear system. With $3$ hashes, $\delta_3 \approx 1.089$ and with $4$ hashes, $\delta_4 \approx 1.024$. The canonical Huffman dictionary requires $D_V$ space to store the unique values and $D_e = n\lceil \log_2 \max_i l_i\rceil $ bits to store the encoding lengths for the set of values.

\textbf{Cost of the filtered CSF:} When we prefilter the CSF, we remove (some of) the majority keys from the CSF and change the frequency distribution for the Huffman encoding. Specifically, we reduce the frequency of the majority value from $f_0$ to $f'_0$, causing the Huffman encoding to change from $e$ to $e'$.
Removal of the majority value can potentially update all of the Huffman code lengths, causing them to change from $\{l_0, ... l_{n}\}$ to $\{l'_0, ... l'_{n}\}$. The cost $D_V$ to store the vocabulary remains the same.

% Suppose we construct a Bloom filter with false-positive rate $\epsilon$. This introduces an overhead of $1.44 \log_2 \epsilon^{-1}$ bits per item in the filter~\cite{Bloom1970space} and causes the Huffman encoding to change from $e$ to $e'$:

The cost of the prefiltered CSF also includes the cost to store the filter, which is $b(\epsilon)$ multiplied by the number of keys in the filter.

$$ C_{\mathrm{FSF}} = \delta f'_0 l'_0 + \delta \sum_{i = 1}^{n} f_i l'_i + D_V + D_{e'} + b(\epsilon) (N - f_0)$$

Here, $f'_0$ denotes the number of keys with value $v_0$ that erroneously pass through the filter. We insert $(N - f_0)$ minority keys into the filter at a cost of $b(\epsilon)$ bits per key, so the filter overhead is $b(\epsilon) (N - f_0)$.

\textbf{Bounding the expected cost:} We wish to find a lower bound on the performance improvement offered by the filter. Note that because $C_{\mathrm{FSF}}$ measures the storage cost (entropy), we want $C_{\mathrm{FSF}} < C_{\mathrm{CSF}}$. We will analyze the following expression.
\begin{align*}
C_{\mathrm{CSF}} - C_{\mathrm{FSF}} &= \delta \sum_{i = 0}^{n} f_i l_i + D_V + D_e \\&- \left(\delta f'_0 l'_0 + \delta \sum_{i = 1}^{n} f_i l'_i + D_V + D_{e'} + b(\epsilon) (N - f_0)\right)
\end{align*}

We may reorganize this expression into the following form.
\begin{align*}
C_{\mathrm{CSF}} - C_{\mathrm{FSF}} &= \delta \left(f_0 l_0 + \sum_{i = 1}^n f_i l_i - f'_0 l'_0 - \sum_{i=1}^n f_i l'_i\right) \\
& + D_e - D_{e'} \\
& + D_V - D_V \\ 
& - b(\epsilon) (N - f_0) 
\end{align*}

We will bound each of these terms individually.

\textbf{Bounding the Huffman code entropy:} To bound the terms involving $f_i l_i$ and $f_i l'_i$, we use the optimality of Huffman codes. Specifically, we have that
$$ f'_0 l'_0 + \sum_{i = 1}^{n} f_i l'_i \leq f'_0 l_0 + \sum_{i=1}^{n} f_i l_i$$
because the code $e'$ designed for $\{f'_0, f_1, f_2, ... f_{n}\}$ will have smaller average space than the code $e$ designed for $F = \{f_0, f_1, f_2, ... f_{n}\}$ (by Theorem~\ref{thm:HuffmanOptimality}). Therefore,

\begin{align*}
\delta \left(f_0 l_0 + \sum_{i = 1}^n f_i l_i - f'_0 l'_0 - \sum_{i=1}^n f_i l'_i\right)&\\
 \geq \left(f_0 l_0 + \sum_{i = 1}^n f_i l_i - f'_0 l_0 - \sum_{i=1}^n f_i l_i\right)& = l_0 (f_0 - f'_0)
\end{align*}

\textbf{Bounding the Huffman codebook size:} To bound the size of the difference $D_{e'} - D_{e}$, we rely on some basic properties of canonical Huffman codes.
Canonical Huffman codes only need to store the lengths of each code in $e(V)$ and $e'(V)$, not the codes themselves. Let $l_{\max} = \max_i l_i$ and $l'_{\max} = \max_i l'_i$. 

Then
$$D_e = n\lceil \log_2 l_{\max} \rceil \qquad D_{e'} = n\lceil \log_2 l'_{\max}\rceil$$

By replacing $f_0$ with $f'_0$, we increase the depth of the Huffman tree for $e'$ by at most 1. Therefore, $l'_{\max} \leq l_{\max} + 1$. This implies the following bound on the canonical Huffman codebooks.

$$ D_{e'} - D_{e} \leq n \lceil \log_2 (l_{\max}+1) \rceil - z\lceil \log_2 l_{\max} \rceil \leq n$$

\textbf{Taking the expectation:} By combining the previous two parts of the proof, we have the bound 

\begin{align*}
C_{\mathrm{CSF}} - C_{\mathrm{FSF}} &\geq \delta l_0 (f_0 - f'_0)  -n  - b(\epsilon) (N - f_0) 
\end{align*}



The headroom expression $C_{\mathrm{CSF}} - C_{\mathrm{FSF}}$ contains $f'_0$, the number of keys with value $v_0$ that erroneously pass through the filter. It should be noted that $f'_0$ is a random variable, where the randomness comes from the hash functions and other randomization techniques used in the approximate set membership algorithm. 

Because the set membership algorithm returns a false positive with probability $\epsilon$, the expected value of $f'_0$ is $\mathbb{E}[f'_0] = \epsilon f_0$. Thus we have

\begin{align*}
C_{\mathrm{CSF}} - \mathbb{E}[C_{\mathrm{FSF}}] &\geq \delta l_0 f_0 (1 - \epsilon)
-n -b(\epsilon) (N - f_0) 
\end{align*}

To complete the proof, we divide both sides of the equation by $N$, the total number of keys in the key-value store.
\begin{align*}
\frac{1}{N}\left(C_{\mathrm{CSF}} - \mathbb{E}[C_{\mathrm{FSF}}]\right) \geq \delta l_0 \alpha (1 - \epsilon) 
-n / N 
-b(\epsilon) (1 - \alpha) 
\end{align*}

\end{proof}

\subsection{Proving the Upper Bound}
Now we develop an upper bound on the memory cost of the filtered CSF relative to the regular CSF. In order to do this, we require an additional fact about the optimality of Huffman codes. The following theorem is a well-known consequence of the Kraft-McMillan inequality and the Shannon noiseless coding theorem.

\begin{theorem}\label{thm:HuffmanEntropy} Given frequencies $F = \{f_1, f_2, ... f_z\}$ of values $V = \{v_1, v_2, ... v_z\}$ and a Huffman code $H(F)$ with lengths $\{l_1, l_2, ... l_z\}$

$$ -\sum_{i = 0}^{z}\frac{f_i}{N} \log_2 \frac{f_i}{N} \leq \sum_{i = 0}^{z} \frac{f_i}{N} l_i \leq 1 - \sum_{i = 0}^{z}\frac{f_i}{N} \log_2 \frac{f_i}{N} $$

where $N = \sum_{i = 1}^{z} f_i$.
\end{theorem}

\begin{theorem}
Let $C_{\mathrm{FSF}}$ and $C_{CSF}$ denote the memory costs of filtered and non-filtered CSFs, where the filter is a set membership oracle with false positive rate $\epsilon$ that requires $b(\epsilon)$ bits per key.
$$\mathbb{E}[C_{\mathrm{CSF}}- C_{\mathrm{FSF}}] / N  \leq 2\delta - \frac{1}{2}\alpha  \delta  \epsilon + n/N 
- b(\epsilon) (N - f_0)$$
\end{theorem}
\begin{proof}
We follow the same development as the previous theorem, up to the point where we obtain the following cost expression:

\begin{align*}
C_{\mathrm{CSF}} - C_{\mathrm{FSF}} &= \delta \left(f_0 l_0 + \sum_{i = 1}^n f_i l_i - f'_0 l'_0 - \sum_{i=1}^n f_i l'_i\right) \\
& + D_e - D_{e'} \\
& + D_V - D_V \\ 
& - b(\epsilon) (N - f_0) 
\end{align*}

Rather than find lower bounds for each term as before, now we will find upper bounds. By replacing $f_0$ with $f_0'$, we may decrease the depth of the Huffman tree by at most 1, yielding $D_e - D_{e'} \leq n$. As before, we leave the $-b(\epsilon)(N - f_0)$ term alone, and turn our attention to the more difficult term involving the code lengths.

The proof sketch is to use both the upper and lower bounds from Theorem~\ref{thm:HuffmanEntropy} to get an upper bound on the difference. Therefore, we use the upper entropy bound for the $f_i l_i$ terms and the lower entropy bound for the $-f_i' l_i'$ terms.

$$- \sum_{i=0}^{n}\frac{f_i}{N} \log_2\frac{f_i}{N} \leq \frac{f_0}{N}l_0 + \sum_{i=1}^{n}\frac{f_i}{N}l_i \leq 1 - \sum_{i=0}^{n}\frac{f_i}{N}\log_2 \frac{f_i}{N}$$

This yields the following pair of inequalities.

% \begin{align*}
% -\frac{f_0'}{N'} \log_2\frac{f_i'}{N'} - \sum_{i=1}^{n} \frac{f_i}{N'} \log_2 \frac{f_i}{N'} &\leq \frac{f_0'}{N} l_0' + \sum_{i=1}^{n}\frac{f_i}{N}l_i'\\
% 1 - \frac{f_0'}{N'} \log_2 \frac{f_0'}{N'} - \sum_{i=1}^{n} \frac{f_i}{N'} \log_2 \frac{f_i}{N'} &\geq \frac{f_0'}{N} l_0' + \sum_{i=1}^{n}\frac{f_i}{N}l_i'
% \end{align*}

\begin{align*}
\frac{f_0'}{N} l_0' + \sum_{i=1}^{n}\frac{f_i}{N}l_i' &\geq -\frac{f_0'}{N'} \log_2\frac{f_i'}{N'} - \sum_{i=1}^{n} \frac{f_i}{N'} \log_2 \frac{f_i}{N'}\\
\frac{f_0'}{N} l_0' + \sum_{i=1}^{n}\frac{f_i}{N}l_i' &\leq 1 - \frac{f_0'}{N'} \log_2 \frac{f_0'}{N'} - \sum_{i=1}^{n} \frac{f_i}{N'} \log_2 \frac{f_i}{N'}
\end{align*}

Note that $N' = N - f_0 + f_0'$ because the two Huffman codes are constructed on different sets: one on the original data and the second on the filtered data.

This allows us to bound the following expression:
$$Z = f_0 l_0 + \sum_{i=1}^{n} f_i l_i - f_0' l_0' - \sum_{i=1}^{n} f_i l_i'$$
$$ \leq N - f_0 \log_2 \frac{f_0}{N} + f_0' \log_2 \frac{f_0'}{N'} - \sum_{i=1}^{n}f_i\left(\log_2 \frac{f_i}{N} - \log_2 \frac{f_i}{N'}\right)$$
$$ = N - f_0 \log_2 \frac{f_0}{N} + f_0' \log_2 \frac{f_0'}{N'} - \sum_{i=1}^{n}f_i\left(\log_2 N' - \log_2 N\right)$$
$$ = N - f_0 \log_2 \frac{f_0}{N} + f_0' \log_2 \frac{f_0'}{N'} - \log_2\left(\frac{N'}{N}\right)\sum_{i=1}^{n}f_i$$

For the $f_0' \log_2 f_0' / N'$ term, note that
\begin{align*}
f_0' \log_2 \frac{f_0'}{N'} &= f_0' \log_2 \left(\frac{f_0'}{N-f_0 + f_0'}\right) = f_0' \log_2 \left(\frac{1}{\frac{N-f_0}{f_0'} + 1}\right)\\ &= -f_0' \log_2\left(1 + \frac{N-f_0}{f_0'}\right)
\end{align*}

Recall the inequality
$-\ln(1 + z) \leq -\frac{2z}{2+z}$ for $z \geq 0$~\cite{topsoe2004some}.
% Reference: Some bounds for the logarithmic function by Flemming Topsoe (https://rgmia.org/papers/v7n2/pade.pdf)
Applying this inequality to our expression with $z = (N-f_0)/f_0'$, we obtain

\begin{align*}
-f_0'\log_2\left(1 + \frac{N-f_0}{f_0'}\right) &\leq -2f_0'\frac{N-f_0}{f_0'\left(2 + \frac{N-f_0}{f_0'}\right)} = -2f_0'\frac{N-f_0}{2f_0' + N-f_0}\\ &\leq -2f_0'\frac{N-f_0}{3N - f_0}
\end{align*}
where the second inequality is because $f_0' \leq N$. This leaves us with the overall inequality

$$Z \leq N - f_0 \log_2 \frac{f_0}{N} - 2f_0' \frac{N-f_0}{3N-f_0} - \log_2\left(\frac{N'}{N}\right) \sum_{i=1}^{n}f_i$$

Now we address the $-\log_2\left(\frac{N'}{N}\right) \sum_{i=1}^{n}f_i$ term, observing that the function
$$-\log_2\left(\frac{N'}{N}\right) = -\log_2\left(\frac{N-f_0 + f_0'}{N}\right) \leq -\log_2\left(\frac{N-f_0}{N}\right)$$
because it is a monotonically decreasing function of $f_0'$ that attains a maximum at $f_0' = 0$. Finally, note that
$\sum_{i=1}^{n}f_i = N - f_0$ by the definition of $N$,
yielding the following overall bounds.

$$Z \leq N - f_0 \log_2 \frac{f_0}{N} - 2f_0' \frac{N-f_0}{3N-f_0} - \log_2\left(\frac{N-f_0}{N}\right) \left(N-f_0\right)$$

We may reorganize this expression into the following form by reordering terms and using the fact that $\alpha = f_0 / N$.

$$Z \leq N - f_0 \log_2 \alpha - 2f_0' \frac{1-\alpha}{3-\alpha} - \log_2\left(1-\alpha\right) \left(N-f_0\right)$$

By combining this bound with the previous bound on $D_e - D_{e'}$ we have the following bound on $\Delta = C_{\mathrm{CSF}} - C_{\mathrm{FSF}}$

\begin{align*}
\Delta &= \delta \left(N - f_0 \log_2 \alpha - 2f_0' \frac{1-\alpha}{3-\alpha} - (N-f_0)\log_2 (1-\alpha)\right) \\
& + n - b(\epsilon) (N - f_0) 
\end{align*}

Taking the expectation and dividing by $N$, we obtain
\begin{align*}
\mathbb{E}[\Delta] / N &\leq \delta \left(1 - \alpha \log_2 \alpha - 2\alpha \epsilon \frac{1-\alpha}{3-\alpha} - (1-\alpha)\log_2 (1-\alpha)\right) \\
& + n/N - b(\epsilon) (N - f_0)
\end{align*}
Observe that $-\alpha \log_2 \alpha - (1-\alpha) \log_2(1-\alpha)$ is the entropy of a Bernoulli trial with success rate $\alpha$, which is trivially bounded above by 1 bit. This gives rise to the expression
$$\mathbb{E}[\Delta] / N  \leq 2\delta - 2\alpha  \delta \epsilon \frac{1-\alpha}{3-\alpha} + n/N 
- b(\epsilon) (1 - \alpha)$$
which can be further simplified to
$$\mathbb{E}[\Delta] / N  \leq 2\delta - \frac{1}{2}\alpha \delta \epsilon + n/N 
- b(\epsilon) (1 - \alpha)$$
\end{proof}

Putting the two bounds together,

\begin{align*}
\mathbb{E}[C_{\mathrm{CSF}}- C_{\mathrm{FSF}}] / N &\geq \alpha \delta (1-\epsilon) - (1-\alpha) b(\epsilon) - n/N \\
\mathbb{E}[C_{\mathrm{CSF}}- C_{\mathrm{FSF}}] / N &\leq 2\delta - \frac{1}{2}\alpha \epsilon
- b(\epsilon) (1 - \alpha) + n/N 
\end{align*}

The gap is
$ 2\delta + \frac{1}{2}\alpha \delta \epsilon - \alpha \delta$
which, as expected, is greater than 0.

\section{Theory Validation}


\begin{figure*}[!t]
\begin{center}
\includegraphics[width=\textwidth]{vldb2026/figures/theory_validation/alpha_sweep_zipfian.png}
\end{center}

\begin{center}
\includegraphics[width=\textwidth]{vldb2026/figures/theory_validation/alpha_sweep_unique.png}
\end{center}

\begin{center}
\includegraphics[width=\textwidth]{vldb2026/figures/theory_validation/alpha_sweep_uniform_100.png}
\end{center}

\caption{Results of sweeping $\alpha$ for each distribution across all five filter types.}
\label{fig:alpha_sweep}
\end{figure*}

The AutoCSF algorithm is based on three theoretical claims:
\begin{enumerate}
    \item The space savings from filter augmentation are bounded between our lower and upper bounds (Theorems 5.2 and 5.4).
    \item The sign of the lower bound provides a reliable decision criterion for when filtering is beneficial.
    \item Maximizing the lower bound over the filter's parameter space yields the parameter with the largest provable space savings.
\end{enumerate}
In this section, we evaluate each of these claims empirically. Our goal is to determine whether the bounds are tight enough to be practically useful, whether the decision criterion produces false positives or false negatives, and how much regret (if any) the theory-guided parameter incurs relative to an exhaustive search over all discrete parameter settings.

We construct synthetic key-value datasets that sweep the majority fraction $\alpha$ from 0.50 to 0.99 and evaluate three minority value distributions that span a range of minority entropy.
\begin{itemize}
    \item \textbf{Unique}: each minority key has a distinct value (worst case for the bounds).
    \item \textbf{Zipfian}: power-law with exponent $s = 1.5$.
    \item \textbf{Uniform-100}: values drawn uniformly from 100 symbols.
\end{itemize}
We evaluated five types of filters: XOR filter, binary fuse filter, and Bloom filters with $k \in \{1, 2, 3\}$ hash functions.

Figure ~\ref{fig:alpha_sweep} shows the results of sweeping $\alpha$ for each distribution across all five filter types. In each panel, the shaded region represents the gap between our lower bound (Theorem 5.2) and upper bound (Theorem 5.4), the green curve shows the empirical savings achieved by the theory-guided parameter, and the red curve shows the best empirical savings found via exhaustive search.

\begin{figure*}[!t]
\begin{center}
\includegraphics[width=0.8\textwidth]{vldb2026/figures/theory_validation/epsilon_sweep_uniform_100_alpha0.7.png}
\end{center}

\begin{center}
\includegraphics[width=0.8\textwidth]{vldb2026/figures/theory_validation/epsilon_sweep_uniform_100_alpha0.9.png}
\end{center}


\begin{center}
\includegraphics[width=0.8\textwidth]{vldb2026/figures/theory_validation/epsilon_sweep_unique_alpha0.7.png}
\end{center}


\begin{center}
\includegraphics[width=0.8\textwidth]{vldb2026/figures/theory_validation/epsilon_sweep_unique_alpha0.9.png}
\end{center}


\begin{center}
\includegraphics[width=0.8\textwidth]{vldb2026/figures/theory_validation/epsilon_sweep_zipfian_alpha0.7.png}
\end{center}

\begin{center}
\includegraphics[width=0.8\textwidth]{vldb2026/figures/theory_validation/epsilon_sweep_zipfian_alpha0.9.png}
\end{center}


\caption{Results of sweeping $\epsilon$ for each distribution across all five filter types.}
\label{fig:epsilon_sweep}
\end{figure*}


\subsection{Bound Tightness}

Figure~\ref{fig:alpha_sweep} shows four curves per panel. The \emph{lower bound} (blue dashed) is Theorem~5.2 evaluated at the discrete parameter $\varepsilon_{\mathrm{LB}}$ that maximizes the lower bound. The \emph{best empirical} curve (red) is the measured bits/key saved at the discrete parameter $\varepsilon_{\mathrm{emp}}$ that yields the largest savings, found by exhaustive filter-parameter search  measured over real AutoCSF objects. We also plot the upper bound (Theorem~5.4) evaluated at two $\varepsilon$ values: $\varepsilon_{\mathrm{LB}}$ and $\varepsilon_{\mathrm{emp}}$.

We observe that across all filter-distribution combinations and all 50 values of $\alpha$, the empirical savings consistently fall between the lower bound and the upper bound at $\varepsilon_{\mathrm{emp}}$. We note that this is not a formal proof of correctness. The bounds assume a continuous $\varepsilon$ while the experiments operate over discrete filter parameters, and the measured space includes implementation overheads (metadata, alignment) not captured by the theory. Nonetheless, empirical values stay within the predicted range across all configurations, which provides strong evidence that the bounds are practically valid.

The upper bound is looser, with a gap of up to $\sim$1.5 bits/key at moderate $\alpha$ for the unique distribution, and a smaller gap for zipfian and uniform-100. This looseness does not affect AutoCSF's practical decisions, which are driven entirely by the lower bound.

\subsection{Validating the Decision Criterion}


To further validate AutoCSF, we sweep the false positive rate $\varepsilon$ across all discrete filter configurations for each distribution at two values of $\alpha$: $\alpha = 0.7$, where filtering is marginal, and $\alpha = 0.9$, where filtering provides clear benefits. Each panel in Figure~\ref{fig:epsilon_sweep} sweeps $\varepsilon$ on a log scale and plots the continuous lower and upper bounds (shaded region). The empirical bits/key saved are overlaid at the discrete $\varepsilon$ values corresponding to each filter parameter (e.g., fingerprint bits or bits per element), and the theory-optimal $\varepsilon^*$ is marked as the continuous maximum of the lower bound.

\textbf{Decision criterion.} The lower bound crossing zero provides a decision boundary: when $\text{LB} > 0$ at the theory-optimal $\varepsilon^*$, AutoCSF recommends filtering; otherwise, it defaults to a plain CSF. Across all filter types, distributions, and $\alpha$ values in the alpha sweep, we find that whenever the lower bound recommends filtering, the empirical savings are positive, i.e. the criterion never recommends a filter that increases space. The criterion is conservative, however, for the unique distribution: there is a range of $\alpha$ values (roughly $0.71$--$0.80$ for XOR) where filtering produces small empirical savings (${\sim}0.05$--$0.2$ bits/key) but the lower bound remains negative. This gap is driven by the $n/N$ term, which is large for the unique distribution where $n \approx N(1-\alpha)$. For zipfian and uniform-100, where $n/N \approx 0$, the lower bound crossing aligns closely with the point at which filtering first becomes empirically helpful.

\textbf{Parameter selection.} In practice, AutoCSF does not need to solve for the continuous optimum $\varepsilon^*$. Instead, it enumerates all discrete filter configurations (e.g., fingerprint\_bits $\in \{1, \ldots, 8\}$ for XOR filters, or all $(k, \text{bpe})$ pairs for Bloom filters) and selects the configuration whose lower bound is largest. This discrete search is inexpensive. The parameter space is small and sidesteps any mismatch between the continuous optimum and the nearest discrete parameter. To evaluate how well the lower bound serves as a proxy for actual savings, we compare the parameter selected by the lower bound against the empirically optimal parameter found by exhaustive search. The two agree in the large majority of cases, and when they disagree it is always by a single discrete step, with the theory-guided parameter saving at most $0.02$ bits/key less than the empirical optimum.

\subsection{Comparison with Prior Work}


The closest prior work to AutoCSF is the Bloom-enhanced CSF (BCSF) of Shibuya et al.~\cite{shibuya2022space}. Like AutoCSF, their method augments a CSF with a Bloom filter to handle the majority value. Their approach differs from ours in three respects:
\begin{itemize}
\item \textbf{CSF cost model.} They derive the optimal false positive rate $\varepsilon^*$ using a heuristic cost model $C_{\mathrm{CSF}}(H_0)$ fit empirically to observed CSF sizes, rather than from provable bounds.
\item \textbf{Bloom filter cost model.} Their Bloom filter cost model uses the idealized constant $C_{\mathrm{BF}} = \log_2 e \approx 1.44$ bits per key, which assumes a continuous (real-valued) number of hash functions, rather than a parameterized model that accounts for the discrete choice of hash count $k$ and bits per element.
\item \textbf{Decision criterion.} Their method recommends no filter when $\varepsilon^* \geq 1$, but this criterion is rarely triggered in practice: the heuristic $C_{\mathrm{CSF}}(H_0)$ produces values large enough that the method recommends filtering even at $\alpha = 0.50$, where filtering does not reduce space.
\end{itemize}

To compare the two approaches, we restrict AutoCSF to Bloom filters only (selecting the best $(k, \text{bpe})$ pair via the lower bound) and measure the resulting bits per key across all three distributions. Figure~\ref{fig:shibuya_bpk} shows the measured bits per key for each method alongside a no-filter baseline.


The comparison reveals a clear difference in the \emph{decision} to filter. At low $\alpha$, the heuristic recommends filter configurations that \emph{increase} space usage relative to a plain CSF, with the heuristic curve exceeding the no-filter baseline by up to 1 bit/key for the unique distribution at $\alpha = 0.50$. This gap persists across all three distributions up to $\alpha \approx 0.7$--$0.8$ depending on the minority entropy. AutoCSF avoids this by defaulting to a plain CSF whenever the lower bound is negative. When filtering \emph{is} beneficial (roughly $\alpha \gtrsim 0.8$), both methods achieve comparable space usage, with AutoCSF matching or slightly improving upon the heuristic. These results confirm that AutoCSF improves upon the heuristic both in the decision of when to apply a filter and in the selection of filter parameters.



We restrict both methods to Bloom filters in this comparison for fairness, since the BCSF framework only supports Bloom filters. However, as shown in the alpha sweep experiments, AutoCSF can also leverage XOR filters, binary fuse filters, and other set membership structures, which consistently yield further space savings.



\begin{figure*}[!t]
\includegraphics[width=0.8\textwidth]{vldb2026/figures/theory_validation/shibuya_comparison.png}
\caption{Results of comparing heuristic and theoretical cost models}
\label{fig:shibuya_bpk}
\end{figure*}



\section{Experiments}


The previous section validates AutoCSF's theoretical bounds and decision criterion in isolation. We now evaluate AutoCSF as a complete, end-to-end index and compare it against four baselines that span the landscape of practical approaches to static key-value indexing. Our goal is to answer three questions: (1)~Does AutoCSF's theory-guided filter selection translate into real space savings over competing methods? (2)~What is the latency cost, if any, of filter-augmented CSFs? (3)~How do these trade-offs change on real-world genomics workloads where the value distributions are not synthetically controlled?

\subsection{Baselines}

We compare AutoCSF against the following methods:

\begin{itemize}
\item \textbf{BCSF (Shibuya)}~\cite{shibuya2022space}. The Bloom-enhanced CSF described in Section~3. We use \citeauthor{shibuya2022space}'s heuristic cost model to set the Bloom filter parameters. This baseline isolates the effect of AutoCSF's improved parameter selection: both methods use a CSF with a pre-filter, but differ in how the filter parameters are chosen.

\item \textbf{Sux4J CSF}~\cite{genuzio2020fast}. The standalone CSF implementation in Java. This is the CSF without any filter augmentation, representing the ``plain CSF'' baseline that AutoCSF improves upon when $\alpha$ is sufficiently large. 

\item \textbf{MPH Table}. A minimal perfect hash function (via Sux4J's GOV construction~\cite{genuzio2020fast}) paired with a compact value array. This is a distribution-agnostic baseline as it does not exploit value skew and its memory usage depends only on the vocabulary size.

\item \textbf{Learned CSF}~\cite{hermann2025learned}. A learned static function that replaces the linear-system solver in a CSF with a small neural network trained to predict Huffman codes from keys. The model is augmented with a Bloom filter and a correction table. This method typically achieves the smallest memory footprint, but at the cost of significantly higher query latency and construction time due to model training.
\end{itemize}

\subsection{Setup}

\textbf{Synthetic datasets.} We generate key-value datasets with $N = 100{,}000$ keys, sweeping the majority fraction $\alpha$ from 0.50 to 0.99. The majority value is assigned to an $\alpha$-fraction of keys uniformly at random; the remaining $(1-\alpha)N$ minority keys draw values from one of three distributions: \emph{Uniform-100} (100 equally likely symbols), \emph{Zipfian} (power-law with exponent $s = 1.5$), and \emph{Unique} (every minority key has a distinct value). These distributions span a wide range of minority entropy, from low (Uniform-100) to maximal (Unique), and stress-test different aspects of each method's compression strategy.

\textbf{Genomics datasets.} We evaluate on the three real $k$-mer count datasets used in \cite{shibuya2022space} derived from whole-genome sequencing data, using $k = 15$:
\begin{itemize}
\item \emph{E.\ coli} (Sakai strain): $n = 5.3$M keys, $\alpha = 0.97$, 42 distinct values.
\item \emph{SRR10211353}: $n = 9.8$M keys, $\alpha = 0.20$, 228 distinct values.
\item \emph{C.\ elegans}: $n = 69.7$M keys, $\alpha = 0.82$, 760 distinct values.
\end{itemize}
These datasets exhibit the diversity of real genomics workloads: E.\ coli has extreme skew ($\alpha = 0.97$) with very few distinct values, SRR has low skew ($\alpha = 0.20$) with moderate vocabulary, and C.\ elegans is a large-scale dataset (70M keys) with intermediate skew.

\textbf{Metrics.} We report three metrics: \emph{memory} in bits per key (bpk), \emph{query latency} as the mean inference time per key in nanoseconds, and \emph{construction time} in seconds. For AutoCSF and BCSF, we use binary fuse filters~\cite{graf2020xor}, which offer lower false positive rates per bit than Bloom filters. All experiments are run on an M4 Macbook Pro.

\subsection{Synthetic Benchmarks}

\textbf{Pareto frontier.} Figure~\ref{fig:pareto} plots memory (bpk) against query latency (ns) on log-log axes for all five methods across the three synthetic distributions plus genomics data. Each method traces a trajectory as $\alpha$ sweeps from 0.5 to 0.99; larger markers highlight $\alpha \in \{0.5, 0.8, 0.95\}$.

\begin{figure*}[t]
\centering
\includegraphics[width=\textwidth]{vldb2026/figures/theory_validation/paper_pareto.png}
\caption{Memory--latency Pareto frontier across synthetic distributions and genomics datasets ($N = 100{,}000$ for synthetic, real sizes for genomics). Each trajectory sweeps $\alpha$ from 0.5 (right, more memory) to 0.99 (left, less memory). Log-log scale.}
\label{fig:pareto}
\end{figure*}


Several patterns emerge from the Pareto plot. AutoCSF and BCSF consistently occupy the lower-left corner of the Pareto frontier---low memory \emph{and} low latency---across all three distributions. Sux4J CSF uses comparable memory at low $\alpha$ but carries a latency penalty of 30--90\% over AutoCSF (due to our faster C++ implementation), and its memory advantage disappears at high $\alpha$ where it cannot exploit the majority value. MPH Table uses 5--20$\times$ more memory than AutoCSF and its latency is 4--7$\times$ higher. Learned CSF achieves the tightest memory on several operating points, most notably on the Unique distribution, where its learned model captures per-key structure that analytical methods cannot exploit (e.g., 6.9~bpk vs.\ AutoCSF's 10.6~bpk at $\alpha = 0.8$). However, this memory advantage comes at a steep cost in query latency: 1--4 orders of magnitude higher than AutoCSF, ranging from 6$\mu$s on Uniform-100 to 77$\mu$s on Zipfian at $\alpha = 0.5$ and up to 4 \emph{seconds} per query on Unique at $\alpha = 0.5$. The log-log scale in Figure~\ref{fig:pareto} is necessary to display these extremes on the same axes.

\textbf{Memory scaling with $\alpha$.} Figure~\ref{fig:mem_alpha} isolates the memory dimension by plotting bits per key against $\alpha$ for each distribution. As $\alpha$ increases, all CSF-based methods improve because there is less minority entropy to encode. AutoCSF and BCSF track each other closely, with AutoCSF achieving a 5--10\% memory advantage due to better filter parameter selection. Sux4J CSF is competitive at low $\alpha$ (e.g., 5.0 vs.\ 5.2~bpk at $\alpha = 0.5$, Uniform-100) but diverges at high $\alpha$: at $\alpha = 0.99$, Sux4J uses 1.4~bpk versus AutoCSF's 0.2~bpk, a 7$\times$ gap, because it cannot filter out the majority value. MPH Table's memory \emph{increases} with $\alpha$ (from 35 to 58~bpk) because its per-key cost is fixed by the vocabulary size, not the frequency distribution.

\begin{figure*}[t]
\centering
\includegraphics[width=\textwidth]{vldb2026/figures/theory_validation/paper_memory_vs_alpha.png}
\caption{Memory (bits/key) vs.\ majority fraction $\alpha$ for the three synthetic distributions. AutoCSF and BCSF exploit increasing skew; Sux4J CSF improves more slowly; MPH Table is distribution-agnostic.}
\label{fig:mem_alpha}
\end{figure*}


The Unique distribution is the hardest case for all methods: at $\alpha = 0.5$, half the keys have distinct values, so the CSF must store nearly $\log_2(N/2) \approx 16$ bits of entropy per minority key. AutoCSF (26.3~bpk) outperforms Sux4J CSF (41.9~bpk) by 37\%  due to implementation differences in our C++ package. Learned CSF achieves the best memory on this distribution (11.4~bpk at $\alpha = 0.5$), as the learned model can exploit per-key patterns that are invisible to Huffman coding. However, as Table~\ref{tab:synthetic} shows, this memory advantage comes at higher latency (4M~ns) and construction time (16 minutes). At higher skew ($\alpha = 0.95$), AutoCSF closes the gap to 2.7~bpk vs.\ Learned CSF's 3.5~bpk, with $1{,}300\times$ faster queries.

\textbf{Construction time.} Table~\ref{tab:synthetic} reports memory, latency, and construction time for all five methods at $\alpha \in \{0.5, 0.8, 0.95\}$. AutoCSF and BCSF build in under 0.1 seconds across all configurations. Sux4J CSF is 5--10$\times$ slower due to implementation improvements (our implementation in C++). MPH Table is comparable to Sux4J in construction time. Learned CSF is the most expensive to build: 5--983 seconds depending on distribution complexity, with the Unique distribution requiring over 16 minutes at $\alpha = 0.5$ due to the large number of distinct values the model must learn. At $\alpha = 0.95$ on Uniform-100, where Learned CSF's memory advantage over AutoCSF is only 0.1~bpk, it takes 320$\times$ longer to construct.

\begin{table*}[t]
\centering
\begin{tabular}{lrrrrrrrrr}
\toprule
 & \multicolumn{3}{c}{$\alpha\!=\!0.5$} & \multicolumn{3}{c}{$\alpha\!=\!0.8$} & \multicolumn{3}{c}{$\alpha\!=\!0.95$} \\
Method & bpk & ns & build (s) & bpk & ns & build (s) & bpk & ns & build (s) \\
\midrule
\multicolumn{10}{c}{\emph{Uniform-100}} \\
\midrule
AutoCSF & 5.2 & \textbf{239} & 0.043 & 2.5 & 255 & \textbf{0.027} & 0.8 & \textbf{178} & \textbf{0.018} \\
BCSF (Shibuya) & 5.5 & 319 & \textbf{0.042} & 2.7 & \textbf{250} & 0.027 & 0.8 & 214 & 0.019 \\
Sux4J CSF & 5.0 & 346 & 0.415 & 2.8 & 337 & 0.184 & 1.7 & 326 & 0.189 \\
MPH Table & 35.0 & 1412 & 0.569 & 49.0 & 1505 & 0.438 & 55.9 & 1551 & 0.572 \\
Learned CSF & \textbf{4.4} & 6435 & 5.173 & \textbf{2.2} & 6160 & 5.480 & \textbf{0.7} & 3199 & 5.733 \\
\midrule
\multicolumn{10}{c}{\emph{Zipfian}} \\
\midrule
AutoCSF & \textbf{4.6} & \textbf{265} & 0.036 & \textbf{2.3} & \textbf{239} & \textbf{0.023} & \textbf{0.8} & \textbf{180} & \textbf{0.017} \\
BCSF (Shibuya) & 4.9 & 283 & \textbf{0.035} & 2.4 & 247 & 0.027 & 0.8 & 229 & 0.019 \\
Sux4J CSF & 5.0 & 397 & 0.291 & 2.9 & 355 & 0.185 & 1.8 & 333 & 0.169 \\
MPH Table & 35.4 & 1195 & 0.569 & 49.2 & 1344 & 0.490 & 56.0 & 1323 & 0.470 \\
Learned CSF & 4.9 & 76811 & 32.064 & 2.4 & 39387 & 22.474 & 1.4 & 13530 & 13.284 \\
\midrule
\multicolumn{10}{c}{\emph{Unique}} \\
\midrule
AutoCSF & 26.3 & \textbf{372} & \textbf{0.081} & 10.6 & \textbf{282} & \textbf{0.047} & \textbf{2.7} & \textbf{188} & \textbf{0.025} \\
BCSF (Shibuya) & 27.3 & 400 & 0.084 & 11.0 & 341 & 0.051 & 2.7 & 236 & 0.026 \\
Sux4J CSF & 41.9 & 667 & 0.559 & 17.2 & 377 & 0.339 & 5.1 & 330 & 0.198 \\
MPH Table & 43.9 & 1363 & 0.496 & 52.2 & 1236 & 0.487 & 56.4 & 1390 & 0.472 \\
Learned CSF & \textbf{11.4} & 3997545 & 983.000 & \textbf{6.9} & 1431918 & 359.657 & 3.5 & 243241 & 85.772 \\
\bottomrule
\end{tabular}
\caption{Synthetic benchmark results ($N = 100{,}000$). Memory in bits/key (bpk), query latency in nanoseconds (ns), and construction time in seconds. \textbf{Bold} indicates best in each column.}
\label{tab:synthetic}
\end{table*}

\subsection{Genomics Benchmarks}

Table~\ref{tab:genomics} reports results on the three genomics datasets. The datasets span a wide range of skew: E.\ coli is highly skewed ($\alpha = 0.97$), C.\ elegans has moderate skew ($\alpha = 0.82$), and SRR has low skew ($\alpha = 0.20$). This diversity tests whether AutoCSF's decision criterion correctly adapts its strategy to the data.
\begin{table*}[t]
\centering
\begin{tabular}{lrrrrrrrrr}
\toprule
 & \multicolumn{3}{c}{E.\ coli ($n$=5.3M, $\alpha$=0.97)} & \multicolumn{3}{c}{SRR ($n$=9.8M, $\alpha$=0.20)} & \multicolumn{3}{c}{C.\ elegans ($n$=69.7M, $\alpha$=0.82)} \\
Method & bpk & ns & build (s) & bpk & ns & build (s) & bpk & ns & build (s) \\
\midrule
AutoCSF & \textbf{0.30} & \textbf{335} & \textbf{0.7} & 4.20 & \textbf{458} & \textbf{4.3} & \textbf{1.23} & 394 & \textbf{13.7} \\
BCSF (Shibuya) & 0.34 & 432 & 0.9 & 4.23 & 594 & 4.4 & 1.36 & \textbf{386} & 15.1 \\
Sux4J CSF & 1.24 & 598 & 1.3 & 3.59 & 621 & 4.6 & 1.59 & 853 & 17.8 \\
MPH Table & 11.55 & 1666 & 8.1 & 11.55 & 1716 & 13.6 & 11.55 & 2054 & 136.3 \\
Learned CSF & 0.33 & 1725 & 127.4 & \textbf{3.49} & 8312 & 501.4 & --- & --- & --- \\
\bottomrule
\end{tabular}
\caption{Genomics benchmark results. Memory in bits/key (bpk), query latency in nanoseconds (ns), and construction time in seconds. ``---'' indicates we did not complete the experiment due to lack of time. \textbf{Bold} indicates best in each column.}
\label{tab:genomics}
\end{table*}

On E.\ coli, where $\alpha = 0.97$, AutoCSF achieves 0.30~bpk, less than one-third of a bit per key to index 5.3 million $k$-mers. This is $4.1\times$ smaller than Sux4J CSF (1.24~bpk), which cannot exploit the extreme majority dominance, and $38\times$ smaller than MPH Table (11.55~bpk). Learned CSF is competitive in memory (0.33~bpk) but requires 127 seconds to build versus AutoCSF's 0.7 seconds ($182\times$ slower) and has $5.1\times$ higher query latency.

The SRR dataset ($\alpha = 0.20$) represents a case where filter augmentation is not beneficial. Here, Sux4J CSF achieves the best memory (3.59~bpk) among non-learned methods. AutoCSF (4.20~bpk) uses slightly more memory than Sux4J due to differences in our underlying CSF implementation rather than filter overhead. Despite this, AutoCSF achieves the fastest query latency (458~ns vs.\ Sux4J's 621~ns). Learned CSF achieves the best memory (3.49~bpk) but at a cost: 501 seconds to build ($117\times$ slower than AutoCSF) and $18\times$ higher query latency (8312~ns vs.\ 458~ns).


On C.\ elegans ($n = 69.7$M, $\alpha = 0.82$), AutoCSF scales to the largest dataset while maintaining its advantages: 1.23~bpk with 394~ns latency and a 13.7-second build time. For context, this means the entire 70-million-entry $k$-mer count table is indexed in approximately 10.7~MB of memory. MPH Table would require 100.6~MB for the same data, and Sux4J CSF uses 13.8~MB. We did not complete our experiments for Learned CSF on this dataset (due to lack of time).

\subsection{Discussion}

Across all experiments, a consistent picture emerges. AutoCSF dominates the Pareto frontier of memory versus latency whenever the majority fraction $\alpha$ is moderate to high ($\gtrsim 0.7$), which is precisely the regime where filter augmentation is most beneficial and where many real-world workloads---especially in genomics---operate. The margin over heuristic construction is modest (5--13\% in memory) but systematic, confirming that the theory-guided parameter selection from Section~4 translates to measurable improvements in practice over the heuristic approach.

The comparison with Learned CSF is particularly instructive. Learned CSF occasionally achieves slightly lower memory than AutoCSF (by 0.1--0.5~bpk), but this comes at the cost of latency and construction time. These results suggest that for practical deployments where query latency and build time matter, the marginal memory savings of learned approaches do not justify the overhead, especially when AutoCSF achieves comparable compression through a principled analytical framework that requires no model training.

\section{Conclusion}

We presented AutoCSF, a novel, theoretically principled algorithm that determines both \emph{when} to augment a compressed static function with an approximate set membership data structure and \emph{how} to optimally set the parameters for this index to enable fast key-value lookups with a small memory footprint. While the idea of augmenting a CSF with a filter was previously proposed in the literature \cite{shibuya2022space}, the existing state of the art relied on data-driven heuristics, which, as we showed in this paper, can provide misleading guidance in practice. The core innovation in AutoCSF is the development of novel lower and upper bounds on the \emph{difference} in cost between a CSF and filter-augmented CSF, using mathematical tools from information theory. We validate that our theoretical results closely align with empirical observations and provide accurate guidance in parameter selection for practitioners. We also compare AutoCSF to other succinct retrieval systems in the literature, such as BCSF and Learned CSF, on both synthetic and real genomics workloads where we find that AutoCSF provides a Pareto-optimal tradeoff in query latency and memory footprint while maintaining low index build times. 

%\clearpage

\bibliographystyle{ACM-Reference-Format}
\bibliography{sample}

\end{document}
\endinput
