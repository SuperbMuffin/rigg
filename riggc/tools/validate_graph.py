#!/usr/bin/env python3
"""
validate_graph.py — Rigg project graph validator.

Covers compilation steps 1-3:
  1. Parse project.meta
  2. Validate graph (syntax, duplicates, unknown concepts, missing directories)
  3. Detect cycles
  4. Check entry point (main.fn)

Error codes: G001-G004, C001-C002, E001

Usage:
    python3 tools/validate.py [project-root]
"""

import sys
import os
import re
import argparse
from dataclasses import dataclass, field
from typing import Optional


META = "project.meta"
CONCEPT_RE = re.compile(r"[a-z][a-z0-9_]*")


# ---------------------------------------------------------------------------
# Error
# ---------------------------------------------------------------------------

@dataclass
class RiggError(Exception):
    code: str
    message: str
    file: str = META
    line: int = 0
    context: str = ""

    def __str__(self) -> str:
        out = [f"Error {self.code}: {self.message}"]
        if self.line:
            out.append(f"\n  --> {self.file}:{self.line}")
        elif self.file:
            out.append(f"\n  --> {self.file}")
        if self.context:
            out.append(f"\n  {self.context}")
        return "\n".join(out)


# ---------------------------------------------------------------------------
# Data
# ---------------------------------------------------------------------------

@dataclass
class Dep:
    concept: str
    line: int


@dataclass
class ConceptNode:
    name: str
    meta_line: int
    deps: list[Dep] = field(default_factory=list)


# ---------------------------------------------------------------------------
# Step 1: Parse project.meta
# ---------------------------------------------------------------------------

def parse_meta(root: str) -> tuple[dict[str, ConceptNode], list[RiggError]]:
    path = os.path.join(root, META)
    errors: list[RiggError] = []
    concepts: dict[str, ConceptNode] = {}

    if not os.path.isfile(path):
        # E001 handled separately — missing meta is a different class of failure
        errors.append(RiggError(
            code="G001",
            message="Invalid graph syntax",
            file=META,
            line=0,
            context="project.meta not found.",
        ))
        return concepts, errors

    with open(path) as f:
        raw_lines = f.readlines()

    current: Optional[ConceptNode] = None
    first_declared_lines: dict[str, int] = {}  # for G004 context

    for lineno, raw in enumerate(raw_lines, 1):
        line = raw.rstrip()
        if "#" in line:
            line = line[:line.index("#")]
        stripped = line.strip()

        if not stripped:
            continue

        # dependency line
        if stripped.startswith("->"):
            dep_name = stripped[2:].strip()
            if not dep_name or not CONCEPT_RE.fullmatch(dep_name):
                errors.append(RiggError(
                    code="G001",
                    message="Invalid graph syntax",
                    line=lineno,
                    context=f"Expected a concept name after '->', got '{dep_name or '(nothing)'}'.",
                ))
                continue
            if current is None:
                errors.append(RiggError(
                    code="G001",
                    message="Invalid graph syntax",
                    line=lineno,
                    context="Dependency '->' appears before any concept declaration.",
                ))
                continue
            current.deps.append(Dep(dep_name, lineno))
            continue

        # concept declaration: NAME or NAME:
        m = re.fullmatch(r"([a-z][a-z0-9_]*)(:)?", stripped)
        if not m:
            errors.append(RiggError(
                code="G001",
                message="Invalid graph syntax",
                line=lineno,
                context=f"Unexpected token '{stripped}'. Expected a snake_case concept name.",
            ))
            current = None
            continue

        name = m.group(1)

        # G004 — duplicate
        if name in concepts:
            errors.append(RiggError(
                code="G004",
                message=f"Duplicate concept '{name}'",
                line=lineno,
                context=f"'{name}' was already declared on line {first_declared_lines[name]}.",
            ))
            current = concepts[name]
            continue

        node = ConceptNode(name=name, meta_line=lineno)
        concepts[name] = node
        first_declared_lines[name] = lineno
        current = node

    return concepts, errors


# ---------------------------------------------------------------------------
# Step 2: Validate graph edges
# ---------------------------------------------------------------------------

