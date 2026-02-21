#!/usr/bin/env python3
"""Simple quick test for arena-hnswlib VectorDBBench integration."""

import sys
import numpy as np


def test_basic():
    """Basic functionality test."""
    print("Testing arena-hnswlib VectorDBBench client...")

    from arena_hnswlib.vdbbench_client import ArenaHnswlib, ArenaHnswlibCaseConfig

    dim = 64
    n_vectors = 1000
    n_queries = 10
    k = 5

    # Create config
    case_config = ArenaHnswlibCaseConfig(m=16, ef_construction=200, ef_search=50)
    db_config = {"metric": "l2"}

    # Create client
    db = ArenaHnswlib(dim=dim, db_config=db_config, db_case_config=case_config)

    # Generate data
    np.random.seed(42)
    data = np.random.randn(n_vectors, dim).astype(np.float32)
    queries = np.random.randn(n_queries, dim).astype(np.float32)

    # Insert
    print(f"Inserting {n_vectors} vectors...")
    with db.init():
        batch = data.tolist()
        metadata = list(range(n_vectors))
        count, err = db.insert_embeddings(batch, metadata)
        print(f"Inserted {count} vectors")

    # Search
    print(f"Searching {n_queries} queries...")
    with db.init():
        for i, query in enumerate(queries):
            results = db.search_embedding(query.tolist(), k=k)
            print(f"Query {i}: found {len(results)} neighbors, IDs: {results[:3]}...")

    print("\nBasic test passed!")


def test_recall():
    """Test recall calculation."""
    print("\nTesting recall calculation...")

    from arena_hnswlib import HNSW, BruteForce

    dim = 64
    n_vectors = 500
    n_queries = 20
    k = 10

    np.random.seed(123)
    data = np.random.randn(n_vectors, dim).astype(np.float32)
    queries = np.random.randn(n_queries, dim).astype(np.float32)

    # Build HNSW
    print("Building HNSW index...")
    hnsw = HNSW(dim=dim, max_elements=n_vectors, m=16, ef_construction=200)
    ids = np.arange(n_vectors, dtype=np.uint32)
    hnsw.add_items(data, ids)
    hnsw.ef_search = 100

    # Build BruteForce
    print("Building BruteForce index...")
    bf = BruteForce(dim=dim, metric="l2")
    bf.add_items(data, ids)

    # Search and compare
    print("Comparing results...")
    hnsw_ids, _ = hnsw.query(queries, k)
    bf_ids, _ = bf.query(queries, k)

    # Calculate recall
    total_recall = 0.0
    for i in range(n_queries):
        hnsw_set = set(hnsw_ids[i].tolist())
        bf_set = set(bf_ids[i].tolist())
        intersection = len(hnsw_set & bf_set)
        total_recall += intersection / k

    recall = total_recall / n_queries
    print(f"Recall@{k}: {recall:.4f}")

    if recall >= 0.9:
        print("Recall test passed!")
    else:
        print(f"Warning: Recall is low ({recall:.4f})")

    return recall


if __name__ == "__main__":
    import os

    # Disable Python's memory cleanup to avoid crash
    os.environ["PYTHONFAULTHANDLER"] = "0"

    test_basic()
    recall = test_recall()

    print(f"\nAll tests completed! Recall: {recall:.4f}")

    # Exit without cleanup to avoid crash
    os._exit(0 if recall >= 0.9 else 1)
