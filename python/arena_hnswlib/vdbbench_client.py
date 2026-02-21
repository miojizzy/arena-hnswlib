"""
VectorDBBench-compatible client for arena-hnswlib.

Provides ArenaHnswlib, ArenaHnswlibConfig, and ArenaHnswlibCaseConfig
following the VectorDBBench client interface conventions.
"""

from __future__ import annotations

import contextlib
import numpy as np
from dataclasses import dataclass, field
from typing import Optional

from . import HNSW


@dataclass
class ArenaHnswlibConfig:
    """Database-level configuration (metric selection)."""

    metric: str = "l2"

    def to_dict(self) -> dict:
        return {"metric": self.metric}


@dataclass
class ArenaHnswlibCaseConfig:
    """Case-level (per-run) configuration for index parameters."""

    m: int = 16
    ef_construction: int = 200
    ef_search: int = 100


class ArenaHnswlib:
    """VectorDBBench-compatible client wrapping arena-hnswlib HNSW index.

    Usage::

        db = ArenaHnswlib(dim=128, db_config={"metric": "l2"},
                          db_case_config=ArenaHnswlibCaseConfig())
        with db.init():
            db.insert_embeddings(batch, metadata)
        with db.init():
            ids = db.search_embedding(query, k=10)
    """

    def __init__(
        self,
        dim: int,
        db_config: dict,
        db_case_config: ArenaHnswlibCaseConfig,
        drop_old: bool = True,
    ) -> None:
        self.dim = dim
        self.metric: str = db_config.get("metric", "l2")
        self.case_config = db_case_config
        self._index: Optional[HNSW] = None
        self._max_elements: int = 0

        if drop_old:
            self._index = None

    # ------------------------------------------------------------------
    # Context manager – initialises the index on enter
    # ------------------------------------------------------------------
    @contextlib.contextmanager
    def init(self):
        """Context manager that lazily initialises the HNSW index."""
        if self._index is None:
            # Use a generous initial capacity; will be grown on demand
            self._max_elements = max(self._max_elements, 100_000)
            self._index = HNSW(
                dim=self.dim,
                max_elements=self._max_elements,
                m=self.case_config.m,
                ef_construction=self.case_config.ef_construction,
                metric=self.metric,
            )
            self._index.ef_search = self.case_config.ef_search
        try:
            yield self
        finally:
            pass  # index is kept alive across context blocks

    # ------------------------------------------------------------------
    # Insert
    # ------------------------------------------------------------------
    def insert_embeddings(
        self,
        embeddings: list[list[float]],
        metadata: list[int],
    ) -> tuple[int, Optional[str]]:
        """Insert a batch of embeddings with associated integer IDs.

        Returns:
            (count_inserted, error_message_or_None)
        """
        if self._index is None:
            return 0, "Index not initialised – call within `with db.init():`"

        try:
            data = np.array(embeddings, dtype=np.float32)
            ids  = np.array(metadata,   dtype=np.uint32)

            # Grow index if needed
            needed = self._max_elements
            current_need = len(ids)  # best-effort; real tracking would be better
            if current_need > needed:
                # Re-create index with larger capacity (VectorDBBench calls
                # insert_embeddings sequentially so this branch is rare)
                needed = current_need * 2
                self._max_elements = needed
                new_idx = HNSW(
                    dim=self.dim,
                    max_elements=needed,
                    m=self.case_config.m,
                    ef_construction=self.case_config.ef_construction,
                    metric=self.metric,
                )
                new_idx.ef_search = self.case_config.ef_search
                self._index = new_idx

            self._index.add_items(data, ids)
            return len(ids), None
        except Exception as exc:  # noqa: BLE001
            return 0, str(exc)

    # ------------------------------------------------------------------
    # Search
    # ------------------------------------------------------------------
    def search_embedding(
        self,
        query: list[float],
        k: int = 10,
    ) -> list[int]:
        """Search for k nearest neighbours.

        Args:
            query: 1-D list of floats with length == dim.
            k:     number of neighbours to return.

        Returns:
            List of integer IDs (length <= k).
        """
        if self._index is None:
            return []

        q = np.array(query, dtype=np.float32).reshape(1, self.dim)
        ids, _dists = self._index.query(q, k)
        result = ids[0].tolist()
        # Filter sentinel values (-1 stored as large uint32)
        return [x for x in result if x != 0xFFFFFFFF]

    # ------------------------------------------------------------------
    # Optimize (no-op for HNSW – graph is built during insert)
    # ------------------------------------------------------------------
    def optimize(self) -> None:
        """Post-build optimisation hook (no-op for HNSW)."""
        pass