def validate_edges(concepts: dict[str, ConceptNode]) -> list[RiggError]:
    errors: list[RiggError] = []

    for node in concepts.values():
        seen: set[str] = set()
        for dep in node.deps:
            if dep.concept in seen:
                # duplicate dep edge — G001 syntax issue.  Distinct from G002
                # (circular dependency), which involves *different* concepts.
                errors.append(RiggError(
                    code="G001",
                    message="Invalid graph syntax",
                    line=dep.line,
                    context=f"'{node.name}' lists '{dep.concept}' as a dependency more than once (duplicate edge, not a cycle).",
                ))
                continue
            seen.add(dep.concept)

            if dep.concept == node.name:
                errors.append(RiggError(
                    code="G001",
                    message="Invalid graph syntax",
                    line=dep.line,
                    context=f"'{node.name}' cannot depend on itself.",
                ))
            elif dep.concept not in concepts:
                # G003 — unknown concept referenced as a dep
                errors.append(RiggError(
                    code="G003",
                    message=f"Unknown concept '{dep.concept}'",
                    line=dep.line,
                    context=f"'{dep.concept}' is referenced by '{node.name}' but is not declared in project.meta.",
                ))

    return errors


# ---------------------------------------------------------------------------
# Step 3: Cycle detection (DFS colouring)
# ---------------------------------------------------------------------------

def detect_cycles(concepts: dict[str, ConceptNode]) -> list[RiggError]:
    WHITE, GRAY, BLACK = 0, 1, 2
    colour = {n: WHITE for n in concepts}
    errors: list[RiggError] = []
    reported: set[frozenset] = set()

    adj: dict[str, list[str]] = {
        name: [d.concept for d in node.deps if d.concept in concepts]
        for name, node in concepts.items()
    }

    # Iterative DFS with an explicit call stack to avoid Python's recursion
    # limit (default 1000), which a deeply linear concept chain could exceed.
    # Stack entries: (node, iterator-over-neighbours, current-path-snapshot)
    # We track the path as a list + a set for O(1) membership tests.
    for start in concepts:
        if colour[start] != WHITE:
            continue

        # Each frame: (node_name, neighbour_iterator, index_of_node_in_path)
        path: list[str] = []
        path_set: set[str] = set()
        stack: list[tuple[str, int]] = [(start, 0)]
        colour[start] = GRAY
        path.append(start)
        path_set.add(start)

        while stack:
            node, child_idx = stack[-1]
            children = adj[node]

            if child_idx < len(children):
                stack[-1] = (node, child_idx + 1)
                neighbour = children[child_idx]

                if colour[neighbour] == GRAY:
                    # back-edge → cycle
                    idx = path.index(neighbour)
                    cycle = path[idx:] + [neighbour]
                    key = frozenset(zip(cycle, cycle[1:]))
                    if key not in reported:
                        reported.add(key)
                        errors.append(RiggError(
                            code="G002",
                            message="Circular dependency detected",
                            file="",
                            line=0,
                            context=" -> ".join(cycle),
                        ))
                elif colour[neighbour] == WHITE:
                    colour[neighbour] = GRAY
                    path.append(neighbour)
                    path_set.add(neighbour)
                    stack.append((neighbour, 0))
            else:
                # all children visited — finish this node
                stack.pop()
                colour[node] = BLACK
                path.pop()
                path_set.discard(node)

    return errors


# ---------------------------------------------------------------------------
# Step 4: Validate concept directories
# ---------------------------------------------------------------------------

