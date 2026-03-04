"""Plot generation for theory validation. Reads JSON data, recomputes theory curves, renders plots."""

import json
import os
import sys

import matplotlib.pyplot as plt
import numpy as np

_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_dir, ".."))

from shared import theory

FIGURES_DIR = os.path.join(_dir, "figures")
DATA_DIR = os.path.join(FIGURES_DIR, "data")

FILTER_LABELS = ["xor", "binary_fuse", "bloom_k1", "bloom_k2", "bloom_k3"]
FILTER_LABELS_COMBINED = ["xor", "binary_fuse", "bloom_k1", "bloom_k3"]
DISTRIBUTIONS = ["unique", "zipfian", "uniform_100"]
EPSILON_SWEEP_ALPHAS = [0.7, 0.9]

FILTER_DISPLAY = {
    "xor": "XOR Filter",
    "binary_fuse": "Binary Fuse Filter",
    "bloom_k1": "Bloom (k=1)",
    "bloom_k2": "Bloom (k=2)",
    "bloom_k3": "Bloom (k=3)",
}

DIST_DISPLAY = {
    "unique": "Unique",
    "zipfian": "Zipfian",
    "uniform_100": "Uniform-100",
}

PARAM_KEY = {
    "xor": "fingerprint_bits",
    "binary_fuse": "fingerprint_bits",
    "bloom_k1": "bpe",
    "bloom_k2": "bpe",
    "bloom_k3": "bpe",
}

PARAM_AXIS_LABEL = {
    "xor": "Fingerprint bits",
    "binary_fuse": "Fingerprint bits",
    "bloom_k1": "Bits per element",
    "bloom_k2": "Bits per element",
    "bloom_k3": "Bits per element",
}


def compute_params(filter_label, param, n_filter=None):
    if filter_label == "xor":
        return theory.xor_params(param)
    elif filter_label == "binary_fuse":
        return theory.binary_fuse_params(param, n_filter)
    elif filter_label.startswith("bloom_k"):
        k = int(filter_label[-1])
        return theory.bloom_params(param, k)


def compute_b_eps(filter_label, eps, n_filter=None):
    if filter_label == "xor":
        return theory.b_eps_xor(eps)
    elif filter_label == "binary_fuse":
        return theory.b_eps_binary_fuse(eps, n_filter)
    elif filter_label.startswith("bloom_k"):
        k = int(filter_label[-1])
        return theory.b_eps_bloom(eps, k)


def compute_theory_best(filter_label, alpha, n_over_N, n_filter=None):
    if filter_label == "xor":
        return theory.best_discrete_xor(alpha, n_over_N)
    elif filter_label == "binary_fuse":
        return theory.best_discrete_binary_fuse(alpha, n_over_N, n_filter)
    elif filter_label.startswith("bloom_k"):
        k = int(filter_label[-1])
        return theory.best_discrete_bloom(alpha, n_over_N, k)


def load_json(path):
    if not os.path.exists(path):
        return None
    with open(path) as f:
        return json.load(f)


def plot_alpha_sweep(data, filter_label, ax=None):
    if ax is None:
        fig, ax = plt.subplots(figsize=(8, 5))
    else:
        fig = ax.figure

    N = data["N"]
    pk = PARAM_KEY[filter_label]

    alphas = []
    lb_vals = []
    ub_at_lb_eps = []
    ub_at_emp_eps = []
    best_empirical = []

    for r in data["results"]:
        alpha = r["alpha"]
        n_over_N = r["n_over_N"]
        n_filter = r.get("n_filter", int(N * (1 - alpha)))

        best_param, _ = compute_theory_best(filter_label, alpha, n_over_N, n_filter)
        b_eps, eps = compute_params(filter_label, best_param, n_filter)

        best_emp_param = r["best_empirical_params"][pk]
        b_eps_emp, eps_emp = compute_params(filter_label, best_emp_param, n_filter)

        alphas.append(alpha)
        lb_vals.append(theory.lower_bound(alpha, eps, b_eps, n_over_N))
        ub_at_lb_eps.append(theory.upper_bound(alpha, eps, b_eps, n_over_N))
        ub_at_emp_eps.append(theory.upper_bound(alpha, eps_emp, b_eps_emp, n_over_N))
        best_empirical.append(r["best_empirical_bpk_saved"])

    ax.plot(
        alphas, ub_at_lb_eps, "b-", linewidth=1, alpha=0.7,
        label=r"UB at $\varepsilon_{\mathrm{LB}}$",
    )
    ax.plot(
        alphas, ub_at_emp_eps, "b-", linewidth=1.5,
        label=r"UB at $\varepsilon_{\mathrm{emp}}$",
    )
    ax.plot(alphas, lb_vals, "b--", linewidth=1.5, label="Lower bound")
    ax.plot(
        alphas,
        best_empirical,
        "r.-",
        linewidth=1.5,
        markersize=4,
        label="Best empirical",
    )
    ax.axhline(y=0, color="gray", linestyle="-", alpha=0.5, linewidth=1)

    for i in range(len(lb_vals) - 1):
        if lb_vals[i] < 0 and lb_vals[i + 1] >= 0:
            cross = alphas[i] + (alphas[i + 1] - alphas[i]) * (-lb_vals[i]) / (
                lb_vals[i + 1] - lb_vals[i]
            )
            ax.axvline(x=cross, color="blue", linestyle=":", alpha=0.5, linewidth=1)
            ax.annotate(
                f"LB=0 @ {cross:.2f}",
                xy=(cross, 0),
                xytext=(cross + 0.02, min(lb_vals) * 0.3),
                fontsize=8,
                color="blue",
            )
            break

    ax.set_xlabel(r"$\alpha$")
    ax.set_ylabel("Bits per key saved")
    ax.set_xlim(0.5, 1.0)
    ax.set_title(
        f"{FILTER_DISPLAY[filter_label]} — {DIST_DISPLAY[data['distribution']]}"
    )
    ax.legend(fontsize=8, loc="upper left")
    ax.grid(True, alpha=0.3)

    return fig, ax


