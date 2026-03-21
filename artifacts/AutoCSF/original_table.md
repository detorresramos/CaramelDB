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