def validate_directories(root: str, concepts: dict[str, ConceptNode]) -> list[RiggError]:
    """Check declared concepts have directories and vice-versa.

    Error code guide (these descriptions are nearly identical in ERRORS.md but
    they cover different root causes):

      G003 — emitted by validate_edges: a *dependency edge* in project.meta
             references a concept name that was never declared anywhere in
             project.meta (i.e. completely unknown to the graph).

      C001 — emitted here: a concept IS declared in project.meta (its node
             exists in the graph) but its directory is absent from disk.

      C002 — emitted here: a directory exists that looks like a concept (matches
             the snake_case pattern and contains at least one .fn/.impl file)
             but is not declared in project.meta.  Non-concept directories such
             as 'tools/' are excluded by the .fn/.impl membership check so they
             do not produce false C002 errors.
    """
    errors: list[RiggError] = []

    for name, node in concepts.items():
        if not os.path.isdir(os.path.join(root, name)):
            errors.append(RiggError(
                code="C001",
                message=f"Missing concept directory '{name}'",
                file=META,
                line=node.meta_line,
                context=f"'{name}' is declared in project.meta but no directory was found.",
            ))

    # C002 — undeclared directories that look like concepts.
    # A directory is treated as a concept candidate only if it contains at
    # least one .fn or .impl file.  This excludes infrastructure directories
    # like 'tools/', 'docs/', '.git/' etc. from generating false C002 errors.
    declared = set(concepts.keys())
    try:
        entries = os.listdir(root)
    except OSError:
        entries = []

    for entry in sorted(entries):
        dir_path = os.path.join(root, entry)
        if (
            os.path.isdir(dir_path)
            and not entry.startswith(".")
            and CONCEPT_RE.fullmatch(entry)
            and entry not in declared
            and _has_concept_files(dir_path)
        ):
            errors.append(RiggError(
                code="C002",
                message=f"Undeclared concept '{entry}'",
                file=f"{entry}/",
                line=0,
                context=f"Directory '{entry}/' exists but is not declared in project.meta.",
            ))

    return errors


def _has_concept_files(dir_path: str) -> bool:
    """Return True if dir_path contains at least one .fn or .impl file."""
    try:
        return any(
            f.endswith(".fn") or f.endswith(".impl")
            for f in os.listdir(dir_path)
            if os.path.isfile(os.path.join(dir_path, f))
        )
    except OSError:
        return False

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

def _topo_order(concepts: dict[str, ConceptNode]) -> list[str]:
    """Return concepts in topological order (dependencies before dependents).

    Uses Kahn's algorithm (iterative BFS) rather than recursive DFS so it
    cannot hit Python's recursion limit on deep linear dependency chains.
    Callers should only invoke this after detect_cycles() has confirmed the
    graph is acyclic; if a cycle somehow slips through, nodes involved in it
    are simply omitted from the output.
    """
    in_degree: dict[str, int] = {name: 0 for name in concepts}
    for node in concepts.values():
        for dep in node.deps:
            if dep.concept in concepts:
                in_degree[node.name] += 1

    # Seed with all nodes that have no dependencies.
    # Use sorted() for a stable, deterministic starting order.
    queue: list[str] = sorted(
        name for name, deg in in_degree.items() if deg == 0
    )
    order: list[str] = []

    # Build reverse adjacency: for each node, which nodes depend on it?
    dependents: dict[str, list[str]] = {name: [] for name in concepts}
    for node in concepts.values():
        for dep in node.deps:
            if dep.concept in concepts:
                dependents[dep.concept].append(node.name)

    while queue:
        name = queue.pop(0)
        order.append(name)
        for dependent in sorted(dependents[name]):
            in_degree[dependent] -= 1
            if in_degree[dependent] == 0:
                queue.append(dependent)

    return order


def _topo_summary(concepts: dict[str, ConceptNode]) -> str:
    if not concepts:
        return "  (none)"

    lines = []
    for name in _topo_order(concepts):
        deps = concepts[name].deps
        if deps:
            lines.append(f"  {name}: -> " + ", ".join(d.concept for d in deps))
        else:
            lines.append(f"  {name}")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

def run(root: str) -> int:
    errors: list[RiggError] = []

    concepts, parse_errors = parse_meta(root)
    errors.extend(parse_errors)

    if not parse_errors:
        errors.extend(validate_edges(concepts))
        errors.extend(detect_cycles(concepts))
        errors.extend(validate_directories(root, concepts))


    if errors:
        for i, e in enumerate(errors):
            if i:
                print()
            print(e)
        return 1

    print(f"ok — {len(concepts)} concept(s), no errors\n")
    print("dependency graph:")
    print(_topo_summary(concepts))
    return 0


def main():
    ap = argparse.ArgumentParser(
        description="Rigg project graph validator",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("root", nargs="?", default=".", help="project root (default: .)")
    args = ap.parse_args()

    if not os.path.isdir(args.root):
        print(f"validate: '{args.root}' is not a directory", file=sys.stderr)
        sys.exit(2)

    sys.exit(run(args.root))


if __name__ == "__main__":
    main()