def plot_alpha_sweep_individual(filter_label, dist):
    path = os.path.join(DATA_DIR, f"alpha_sweep_{filter_label}_{dist}.json")
    data = load_json(path)
    if data is None:
        print(f"  Skipping (no data): {path}")
        return

    fig, ax = plt.subplots(figsize=(8, 5))
    plot_alpha_sweep(data, filter_label, ax)
    plt.tight_layout()

    out = os.path.join(
        FIGURES_DIR, "alpha_sweep", f"alpha_sweep_{filter_label}_{dist}.png"
    )
    os.makedirs(os.path.dirname(out), exist_ok=True)
    fig.savefig(out, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  Saved: {out}")


def plot_alpha_sweep_combined(dist):
    fig, axes = plt.subplots(1, len(FILTER_LABELS_COMBINED), figsize=(20, 4), sharey=True)

    for i, fl in enumerate(FILTER_LABELS_COMBINED):
        path = os.path.join(DATA_DIR, f"alpha_sweep_{fl}_{dist}.json")
        data = load_json(path)
        if data is None:
            axes[i].set_visible(False)
            continue
        plot_alpha_sweep(data, fl, axes[i])
        if i > 0:
            axes[i].set_ylabel("")

    fig.suptitle(f"Alpha Sweep — {DIST_DISPLAY[dist]}", fontsize=14, y=1.02)
    plt.tight_layout()

    out = os.path.join(FIGURES_DIR, "combined", f"alpha_sweep_{dist}.png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    fig.savefig(out, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  Saved: {out}")


def plot_epsilon_sweep(data, filter_label, ax=None, show_legend=True):
    if ax is None:
        fig, ax = plt.subplots(figsize=(8, 5))
    else:
        fig = ax.figure

    alpha = data["alpha"]
    n_over_N = data["n_over_N"]
    N = data["N"]
    n_filter = data.get("n_filter", int(N * (1 - alpha)))
    pk = PARAM_KEY[filter_label]

    param_vals = [e[pk] for e in data["empirical_per_param"]]
    bpk_saved = [e["bpk_saved"] for e in data["empirical_per_param"]]
    eps_discrete = [compute_params(filter_label, p, n_filter)[1] for p in param_vals]

    eps_min = min(eps_discrete) * 0.5
    eps_max = min(max(eps_discrete) * 2, 0.999)
    eps_cont = np.logspace(np.log10(eps_min), np.log10(eps_max), 500)
    lb_cont = []
    ub_cont = []
    for e in eps_cont:
        try:
            b = compute_b_eps(filter_label, e, n_filter)
        except (ValueError, ZeroDivisionError):
            b = float("inf")
        lb_cont.append(theory.lower_bound(alpha, e, b, n_over_N))
        ub_cont.append(theory.upper_bound(alpha, e, b, n_over_N))

    ax.fill_between(eps_cont, lb_cont, ub_cont, alpha=0.15, color="blue")
    ax.plot(eps_cont, ub_cont, "b-", linewidth=1, label="Upper bound")
    ax.plot(eps_cont, lb_cont, "b--", linewidth=1.5, label="Lower bound")
    ax.plot(
        eps_discrete, bpk_saved, "ro-", linewidth=2, markersize=6, label="Empirical"
    )
    ax.axhline(y=0, color="gray", linestyle="-", alpha=0.5, linewidth=1)

    # Theory optimal vertical line (continuous optimum of lower bound)
    theory_opt_idx = int(np.argmax(lb_cont))
    eps_theory = float(eps_cont[theory_opt_idx])
    ax.axvline(
        x=eps_theory,
        color="blue",
        linestyle=":",
        alpha=0.7,
        linewidth=1.5,
        label=rf"Theory opt ($\varepsilon^*$={eps_theory:.3f})",
    )

    # Empirical optimal vertical line (best discrete parameter)
    best_idx = int(np.argmax(bpk_saved))
    best_emp_param = param_vals[best_idx]
    ax.axvline(
        x=eps_discrete[best_idx],
        color="red",
        linestyle=":",
        alpha=0.7,
        linewidth=1.5,
        label=f"Empirical opt ({pk}={best_emp_param})",
    )

    ax.set_xscale("log")
    ax.set_xlabel(r"$\varepsilon$ (false positive rate)")
    ax.set_ylabel("Bits per key saved")
    ax.set_xlim(eps_min, eps_max)

    ax2 = ax.twiny()
    ax2.set_xscale("log")
    ax2.set_xlim(ax.get_xlim())
    ax2.set_xticks(eps_discrete)
    ax2.set_xticklabels([str(p) for p in param_vals], fontsize=7)
    ax2.set_xlabel(PARAM_AXIS_LABEL[filter_label], fontsize=9)

    ax.set_title(
        f"{FILTER_DISPLAY[filter_label]} — {DIST_DISPLAY[data['distribution']]} "
        f"($\\alpha$={alpha})",
        fontsize=11,
    )
    if show_legend:
        ax.legend(fontsize=7, loc="lower left", framealpha=0.9, edgecolor="gray")
    ax.grid(True, alpha=0.3)

    return fig, ax


def plot_epsilon_sweep_individual(filter_label, dist, alpha):
    path = os.path.join(
        DATA_DIR, f"epsilon_sweep_{filter_label}_{dist}_alpha{alpha}.json"
    )
    data = load_json(path)
    if data is None:
        print(f"  Skipping (no data): {path}")
        return

    fig, ax = plt.subplots(figsize=(8, 5))
    plot_epsilon_sweep(data, filter_label, ax)
    plt.tight_layout()

    out = os.path.join(
        FIGURES_DIR,
        "epsilon_sweep",
        f"epsilon_sweep_{filter_label}_{dist}_alpha{alpha}.png",
    )
    os.makedirs(os.path.dirname(out), exist_ok=True)
    fig.savefig(out, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  Saved: {out}")


def plot_epsilon_sweep_combined(dist, alpha):
    fig, axes = plt.subplots(
        1, len(FILTER_LABELS_COMBINED), figsize=(20, 5),
    )

    for i, fl in enumerate(FILTER_LABELS_COMBINED):
        path = os.path.join(DATA_DIR, f"epsilon_sweep_{fl}_{dist}_alpha{alpha}.json")
        data = load_json(path)
        if data is None:
            axes[i].set_visible(False)
            continue
        plot_epsilon_sweep(data, fl, axes[i], show_legend=False)
        if i > 0:
            axes[i].set_ylabel("")

    # Shared legend: collect common entries from first visible subplot
    # (skip per-subplot entries like "Theory opt (fp=X)" which differ)
    common_labels = {"Upper bound", "Lower bound", "Empirical"}
    handles, labels = [], []
    for ax_i in axes:
        if not ax_i.get_visible():
            continue
        for h, l in zip(*ax_i.get_legend_handles_labels()):
            if l in common_labels and l not in labels:
                handles.append(h)
                labels.append(l)
        break
    fig.legend(
        handles, labels, loc="upper center", bbox_to_anchor=(0.5, -0.01),
        ncol=len(labels), fontsize=9, framealpha=0.9,
    )

    fig.suptitle(
        f"Epsilon Sweep — {DIST_DISPLAY[dist]} ($\\alpha$={alpha})",
        fontsize=14,
        y=1.05,
    )
    plt.tight_layout()

    out = os.path.join(
        FIGURES_DIR, "combined", f"epsilon_sweep_{dist}_alpha{alpha}.png"
    )
    os.makedirs(os.path.dirname(out), exist_ok=True)
    fig.savefig(out, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"  Saved: {out}")


def main():
    plt.rcParams.update(
        {
            "font.size": 10,
            "axes.labelsize": 11,
            "axes.titlesize": 12,
            "legend.fontsize": 8,
            "xtick.labelsize": 9,
            "ytick.labelsize": 9,
            "lines.linewidth": 1.5,
        }
    )

    print("=== Individual alpha sweep plots ===")
    for fl in FILTER_LABELS:
        for dist in DISTRIBUTIONS:
            plot_alpha_sweep_individual(fl, dist)

    print("\n=== Combined alpha sweep plots ===")
    for dist in DISTRIBUTIONS:
        plot_alpha_sweep_combined(dist)

    print("\n=== Individual epsilon sweep plots ===")
    for fl in FILTER_LABELS:
        for dist in DISTRIBUTIONS:
            for alpha in EPSILON_SWEEP_ALPHAS:
                plot_epsilon_sweep_individual(fl, dist, alpha)

    print("\n=== Combined epsilon sweep plots ===")
    for dist in DISTRIBUTIONS:
        for alpha in EPSILON_SWEEP_ALPHAS:
            plot_epsilon_sweep_combined(dist, alpha)


if __name__ == "__main__":
    main()
