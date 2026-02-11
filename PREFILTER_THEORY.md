\documentclass{article}


% if you need to pass options to natbib, use, e.g.:
%     \PassOptionsToPackage{numbers, compress}{natbib}
% before loading neurips_2024

\usepackage{booktabs} % For formal tables
\usepackage{balance}
\usepackage{graphicx}
\usepackage{subcaption}
\usepackage{amsmath}
% \usepackage[linesnumbered,ruled]
% \usepackage{algorithm2e}
% \usepackage{algorithmic}
% \usepackage{algorithm}
% \usepackage[noend]{algpseudocode}

\usepackage{algorithm}
\usepackage{algorithmicx}
\usepackage{algpseudocode}


\usepackage{bbm}

\usepackage{pifont}
\usepackage{amsmath} % for ams mathematical facilities
\usepackage{mathtools} % for \DeclarePairedDelimiter

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
\DeclarePairedDelimiter{\floor}{\lfloor}{\rfloor}
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






\usepackage{multirow}
\usepackage{capt-of}% or \usepackage{caption}
%\usepackage{booktabs}
\usepackage{varwidth}
% \usepackage[switch]{lineno} 
\DeclareMathAlphabet\mathbfcal{OMS}{cmsy}{b}{n}
\newcommand{\ten}[1]{\mathbfcal{#1}}
\newcommand{\mat}[1]{\mathbf{#1}}

% For author name commands:
\DeclareRobustCommand{\ben}[1]{ {\textcolor{blue}{(Ben: #1)}}}

% ready for submission
\usepackage[preprint]{neurips_2024}


% to compile a preprint version, e.g., for submission to arXiv, add add the
% [preprint] option:
%     \usepackage[preprint]{neurips_2024}


% to compile a camera-ready version, add the [final] option, e.g.:
%     \usepackage[final]{neurips_2024}


% to avoid loading the natbib package, add option nonatbib:
%    \usepackage[nonatbib]{neurips_2024}


\usepackage[utf8]{inputenc} % allow utf-8 input
\usepackage[T1]{fontenc}    % use 8-bit T1 fonts
\usepackage{hyperref}       % hyperlinks
\usepackage{url}            % simple URL typesetting
\usepackage{booktabs}       % professional-quality tables
\usepackage{amsfonts}       % blackboard math symbols
\usepackage{nicefrac}       % compact symbols for 1/2, etc.
\usepackage{microtype}      % microtypography
\usepackage{xcolor}         % colors

\title{Prefiltering Theory}

\author{}


\begin{document}


\maketitle


\section*{Shibuya's Derivation}

Let $K$ denote the set of all kmers in our dataset and let $K_0$ denote the set of kmers with the most common frequency. We have that $|K_0| = \alpha |K|$ for some $0 \le \alpha \le 1$. Our goal is to minimize space by storing the set of non-dominating values in a bloom filter and the associated counts of these non-dominating values in a CSF (plus the false positives). \\

Assume that the bloom filter takes $C_{BF} \frac{1}{\varepsilon}$ bits per key where $\varepsilon$ denotes the false positive rate. Also assume the CSF takes $C_{CSF}$ bits per key. The total space usage of this Bloom-enhanced CSF data structure is thus $$C_{BF} (1-\alpha) |K| \log \frac{1}{\varepsilon} + C_{CSF} |K| ((1-\alpha) + \varepsilon \alpha)$$ where we assume the logarithm is base 2. \\

To determine if we need a Bloom filter, we need to verify if the following inequality holds $$C_{BF} (1-\alpha) |K| \log \frac{1}{\varepsilon} + C_{CSF} |K| ((1-\alpha) + \varepsilon \alpha) < C_{CSF} |K|$$

We can rewrite this inequality as $$\frac{C_{BF}}{C_{CSF}} \left(\frac{1-\alpha}{\alpha}\right) \log \frac{1}{\varepsilon} + \varepsilon < 1$$

The left-hand side of the previous inequality reaches its minimum at $$\varepsilon^* = \frac{C_{BF}}{C_{CSF}} \left(\frac{1-\alpha}{\alpha}\right) \log e$$

This gives us our decision criteria. We conclude that the Bloom filter helps reduce space if $\varepsilon^* < 1$ and $$\alpha > \frac{C_{BF} \log e}{C_{CSF} + C_{BF}\log e}$$

If $\alpha$ is sufficiently large, we can then construct a bloom filter with false positive rate $\varepsilon^*$. 

The question remains: how do we compute $C_{BF}$ and $C_{CSF}$? The Shibuya paper estimates $C_{BF}$ by the theoretical space coefficient of idealized Bloom filters of 1.44. \textbf{This is not realistic and we can do better by having a more accurate bloom filter cost model parameterized by the number of hash functions we use}. 

Secondly, Shibuya, et al. estimate $C_{CSF}$ empirically in terms of the empirical entropy $H_0$, producing the equation 

\[
C_{CSF} = 
\begin{cases} 
0.22H_0^2 + 0.18H_0 + 1.16, \quad \text{if } H_0 < 2 \\

1.1H_0 + 0.2 \quad \text{otherwise}
\end{cases}
\]

\textbf{This is a heuristic and not a theoretically principled cost model. Can we find real examples where this breaks and gives a suboptimal recommendation? }

\section*{Improved Analysis} 

\begin{theorem}
\label{thm:HuffmanOptimality}
Given frequencies $F = \{f_1, f_2, ... f_{z}\}$ of values $V = \{v_1, v_2, ... v_{z}\}$ and a Huffman code $H(F)$ with lengths $\{l_1, l_2, ... l_{z}\}$,
$$ \sum_{i = 1}^{z} f_i l_i \leq \sum_{i = 1}^{z} f_i l'_i$$
for any other uniquely decodable code with lengths $l'_i$.
\end{theorem}


\begin{theorem}
Let $C_{\mathrm{FSF}}$ and $C_{CSF}$ denote the memory costs of filtered and non-filtered CSFs, where the filter is a set membership oracle with false positive rate $\epsilon$ that requires $b(\epsilon)$ bits per key.
$$ \left(C_{\mathrm{CSF}} - \mathbb{E}[C_{\mathrm{FSF}}]\right) / N \geq \alpha \delta (1 - \epsilon) - (1 - \alpha)b(\epsilon) - n / N$$
Note that the vocabulary size $n$ is typically $o(N)$, making $n / N = o(1)$.
\end{theorem}

\begin{proof}

\textbf{Notation:} We suppose that there are $n$ unique values, sorted in order of decreasing frequency $V = \{v_0, ... v_{n}\}$. We use $e$ to denote the Huffman encoding, $l_i$ for the length of the encoding $e(v_i)$ and $f_i$ for the frequency of $v_i$.

\textbf{Cost of the CSF:} The cost to store a compressed static function of $n = |V|$ distinct values is:

$$ C_{\mathrm{CSF}} = \delta \sum_{i = 0}^{n} f_i l_i + D_V + D_e$$

where $\delta$ is a constant that depends on whether $3$ or $4$ hash functions are used to construct the linear system. With $3$ hashes, $\delta_3 \approx 1.089$ and with $4$ hashes, $\delta_4 \approx 1.024$. The canonical Huffman dictionary requires $D_V$ space to store the unique values and $D_e = n\lceil \log_2 \max_i l_i\rceil $ bits to store the encoding lengths for the set of values.

\textbf{Cost of the filtered CSF:} When we prefilter the CSF, we remove (some of) the majority keys from the CSF and change the frequency distribution for the Huffman encoding. Specifically, we reduce the frequency of the majority value from $f_0$ to $f'_0$, causing the Huffman encoding to change from $e$ to $e'$.
Removal of the majority value can potentially update all of the Huffman code lengths, causing them to change from $\{l_0, ... l_{n}\}$ to $\{l'_0, ... l'_{n}\}$. The cost $D_V$ to store the vocabulary remains the same.

% Suppose we construct a Bloom filter with false-positive rate $\epsilon$. This introduces an overhead of $1.44 \log_2 \epsilon^{-1}$ bits per item in the filter~\cite{bloom1970space} and causes the Huffman encoding to change from $e$ to $e'$:

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

$$ \delta \left(f_0 l_0 + \sum_{i = 1}^n f_i l_i - f'_0 l'_0 - \sum_{i=1}^n f_i l'_i\right) \geq \left(f_0 l_0 + \sum_{i = 1}^n f_i l_i - f'_0 l_0 - \sum_{i=1}^n f_i l_i\right) = l_0 (f_0 - f'_0)$$

\textbf{Bounding the Huffman codebook size:} To bound the size of the difference $D_{e'} - D_{e}$, we rely on some basic properties of canonical Huffman codes.
Canonical Huffman codes only need to store the lengths of each code in $e(V)$ and $e'(V)$, not the codes themselves. Let $l_{\max} = \max_i l_i$ and $l'_{\max} = \max_i l'_i$. 

Then
$$D_e = n\lceil \log_2 l_{\max} \rceil \qquad D_{e'} = n\lceil \log_2 l'_{\max}\rceil$$

By replacing $f_0$ with $f'_0$, we increase the depth of the Huffman tree for $e'$ by at most 1. Therefore, $l'_{\max} \leq l_{\max} + 1$. This implies the following bound on the canonical Huffman codebooks.

$$ D_{e'} - D_{e} \leq n \lceil \log_2 (l_{\max}+1) \rceil - z\lceil \log_2 l_{\max} \rceil \leq n$$

\textbf{Taking the expectation:} By combining the previous two parts of the proof, we have the bound 

\begin{align*}
C_{\mathrm{CSF}} - C_{\mathrm{FSF}} &\geq \delta l_0 (f_0 - f'_0) \\
& -n \\
& - b(\epsilon) (N - f_0) 
\end{align*}



The headroom expression $C_{\mathrm{CSF}} - C_{\mathrm{FSF}}$ contains $f'_0$, the number of keys with value $v_0$ that erroneously pass through the filter. It should be noted that $f'_0$ is a random variable, where the randomness comes from the hash functions and other randomization techniques used in the approximate set membership algorithm. 

Because the set membership algorithm returns a false positive with probability $\epsilon$, the expected value of $f'_0$ is $\mathbb{E}[f'_0] = \epsilon f_0$. Thus we have

\begin{align*}
C_{\mathrm{CSF}} - \mathbb{E}[C_{\mathrm{FSF}}] &\geq \delta l_0 f_0 (1 - \epsilon) \\
& -n \\
& -b(\epsilon) (N - f_0) 
\end{align*}

To complete the proof, we divide both sides of the equation by $N$, the total number of keys in the key-value store.
\begin{align*}
\frac{1}{N}\left(C_{\mathrm{CSF}} - \mathbb{E}[C_{\mathrm{FSF}}]\right) &\geq \delta l_0 \alpha (1 - \epsilon) \\
& -n / N \\
& -b(\epsilon) (1 - \alpha) 
\end{align*}

\end{proof}


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

$$-\frac{f_0'}{N'} \log_2\frac{f_i'}{N'} - \sum_{i=1}^{n} \frac{f_i}{N'} \log_2 \frac{f_i}{N'} \leq \frac{f_0'}{N} l_0' + \sum_{i=1}^{n}\frac{f_i}{N}l_i' \leq 1 - \frac{f_0'}{N'} \log_2 \frac{f_0'}{N'} - \sum_{i=1}^{n} \frac{f_i}{N'} \log_2 \frac{f_i}{N'}$$

Note that $N' = N - f_0 + f_0'$ because the two Huffman codes are constructed on different sets: one on the original data and the second on the filtered data.

This allows us to bound the following expression:
$$Z = f_0 l_0 + \sum_{i=1}^{n} f_i l_i - f_0' l_0' - \sum_{i=1}^{n} f_i l_i'$$
$$ \leq N - f_0 \log_2 \frac{f_0}{N} + f_0' \log_2 \frac{f_0'}{N'} - \sum_{i=1}^{n}f_i\left(\log_2 \frac{f_i}{N} - \log_2 \frac{f_i}{N'}\right)$$
$$ = N - f_0 \log_2 \frac{f_0}{N} + f_0' \log_2 \frac{f_0'}{N'} - \sum_{i=1}^{n}f_i\left(\log_2 N' - \log_2 N\right)$$
$$ = N - f_0 \log_2 \frac{f_0}{N} + f_0' \log_2 \frac{f_0'}{N'} - \log_2\left(\frac{N'}{N}\right)\sum_{i=1}^{n}f_i$$

For the $f_0' \log_2 f_0' / N'$ term, note that
$$f_0' \log_2 \frac{f_0'}{N'} = f_0' \log_2 \left(\frac{f_0'}{N-f_0 + f_0'}\right) = f_0' \log_2 \left(\frac{1}{\frac{N-f_0}{f_0'} + 1}\right) = -f_0' \log_2\left(1 + \frac{N-f_0}{f_0'}\right)$$
Recall the inequality
$-\ln(1 + z) \leq -\frac{2z}{2+z}$ for $z \geq 0$.
% Reference: Some bounds for the logarithmic function by Flemming Topsoe (https://rgmia.org/papers/v7n2/pade.pdf)
Applying this inequality to our expression with $z = (N-f_0)/f_0'$, we obtain

$$ -f_0'\log_2\left(1 + \frac{N-f_0}{f_0'}\right) \leq -2f_0'\frac{N-f_0}{f_0'\left(2 + \frac{N-f_0}{f_0'}\right)} = -2f_0'\frac{N-f_0}{2f_0' + N-f_0} \leq -2f_0'\frac{N-f_0}{3N - f_0}$$
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

By combining this bound with the previous bound on $D_e - D_{e'}$ we have

\begin{align*}
C_{\mathrm{CSF}} - C_{\mathrm{FSF}} &= \delta \left(N - f_0 \log_2 \alpha - 2f_0' \frac{1-\alpha}{3-\alpha} - (N-f_0)\log_2 (1-\alpha)\right) \\
& + n \\
& - b(\epsilon) (N - f_0) 
\end{align*}

Taking the expectation and dividing by $N$, we obtain
\begin{align*}
\mathbb{E}[C_{\mathrm{CSF}} - C_{\mathrm{FSF}}] / N &\leq \delta \left(1 - \alpha \log_2 \alpha - 2\alpha \epsilon \frac{1-\alpha}{3-\alpha} - (1-\alpha)\log_2 (1-\alpha)\right) \\
& + n/N \\
& - b(\epsilon) (N - f_0)
\end{align*}
Observe that $-\alpha \log_2 \alpha - (1-\alpha) \log_2(1-\alpha)$ is the entropy of a Bernoulli trial with success rate $\alpha$, which is trivially bounded above by 1 bit. This gives rise to the expression
$$\mathbb{E}[C_{\mathrm{CSF}}- C_{\mathrm{FSF}}] / N  \leq 2\delta - 2\alpha  \delta \epsilon \frac{1-\alpha}{3-\alpha} + n/N 
- b(\epsilon) (1 - \alpha)$$
which can be further simplified to
$$\mathbb{E}[C_{\mathrm{CSF}}- C_{\mathrm{FSF}}] / N  \leq 2\delta - \frac{1}{2}\alpha \delta \epsilon + n/N 
- b(\epsilon) (1 - \alpha)$$
\end{proof}

Putting the two bounds together,

$$ \alpha \delta (1-\epsilon) - (1-\alpha) b(\epsilon) - n/N \leq \mathbb{E}[C_{\mathrm{CSF}}- C_{\mathrm{FSF}}] / N  \leq 2\delta - \frac{1}{2}\alpha \epsilon
- b(\epsilon) (1 - \alpha) + n/N $$
The gap is
$$ 2\delta + \frac{1}{2}\alpha \delta \epsilon - \alpha \delta$$
which, as expected, is greater than 0.




\end{document}