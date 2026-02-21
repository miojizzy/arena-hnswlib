#!/usr/bin/env python3
"""
Simple VectorDBBench-style benchmark for arena-hnswlib.
This script tests the client in isolation without the full VectorDBBench framework.
"""

import multiprocessing as mp
import numpy as np
import time
import sys


def run_insert_test(dim, n_vectors, m, ef_construction, metric, result_queue):
    """Run insert test in a separate process."""
    import arena_hnswlib

    np.random.seed(42)
    data = np.random.randn(n_vectors, dim).astype(np.float32)
    ids = np.arange(n_vectors, dtype=np.uint32)

    # Create index
    index = arena_hnswlib.HNSW(
        dim=dim,
        max_elements=n_vectors * 2,
        m=m,
        ef_construction=ef_construction,
        metric=metric,
    )

    # Time insert
    start = time.perf_counter()
    index.add_items(data, ids)
    elapsed = time.perf_counter() - start

    result_queue.put(("insert", n_vectors / elapsed))
    print(f"Insert: {n_vectors / elapsed:.2f} vectors/sec")


def run_search_test(dim, n_vectors, n_queries, k, ef_search, metric, result_queue):
    """Run search test in a separate process."""
    import arena_hnswlib

    np.random.seed(42)
    data = np.random.randn(n_vectors, dim).astype(np.float32)
    ids = np.arange(n_vectors, dtype=np.uint32)
    queries = np.random.randn(n_queries, dim).astype(np.float32)

    # Create and populate index
    index = arena_hnswlib.HNSW(
        dim=dim,
        max_elements=n_vectors * 2,
        m=16,
        ef_construction=200,
        metric=metric,
    )
    index.add_items(data, ids)
    index.ef_search = ef_search

    # Time search
    start = time.perf_counter()
    for query in queries:
        result_ids, dists = index.query(query.reshape(1, -1), k)
    elapsed = time.perf_counter() - start

    result_queue.put(("search", n_queries / elapsed))
    print(f"Search: {n_queries / elapsed:.2f} queries/sec")


def run_recall_test(dim, n_vectors, n_queries, k, metric, result_queue):
    """Run recall test in a separate process."""
    import arena_hnswlib

    np.random.seed(123)
    data = np.random.randn(n_vectors, dim).astype(np.float32)
    ids = np.arange(n_vectors, dtype=np.uint32)
    queries = np.random.randn(n_queries, dim).astype(np.float32)

    # Build HNSW
    hnsw = arena_hnswlib.HNSW(
        dim=dim,
        max_elements=n_vectors * 2,
        m=16,
        ef_construction=200,
        metric=metric,
    )
    hnsw.add_items(data, ids)
    hnsw.ef_search = 100

    # Build BruteForce for ground truth
    bf = arena_hnswlib.BruteForce(dim=dim, max_elements=n_vectors, metric=metric)
    bf.add_items(data, ids)

    # Compare results
    hnsw_ids, _ = hnsw.query(queries, k)
    bf_ids, _ = bf.query(queries, k)

    recall_sum = 0.0
    for i in range(n_queries):
        hnsw_set = set(hnsw_ids[i].tolist())
        bf_set = set(bf_ids[i].tolist())
        recall_sum += len(hnsw_set & bf_set) / k

    recall = recall_sum / n_queries
    result_queue.put(("recall", recall))
    print(f"Recall@{k}: {recall:.4f}")


def main():
    """Run benchmark tests in isolated processes."""
    print("=" * 60)
    print("arena-hnswlib Benchmark (VectorDBBench Style)")
    print("=" * 60)

    # Test configuration
    dim = 128
    n_vectors = 10000
    n_queries = 100
    k = 10
    metric = "l2"

    print(f"\nConfiguration:")
    print(f"  Dimension: {dim}")
    print(f"  Num vectors: {n_vectors}")
    print(f"  Num queries: {n_queries}")
    print(f"  K: {k}")
    print(f"  Metric: {metric}")
    print("-" * 60)

    results = {}

    # Test 1: Insert performance
    print("\n[Test 1] Insert Performance")
    q = mp.Queue()
    p = mp.Process(
        target=run_insert_test,
        args=(dim, n_vectors, 16, 200, metric, q),
    )
    p.start()
    p.join(timeout=60)
    if p.is_alive():
        p.terminate()
        print("Insert test timed out")
    elif not q.empty():
        name, value = q.get()
        results[name] = value

    # Test 2: Search performance
    print("\n[Test 2] Search Performance")
    q = mp.Queue()
    p = mp.Process(
        target=run_search_test,
        args=(dim, n_vectors, n_queries, k, 100, metric, q),
    )
    p.start()
    p.join(timeout=60)
    if p.is_alive():
        p.terminate()
        print("Search test timed out")
    elif not q.empty():
        name, value = q.get()
        results[name] = value

    # Test 3: Recall
    print("\n[Test 3] Recall Calculation")
    q = mp.Queue()
    p = mp.Process(
        target=run_recall_test,
        args=(dim, min(n_vectors, 5000), n_queries, k, metric, q),
    )
    p.start()
    p.join(timeout=60)
    if p.is_alive():
        p.terminate()
        print("Recall test timed out")
    elif not q.empty():
        name, value = q.get()
        results[name] = value

    # Summary
    print("\n" + "=" * 60)
    print("Benchmark Summary")
    print("=" * 60)
    if "insert" in results:
        print(f"Insert throughput: {results['insert']:.2f} vectors/sec")
    if "search" in results:
        print(f"Search QPS: {results['search']:.2f} queries/sec")
    if "recall" in results:
        print(f"Recall@{k}: {results['recall']:.4f}")
    print("=" * 60)

    return 0 if results.get("recall", 0) >= 0.8 else 1


if __name__ == "__main__":
    sys.exit(main())
