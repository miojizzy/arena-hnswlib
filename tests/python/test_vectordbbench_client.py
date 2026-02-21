#!/usr/bin/env python3
# Copyright 2026 arena-hnswlib Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Simple benchmark test for arena-hnswlib using VectorDBBench-style interface.

This script tests the basic functionality and measures performance metrics
similar to VectorDBBench benchmarks.
"""

import time
import numpy as np
from typing import Optional


def run_simple_benchmark(
    dim: int = 128,
    num_vectors: int = 10000,
    num_queries: int = 100,
    k: int = 10,
    m: int = 16,
    ef_construction: int = 200,
    ef_search: int = 100,
    metric: str = "l2",
):
    """Run a simple benchmark test."""
    print("=" * 60)
    print("arena-hnswlib Simple Benchmark")
    print("=" * 60)
    print(f"Dimension: {dim}")
    print(f"Num vectors: {num_vectors}")
    print(f"Num queries: {num_queries}")
    print(f"K (neighbors): {k}")
    print(f"M: {m}")
    print(f"EF construction: {ef_construction}")
    print(f"EF search: {ef_search}")
    print(f"Metric: {metric}")
    print("-" * 60)

    # Import the client
    from arena_hnswlib.vdbbench_client import ArenaHnswlib, ArenaHnswlibConfig, ArenaHnswlibCaseConfig

    # Create configuration
    db_config = ArenaHnswlibConfig(metric=metric).to_dict()
    case_config = ArenaHnswlibCaseConfig(
        m=m,
        ef_construction=ef_construction,
        ef_search=ef_search,
    )

    # Create client
    db = ArenaHnswlib(
        dim=dim,
        db_config=db_config,
        db_case_config=case_config,
        drop_old=True,
    )

    # Generate random data
    print("\nGenerating random data...")
    np.random.seed(42)
    data = np.random.randn(num_vectors, dim).astype(np.float32)
    queries = np.random.randn(num_queries, dim).astype(np.float32)

    # Insert embeddings
    print("\nInserting vectors...")
    insert_times = []
    batch_size = 1000

    with db.init():
        for i in range(0, num_vectors, batch_size):
            batch_end = min(i + batch_size, num_vectors)
            batch_data = data[i:batch_end].tolist()
            batch_metadata = list(range(i, batch_end))

            start = time.perf_counter()
            count, err = db.insert_embeddings(batch_data, batch_metadata)
            elapsed = time.perf_counter() - start

            if err:
                print(f"Error inserting batch {i}: {err}")
                break

            insert_times.append(elapsed)

    total_insert_time = sum(insert_times)
    avg_insert_time = total_insert_time / num_vectors * 1000  # ms per vector

    print(f"Total insert time: {total_insert_time:.3f}s")
    print(f"Avg insert time per vector: {avg_insert_time:.4f}ms")
    print(f"Insert throughput: {num_vectors / total_insert_time:.2f} vectors/sec")

    # Optimize (no-op for HNSW)
    db.optimize()

    # Search embeddings
    print("\nSearching vectors...")
    search_times = []
    all_results = []

    with db.init():
        for query in queries:
            start = time.perf_counter()
            results = db.search_embedding(query.tolist(), k=k)
            elapsed = time.perf_counter() - start

            search_times.append(elapsed)
            all_results.append(results)

    total_search_time = sum(search_times)
    avg_search_time = total_search_time / num_queries * 1000  # ms per query

    print(f"Total search time: {total_search_time:.3f}s")
    print(f"Avg search time per query: {avg_search_time:.4f}ms")
    print(f"Search QPS: {num_queries / total_search_time:.2f} queries/sec")

    # Calculate recall (approximate, using random data)
    print("\nCalculating approximate recall...")
    recall_at_k = calculate_recall(data, queries, all_results, k)
    print(f"Recall@{k}: {recall_at_k:.4f}")

    # Summary
    print("\n" + "=" * 60)
    print("Summary")
    print("=" * 60)
    print(f"Insert throughput: {num_vectors / total_insert_time:.2f} vectors/sec")
    print(f"Search QPS: {num_queries / total_search_time:.2f} queries/sec")
    print(f"Avg latency: {avg_search_time:.4f}ms")
    print(f"Recall@{k}: {recall_at_k:.4f}")
    print("=" * 60)


def calculate_recall(
    data: np.ndarray,
    queries: np.ndarray,
    results: list[list[int]],
    k: int,
) -> float:
    """Calculate approximate recall using brute force."""
    from arena_hnswlib import BruteForce

    # Build brute force index
    bf = BruteForce(dim=data.shape[1], metric="l2")
    ids = np.arange(len(data), dtype=np.uint32)
    bf.add_items(data, ids)

    # Get ground truth
    gt_ids, _ = bf.query(queries, k)

    # Calculate recall
    total_recall = 0.0
    for i, result in enumerate(results):
        gt_set = set(gt_ids[i].tolist())
        result_set = set(result[:k])
        intersection = len(gt_set & result_set)
        total_recall += intersection / k

    return total_recall / len(queries)


def main():
    """Run multiple test configurations."""
    print("\nRunning benchmark tests...\n")

    # Small test
    run_simple_benchmark(
        dim=128,
        num_vectors=10000,
        num_queries=100,
        k=10,
        m=16,
        ef_construction=200,
        ef_search=100,
    )

    print("\n")

    # Larger test
    run_simple_benchmark(
        dim=768,
        num_vectors=50000,
        num_queries=200,
        k=100,
        m=16,
        ef_construction=200,
        ef_search=100,
    )


if __name__ == "__main__":
    main()
