"""Static-analysis rules for scripts/code-verify.py.

Adds CLAUDE.md-derived semantic checks on top of the formatter's existing
style rules. C++ rules use tree-sitter's C++ grammar to walk a real AST;
QML rules stay line-based on top of the tokenizer that already lives in
code-verify.py.

Each rule returns a list of (line, kind, message) tuples. The driver in
code-verify.py wraps them as Violations and routes them through the
existing flag-only / auto-fixable pipeline. Every rule here is flag-only.

Tree-sitter is the only new dependency. The module degrades gracefully:
when tree-sitter or tree-sitter-cpp aren't importable, C++ semantic
checks are silently skipped and the formatter still runs its line-based
rules. The CI install pins both in tests/requirements.txt.

`code-verify off / on` fences mask every rule here too, same as the
existing rules — the driver passes the fence mask in.
"""
from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path

try:
    import tree_sitter
    import tree_sitter_cpp
    _CPP_LANG = tree_sitter.Language(tree_sitter_cpp.language())
    _CPP_PARSER = tree_sitter.Parser(_CPP_LANG)
    HAS_TREE_SITTER = True
except Exception:
    HAS_TREE_SITTER = False
    _CPP_LANG = None
    _CPP_PARSER = None


# ---------------------------------------------------------------------------
# Public types
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class Finding:
    line: int
    kind: str
    message: str


# ---------------------------------------------------------------------------
# Hotpath method names (CLAUDE.md: never allocate on the dashboard path)
# ---------------------------------------------------------------------------

# Methods named here are walked for new/make_shared/append calls. The names
# come straight from CLAUDE.md's "Threading Rules" / "Hotpath" sections.
_HOTPATH_METHODS = frozenset({
    "hotpathRxFrame",
    "hotpathRxSourceFrame",
    "processData",
    "onReadyRead",
    "onFrameReady",
    "onRawDataReceived",
    "appendChunk",
    "frameTimestamp",
    "applyTransform",
    "parseProjectFrame",
    "updateData",
    "updateLineSeries",
    "pushSample",
})

# Calls / patterns banned on the hotpath. Each entry is (regex, message).
_HOTPATH_BANNED_CALLS = [
    (re.compile(r"\bnew\s+[A-Za-z_]"), "`new` allocation on hotpath"),
    (re.compile(r"\bstd::make_shared\b"), "`std::make_shared` allocation on hotpath"),
    (re.compile(r"\bstd::make_unique\b"), "`std::make_unique` allocation on hotpath"),
    (re.compile(r"\.append\("), "`.append(` (likely Qt container resize) on hotpath"),
    (re.compile(r"\.push_back\("), "`.push_back(` on hotpath (pre-reserve at init)"),
    (re.compile(r"\bemit\b"), "bare `emit` on hotpath -- use `Q_EMIT`"),
]


# ---------------------------------------------------------------------------
# Hardcoded JSON keys (CLAUDE.md: use Keys::* namespace)
# ---------------------------------------------------------------------------

# Mirrors the inline constexpr declarations at the top of Frame.h. When one
# of these strings appears as a literal in a .cpp/.h that ALSO uses ss_jsr
# or QJsonObject, it's almost certainly a writer/reader bypassing Keys::*.
_PROJECT_KEY_LITERALS = frozenset({
    "busType", "frameStart", "frameEnd", "frameDetection", "frameParserCode",
    "frameParserLanguage", "decoderMethod", "checksumAlgorithm",
    "hexadecimalDelimiters", "schemaVersion", "writerVersion",
    "writerVersionAtCreation", "sourceId",
    "datasetId", "uniqueId",
})


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _line_of(node) -> int:
    """1-based start line of a tree-sitter node."""
    return node.start_point[0] + 1


def _node_text(node, src: bytes) -> str:
    return src[node.start_byte:node.end_byte].decode("utf-8", errors="replace")


def _walk(node):
    """Yield node and all descendants in depth-first order."""
    yield node
    for child in node.children:
        yield from _walk(child)


def _find_field_decl_default(class_node, src: bytes):
    """Yield (line, name) for every `field_declaration` in `class_node` that
    has a `default_value` (the `int m_foo = 0;` pattern that CLAUDE.md
    forbids in headers)."""
    body = class_node.child_by_field_name("body")
    if body is None:
        return
    for child in body.children:
        if child.type != "field_declaration":
            continue
        # field_declaration nodes carry their default_value as a named field.
        default = child.child_by_field_name("default_value")
        if default is None:
            continue
        decl = child.child_by_field_name("declarator")
        if decl is None:
            continue
        # Skip static member initializers (those are valid C++17 inline init,
        # not the in-header-default the rule cares about).
        # tree-sitter exposes `static` via storage_class_specifier siblings.
        is_static = any(
            c.type == "storage_class_specifier" and _node_text(c, src) == "static"
            for c in child.children
        )
        if is_static:
            continue
        # Skip constexpr — those have to be initialized in the header.
        is_constexpr = any(
            c.type == "type_qualifier" and _node_text(c, src) == "constexpr"
            for c in child.children
        )
        if is_constexpr:
            continue
        name = _node_text(decl, src).strip()
        yield (_line_of(child), name)


def _is_void_return(func_node, src: bytes) -> bool:
    """True when a function_definition / field_declaration returns void."""
    type_node = func_node.child_by_field_name("type")
    if type_node is None:
        return True  # constructors / destructors -- treat as "no return"
    return _node_text(type_node, src).strip() == "void"


def _has_attribute(node, src: bytes, attr_name: str) -> bool:
    """True when the declaration carries `[[<attr_name>]]` either before
    the type or as a sibling attribute_specifier."""
    # Attributes are parsed as attribute_declaration siblings before the
    # declaration's type, OR as attribute children inside the declarator.
    # Walk a small radius around the node to find them.
    text_before = src[max(0, node.start_byte - 200):node.start_byte].decode(
        "utf-8", errors="replace"
    )
    if f"[[{attr_name}]]" in text_before or f"[[ {attr_name} ]]" in text_before:
        return True
    for child in node.children:
        if "attribute" in child.type and attr_name in _node_text(child, src):
            return True
    return False


def _function_body_lines(body_node) -> int:
    """Number of source lines a `compound_statement` spans (end - start + 1).
    Returns 0 when the node is missing."""
    if body_node is None:
        return 0
    return body_node.end_point[0] - body_node.start_point[0] + 1


def _max_nesting_depth(body_node) -> int:
    """Maximum control-flow nesting depth inside a compound_statement.
    Counts if/else/for/while/do/switch/try blocks. Lambdas reset the
    counter -- their body is a separate scope."""
    if body_node is None:
        return 0

    NESTERS = {
        "if_statement", "for_statement", "for_range_loop",
        "while_statement", "do_statement", "switch_statement",
        "try_statement",
    }
    LAMBDA = "lambda_expression"

    def walk(n, depth):
        best = depth
        for c in n.children:
            if c.type == LAMBDA:
                continue  # lambda body is its own scope
            if c.type in NESTERS:
                best = max(best, walk(c, depth + 1))
            else:
                best = max(best, walk(c, depth))
        return best

    return walk(body_node, 0)


def _previous_doxygen_brief(node, src: bytes) -> bool:
    """True when the lines immediately before `node` form a /** @brief ... */
    block. The block may sit anywhere from 0 to ~3 blank/decorator lines
    above the declaration."""
    start_line = node.start_point[0]
    if start_line == 0:
        return False

    # Walk backward from the line just above the declaration. We want to
    # find a closing `*/` first, then verify that the matching `/**` is
    # within reach AND mentions @brief. This handles long doxygen blocks
    # (DSP.h's FixedQueue carries a 16-line @brief) without hand-tuning
    # a fixed lookback window.
    src_text = src.decode("utf-8", errors="replace")
    lines = src_text.split("\n")
    cur = start_line - 1
    # Tolerate up to 8 lines of pre-declaration noise between the doxygen
    # close and the declaration: blanks, banner comments, `template<...>`
    # clauses, `requires ...` constraints, attributes (`[[nodiscard]]`),
    # access specifiers carried over from a class body.
    skip = 0
    while cur >= 0 and skip < 8:
        s = lines[cur].strip()
        if (not s or s.startswith("//") or s.startswith("template")
                or s.startswith("requires") or s.startswith("[[")
                or s.startswith("public:") or s.startswith("private:")
                or s.startswith("protected:")):
            cur -= 1
            skip += 1
            continue
        break
    if cur < 0:
        return False
    if not lines[cur].rstrip().endswith("*/"):
        return False
    # Walk further back until we find `/**` -- look at most 60 lines.
    open_idx = -1
    for j in range(cur, max(0, cur - 60) - 1, -1):
        if "/**" in lines[j]:
            open_idx = j
            break
    if open_idx < 0:
        return False
    block = "\n".join(lines[open_idx:cur + 1])
    return "@brief" in block


def _previous_doxygen_block_range(node, src: bytes):
    """Return `(open_idx, close_idx)` 0-based line indices for the doxygen
    block immediately above `node`, or `None` when no block is present. The
    walk mirrors `_previous_doxygen_brief` but exposes the block range so a
    caller can inspect its body (verbose-brief detection)."""
    start_line = node.start_point[0]
    if start_line == 0:
        return None
    src_text = src.decode("utf-8", errors="replace")
    lines = src_text.split("\n")
    cur = start_line - 1
    skip = 0
    while cur >= 0 and skip < 8:
        s = lines[cur].strip()
        if (not s or s.startswith("//") or s.startswith("template")
                or s.startswith("requires") or s.startswith("[[")
                or s.startswith("public:") or s.startswith("private:")
                or s.startswith("protected:")):
            cur -= 1
            skip += 1
            continue
        break
    if cur < 0:
        return None
    if not lines[cur].rstrip().endswith("*/"):
        return None
    open_idx = -1
    for j in range(cur, max(0, cur - 60) - 1, -1):
        if "/**" in lines[j]:
            open_idx = j
            break
    if open_idx < 0:
        return None
    return (open_idx, cur)


_VERBOSE_DOXY_TAGS = (
    "@param", "@return", "@returns", "@retval", "@throws", "@throw",
    "@exception", "@see", "@sa", "@note", "@warning", "@todo", "@since",
    "@deprecated", "@pre", "@post", "@invariant", "@tparam", "@details",
)


def _verbose_doxygen_reason(block_lines: list[str]) -> str:
    """Inspect a `/** ... */` doxygen block and return a short reason string
    when it is "verbose" per CLAUDE.md (one-line `@brief` is the contract).
    Returns `""` when the block is acceptable (one-line `/** @brief ... */`
    or a clean multi-line that's just a wrapped `@brief` sentence).

    Verbose forms (any one trips the rule):
    - Carries a doxygen tag other than `@brief` (`@param`, `@return`,
      `@note`, `@see`, `@throws`, `@tparam`, `@retval`, ...).
    - Contains a blank doxygen continuation line (` *` with no body), which
      separates an extended description paragraph from the brief.
    - Spans more than 4 source lines (the natural ceiling for a wrapped
      one-sentence brief is 3-4 lines: `/**`, ` * @brief ...`, ` * ...wrap`,
      ` */`).
    """
    if not block_lines:
        return ""
    body = "\n".join(block_lines)
    if "@brief" not in body:
        return ""
    for tag in _VERBOSE_DOXY_TAGS:
        if tag in body:
            return f"contains `{tag}` -- one-line `@brief` is the contract"
    for raw in block_lines:
        if raw.strip() == "*":
            return ("contains a blank `*` continuation -- extended "
                    "description belongs in the commit message, not the "
                    "header doxygen")
    if len(block_lines) > 4:
        return (f"spans {len(block_lines)} lines -- collapse to a one-line "
                "`/** @brief ... */`")
    return ""


# ---------------------------------------------------------------------------
# C++ rules (tree-sitter)
# ---------------------------------------------------------------------------

def _cpp_rules(src: bytes, path: Path, fence_mask: list[bool]) -> list[Finding]:
    """Run tree-sitter-backed C++/Qt rules on `src`. Returns one Finding per
    issue. `fence_mask[i]` is True when source line `i+1` sits inside a
    `// code-verify off` / `// code-verify on` fence and must be skipped.

    The rules implemented here mirror CLAUDE.md's "Common Mistakes" table
    and the NASA Power-of-Ten section:

        qt-bare-emit             bare `emit` outside strings/comments
        qt-uppercase-signal-slot Q_SIGNALS: / Q_SLOTS: section labels
        qt-invokable-void        Q_INVOKABLE void f() (use `public slots:`)
        qt-header-member-init    int m_foo = 0; in a header class body
        qt-disconnect-nullptr    disconnect(..., nullptr) as the slot arg
        qt-direct-jsengine-call  parseFunction.call(...) without guardedCall
        cxx-function-too-long    function definition over 100 lines
        cxx-nesting-too-deep     control-flow nesting > 3 levels
        cxx-goto-or-jmp          goto / setjmp / longjmp
        doc-missing-brief-cpp    .cpp function definition without /** @brief */
        doc-missing-brief-h      header type-level definition without /** @brief */
        doc-verbose-brief        doxygen block carries @param/@return/blank-`*`
                                 paragraphs or wraps to 5+ lines (one-line
                                 `/** @brief ... */` is the contract)
        hotpath-allocation       allocation/append on a known hotpath method
        keys-hardcoded-literal   raw "busType" etc. literal where Keys:: belongs
        cxx-anonymous-namespace  helpers/types defined inside `namespace { ... }`
    """
    if not HAS_TREE_SITTER:
        return []

    out: list[Finding] = []
    tree = _CPP_PARSER.parse(src)
    root = tree.root_node
    is_header = path.suffix in {".h", ".hpp", ".hxx"}

    def fenced(line: int) -> bool:
        idx = line - 1
        return 0 <= idx < len(fence_mask) and fence_mask[idx]

    # ---- qt-bare-emit / qt-uppercase-signal-slot / cxx-goto / Q_INVOKABLE void
    # The fastest pass that catches text-level Qt mistakes is a token sweep
    # that respects strings/comments. We use a tiny per-line scan that
    # strips comments and string literals first.
    src_text = src.decode("utf-8", errors="replace")
    lines = src_text.split("\n")
    for i, raw in enumerate(lines, start=1):
        if fenced(i):
            continue
        scrubbed = _strip_strings_and_line_comments(raw)
        # Bare `emit` -- token must be word-isolated.
        if re.search(r"\bemit\b\s+[A-Za-z_]", scrubbed):
            out.append(Finding(i, "qt-bare-emit",
                               "use `Q_EMIT` instead of bare `emit`"))
        if re.search(r"\bQ_SIGNALS\s*:", scrubbed):
            out.append(Finding(i, "qt-uppercase-signal-slot",
                               "use lowercase `signals:` (CLAUDE.md)"))
        if re.search(r"\b(?:public|protected|private)?\s*Q_SLOTS\s*:", scrubbed):
            out.append(Finding(i, "qt-uppercase-signal-slot",
                               "use lowercase `slots:` (CLAUDE.md)"))
        if re.search(r"\bgoto\b", scrubbed):
            out.append(Finding(i, "cxx-goto-or-jmp",
                               "`goto` is banned (NASA Power of Ten rule 1)"))
        if re.search(r"\b(?:setjmp|longjmp)\s*\(", scrubbed):
            out.append(Finding(i, "cxx-goto-or-jmp",
                               "`setjmp`/`longjmp` are banned (NASA P10 rule 1)"))

    # Q_INVOKABLE void on the same source line is unambiguous; spread cases
    # are rare enough that we accept the false-negative.
    invokable_void_re = re.compile(r"\bQ_INVOKABLE\b\s+void\b")
    for i, raw in enumerate(lines, start=1):
        if fenced(i):
            continue
        scrubbed = _strip_strings_and_line_comments(raw)
        if invokable_void_re.search(scrubbed):
            out.append(Finding(i, "qt-invokable-void",
                               "`Q_INVOKABLE void` -- move to `public slots:`"))

    # disconnect(<conn>, nullptr) -- the 2-arg form where nullptr is in
    # the slot slot. CLAUDE.md flags this because it disconnects every
    # slot the connection was paired with. The 4-arg wildcard form
    # `disconnect(sender, nullptr, receiver, nullptr)` is idiomatic Qt
    # ("disconnect every signal-slot pair between sender and receiver")
    # and explicitly NOT what the rule cares about.
    for i, raw in enumerate(lines, start=1):
        if fenced(i):
            continue
        scrubbed = _strip_strings_and_line_comments(raw)
        # Match: disconnect ( <one-arg> , nullptr )  -- exactly two args.
        # The first arg can be a chain like `obj->signal`, but must not
        # contain a comma (we only want the 2-arg form).
        if re.search(r"\bdisconnect\s*\(\s*[^,()]+\s*,\s*nullptr\s*\)",
                     scrubbed):
            out.append(Finding(i, "qt-disconnect-nullptr",
                               "`disconnect(<conn>, nullptr)` disconnects ALL slots from the "
                               "connection; capture the QMetaObject::Connection and disconnect that"))

    # parseFunction.call(...) is the JS engine path that must go through
    # IScriptEngine::guardedCall for the runtime watchdog.
    for i, raw in enumerate(lines, start=1):
        if fenced(i):
            continue
        scrubbed = _strip_strings_and_line_comments(raw)
        if re.search(r"\bparseFunction\.call\s*\(", scrubbed):
            out.append(Finding(i, "qt-direct-jsengine-call",
                               "`parseFunction.call()` bypasses the runtime watchdog -- "
                               "route through `IScriptEngine::guardedCall()`"))

    # ---- AST-driven rules
    # Walk every function_definition: function-too-long, nesting-too-deep,
    # missing @brief in .cpp, hotpath allocations.
    for n in _walk(root):
        if n.type == "function_definition":
            line = _line_of(n)
            if fenced(line):
                continue
            body = n.child_by_field_name("body")
            length = _function_body_lines(body)
            if length > 100:
                out.append(Finding(line, "cxx-function-too-long",
                                   f"function spans {length} lines (>100; "
                                   f"NASA P10 rule 4 caps at 100)"))
            depth = _max_nesting_depth(body)
            if depth > 3:
                out.append(Finding(line, "cxx-nesting-too-deep",
                                   f"control-flow nesting depth {depth} (>3; "
                                   f"CLAUDE.md caps at 3)"))

            if not is_header and not _previous_doxygen_brief(n, src):
                fname = _function_name(n, src)
                # Skip lambdas (no name), tree-sitter macro mis-parses
                # (single-letter / lowercase keywords), and inline-defined
                # methods nested inside another function definition.
                if not fname:
                    continue
                if _has_function_ancestor(n):
                    continue
                # Anonymous-namespace static helpers are still public-facing
                # in this codebase; the rule applies. But generated/extern-C
                # main entry points aren't worth flagging.
                if fname == "main" and not _has_function_ancestor(n):
                    continue
                out.append(Finding(
                    line, "doc-missing-brief-cpp",
                    f"`{fname}` lacks a preceding `/** @brief ... */`"))

            # Verbose doxygen above a function definition. Applies to both
            # `.cpp` definitions and inline-defined methods in headers; the
            # contract per CLAUDE.md is a one-line `/** @brief ... */`.
            if not _has_function_ancestor(n):
                fname = _function_name(n, src)
                if fname:
                    rng = _previous_doxygen_block_range(n, src)
                    if rng is not None:
                        open_idx, close_idx = rng
                        block_lines = src.decode("utf-8", errors="replace") \
                            .split("\n")[open_idx:close_idx + 1]
                        reason = _verbose_doxygen_reason(block_lines)
                        if reason and not fenced(open_idx + 1):
                            out.append(Finding(
                                open_idx + 1, "doc-verbose-brief",
                                f"verbose doxygen above `{fname}`: {reason}"))

            # Hotpath allocations.
            fname = _function_name(n, src)
            if fname in _HOTPATH_METHODS and body is not None:
                body_text = _node_text(body, src)
                body_start = body.start_point[0] + 1
                for j, body_line in enumerate(body_text.split("\n")):
                    abs_line = body_start + j
                    if fenced(abs_line):
                        continue
                    scrubbed = _strip_strings_and_line_comments(body_line)
                    for pat, msg in _HOTPATH_BANNED_CALLS:
                        if pat.search(scrubbed):
                            out.append(Finding(
                                abs_line, "hotpath-allocation",
                                f"{msg} (in `{fname}`)"))
                            break

    # ---- Anonymous-namespace helpers (.cpp only). Anonymous namespaces hide
    # symbols from the linker but also from grep, doxygen, IDE call hierarchies,
    # and any reader trying to trace where a helper lives. CLAUDE.md prefers
    # named-namespace statics or class-private statics; flag every helper /
    # variable / type defined inside `namespace { ... }`. Headers don't get
    # this rule because anon namespaces in headers are a separate (worse) bug
    # caught by other tools.
    if not is_header:
        for n in _walk(root):
            if n.type != "namespace_definition":
                continue
            if not _is_anonymous_namespace(n):
                continue
            body = n.child_by_field_name("body")
            if body is None:
                continue
            for child in body.children:
                if child.type not in (
                    "function_definition", "declaration", "template_declaration",
                    "class_specifier", "struct_specifier", "enum_specifier",
                    "type_definition", "alias_declaration", "union_specifier",
                ):
                    continue
                line = _line_of(child)
                if fenced(line):
                    continue
                label = _anon_member_label(child, src)
                out.append(Finding(
                    line, "cxx-anonymous-namespace",
                    f"`{label}` defined inside an anonymous namespace -- "
                    f"hard to trace; prefer a class-private `static` "
                    f"(default), file-scope `static` for free helpers, or "
                    f"a named `detail` namespace for TU-private types"))

    # ---- Header-only: function doxygen blocks above non-type declarations.
    # CLAUDE.md "Headers (.h) -- strict rule": the only block-doc allowed in
    # a header is `/** @brief ... */` above a TYPE-LEVEL definition. Function
    # doxygen blocks above member-function decls are explicitly forbidden
    # ("No function doxygen, member-variable comments, signal/slot comments,
    # @param/@return/@note/@see, or inline `//`. Names + types are the
    # documentation."). The existing tools track type-level coverage; this
    # check finds the opposite: blocks that should be deleted.
    if is_header:
        src_text2 = src.decode("utf-8", errors="replace")
        lines2 = src_text2.split("\n")
        for n in _walk(root):
            if n.type != "field_declaration":
                continue
            line = _line_of(n)
            if fenced(line):
                continue
            decl = n.child_by_field_name("declarator")
            if decl is None:
                continue
            # Only complain when the declarator is a function declarator
            # (we don't want to flag plain member-variable doxygen here --
            # `qt-header-member-init` covers that for QObject classes, and
            # POD config-bag fields are intentionally exempt).
            if decl.type != "function_declarator":
                continue
            # Skip inline-defined methods -- those are function bodies, and
            # the doc-missing-brief-cpp rule handles their doxygen.
            #
            # An inline-defined method's `field_declaration` carries its body
            # as a `compound_statement` child of the function_declarator's
            # parent. Detect by walking the field_declaration for any
            # compound_statement descendant that's a body, not an init list.
            has_body = False
            for c in n.children:
                if c.type == "function_definition":
                    has_body = True
                    break
            if has_body:
                continue
            # Walk back from `line - 1` to find a closing `*/`. If found, find
            # its matching `/**` and check whether the block is multi-line.
            # A one-line `/** @brief ... */` block above a member function
            # is also banned but we report it the same way -- the message
            # tells the reader to delete the block.
            cur = line - 2
            skip = 0
            while cur >= 0 and skip < 6:
                s = lines2[cur].strip()
                if (not s or s.startswith("//")
                        or s.startswith("[[") or s.startswith("template")
                        or s.startswith("requires")):
                    cur -= 1
                    skip += 1
                    continue
                break
            if cur < 0:
                continue
            if not lines2[cur].rstrip().endswith("*/"):
                continue
            # Walk further back to find the opening `/**`.
            open_idx = -1
            for j in range(cur, max(0, cur - 60) - 1, -1):
                if "/**" in lines2[j]:
                    open_idx = j
                    break
            if open_idx < 0:
                continue
            block = "\n".join(lines2[open_idx:cur + 1])
            # If the block is just `/** @brief ... */` on one line above a
            # type-level def, the doc-missing-brief-h rule is happy. But
            # if it's above a function declaration, CLAUDE.md still says it
            # shouldn't be there. Report it with a kind that maps to advisory
            # (heuristic only, broad existing-code debt).
            out.append(Finding(
                open_idx + 1, "doc-header-function-block",
                "header function declaration carries a doxygen block -- "
                "headers should hold ONLY @brief banners above type-level "
                "definitions; delete the block (CLAUDE.md \"Headers (.h) -- "
                "strict rule\")"))

    # ---- Header-only rules: in-header member init, @brief on type-level defs
    if is_header:
        # In-header member init -- only QObject classes (`Q_OBJECT` macro
        # in the body) per CLAUDE.md. Plain POD structs / config bags are
        # idiomatic with `int x = 0;` and are NOT what the rule targets.
        for n in _walk(root):
            if n.type != "class_specifier":
                continue
            cls_line = _line_of(n)
            if fenced(cls_line):
                continue
            body = n.child_by_field_name("body")
            if body is None:
                continue
            body_text = _node_text(body, src)
            if "Q_OBJECT" not in body_text and "Q_GADGET" not in body_text:
                continue
            for line, name in _find_field_decl_default(n, src):
                if fenced(line):
                    continue
                # Only complain about `m_<name>` style members (CLAUDE.md
                # convention: private members are `m_camelCase`). Public
                # config fields without the prefix aren't covered.
                last_ident = name.split()[-1].lstrip("&*")
                if not last_ident.startswith("m_"):
                    continue
                out.append(Finding(
                    line, "qt-header-member-init",
                    f"in-header default init `{last_ident}` -- "
                    f"initialize in the constructor member init list"))

        # Type-level @brief: every class/struct/enum/typedef/using-alias at
        # namespace scope needs a /** @brief */ banner, per CLAUDE.md.
        for n in _walk(root):
            if n.type not in {
                "class_specifier", "struct_specifier",
                "enum_specifier", "type_definition", "alias_declaration",
            }:
                continue
            # Skip nested types (parent is field_declaration_list / class
            # body) -- the enclosing class's @brief covers them.
            if _is_nested_type(n):
                continue
            # Skip forward declarations (no body / definition).
            if n.type in ("class_specifier", "struct_specifier",
                          "enum_specifier"):
                body_field = n.child_by_field_name("body")
                if body_field is None:
                    continue
            # Skip type aliases inside function bodies / templates (not
            # namespace-scope, no @brief required).
            if _has_function_ancestor(n):
                continue
            line = _line_of(n)
            if fenced(line):
                continue
            if not _previous_doxygen_brief(n, src):
                name = _type_name(n, src)
                if name:
                    out.append(Finding(
                        line, "doc-missing-brief-h",
                        f"`{name}` lacks a preceding `/** @brief ... */`"))
            else:
                # Verbose @brief on a type-level definition. Same rule:
                # one-line `/** @brief ... */`, no `@param`/`@note`/blank-`*`
                # paragraph splits / 5+ line prose.
                name = _type_name(n, src)
                if name:
                    rng = _previous_doxygen_block_range(n, src)
                    if rng is not None:
                        open_idx, close_idx = rng
                        block_lines = src.decode("utf-8", errors="replace") \
                            .split("\n")[open_idx:close_idx + 1]
                        reason = _verbose_doxygen_reason(block_lines)
                        if reason and not fenced(open_idx + 1):
                            out.append(Finding(
                                open_idx + 1, "doc-verbose-brief",
                                f"verbose doxygen above `{name}`: {reason}"))

    # ---- nodiscard on const getters in headers. CLAUDE.md says
    # "[[nodiscard]] on every non-void return". We narrow to the case
    # most likely to be a real getter (and least likely to be noise):
    # const member functions returning non-void inside a class body.
    # Setters, signals, Q_INVOKABLE methods, operators and constructors
    # are explicitly skipped.
    if is_header:
        for n in _walk(root):
            if n.type != "field_declaration":
                continue
            line = _line_of(n)
            if fenced(line):
                continue
            decl = n.child_by_field_name("declarator")
            if decl is None or decl.type != "function_declarator":
                continue
            type_node = n.child_by_field_name("type")
            if type_node is None:
                continue
            if _node_text(type_node, src).strip() == "void":
                continue
            if _in_signals_section(n, src):
                continue
            if _has_attribute(n, src, "nodiscard"):
                continue
            # Const member functions only. tree-sitter exposes the trailing
            # const as a `type_qualifier` inside the function_declarator.
            is_const = any(
                c.type == "type_qualifier" and _node_text(c, src) == "const"
                for c in decl.children
            )
            if not is_const:
                continue
            # Skip Q_INVOKABLE -- those are runtime-callable, not getters
            # with a strong nodiscard contract.
            line_text = src.decode("utf-8", errors="replace").split("\n")[line - 1]
            if "Q_INVOKABLE" in line_text or "Q_PROPERTY" in line_text:
                continue
            name = _node_text(decl, src).split("(", 1)[0].strip()
            if not name or name.startswith("operator"):
                continue
            # Skip setters that happen to be const (extremely rare, but
            # leaves a clean exit condition).
            if name.startswith("set") and len(name) > 3 and name[3].isupper():
                continue
            out.append(Finding(
                line, "qt-missing-nodiscard",
                f"non-void const getter `{name}` lacks `[[nodiscard]]`"))

    # ---- Hardcoded JSON keys: only flag inside writer/reader functions,
    # detected by presence of `ss_jsr(` OR `QJsonObject` OR `QJsonValue` in
    # the file. Frame.h itself is the source of truth and exempt.
    if path.name not in {"Frame.h", "Frame.cpp"}:
        text = src_text
        if "ss_jsr(" in text or "QJsonObject" in text or "QJsonValue" in text:
            for i, raw in enumerate(lines, start=1):
                if fenced(i):
                    continue
                scrubbed = _strip_line_comments(raw)
                # Match `"<key>"` literal where <key> is in the curated list.
                for m in re.finditer(r'"([A-Za-z_][A-Za-z0-9_]*)"', scrubbed):
                    if m.group(1) in _PROJECT_KEY_LITERALS:
                        out.append(Finding(
                            i, "keys-hardcoded-literal",
                            f"hardcoded JSON key `\"{m.group(1)}\"` -- "
                            f"use `Keys::{_camel(m.group(1))}` from Frame.h"))
                        break  # one finding per line is plenty
    return out


def _function_name(func_node, src: bytes) -> str:
    """Return the function's name from a function_definition node, or "".
    Walks declarator -> identifier / qualified_identifier / field_identifier.
    For nested `qualified_identifier` (e.g. `A::B::C::foo`), the rightmost
    segment is the actual function name."""
    decl = func_node.child_by_field_name("declarator")
    while decl is not None and decl.type == "function_declarator":
        decl = decl.child_by_field_name("declarator")
    if decl is None:
        return ""

    # Walk to the rightmost segment of any qualified-identifier chain.
    while decl is not None and decl.type == "qualified_identifier":
        nested = None
        for child in decl.children:
            if child.type in ("identifier", "field_identifier",
                              "destructor_name", "qualified_identifier",
                              "operator_name", "template_function"):
                nested = child
        if nested is None:
            return ""
        decl = nested

    if decl.type in ("identifier", "field_identifier", "destructor_name"):
        return _node_text(decl, src)
    if decl.type == "operator_name":
        return _node_text(decl, src)
    if decl.type == "template_function":
        name = decl.child_by_field_name("name")
        if name is not None:
            return _node_text(name, src)
    return ""


def _type_name(node, src: bytes) -> str:
    """Return the named type's identifier, or "" for anonymous declarations."""
    name = node.child_by_field_name("name")
    if name is not None:
        return _node_text(name, src)
    # alias_declaration has no `name` field but its first identifier is the
    # alias being declared.
    for child in node.children:
        if child.type in ("type_identifier", "identifier"):
            return _node_text(child, src)
    return ""


def _is_nested_type(node) -> bool:
    """True when `node` lives inside a class/struct body."""
    parent = node.parent
    while parent is not None:
        if parent.type == "field_declaration_list":
            return True
        if parent.type == "translation_unit":
            return False
        parent = parent.parent
    return False


def _has_function_ancestor(node) -> bool:
    """True when `node` is nested inside another function_definition --
    typically a lambda body or an inline helper. Such nodes inherit the
    enclosing function's @brief."""
    parent = node.parent
    while parent is not None:
        if parent.type in ("function_definition", "lambda_expression"):
            return True
        parent = parent.parent
    return False


def _in_signals_section(node, src: bytes) -> bool:
    """Heuristic: walk backwards to the previous access_specifier inside the
    same field_declaration_list. Returns True when that specifier is
    `signals` or `Q_SIGNALS`."""
    parent = node.parent
    if parent is None or parent.type != "field_declaration_list":
        return False
    cur = node
    while cur is not None:
        prev = cur.prev_sibling
        if prev is None:
            return False
        if prev.type == "access_specifier":
            spec = _node_text(prev, src).strip().rstrip(":").strip()
            return spec in ("signals", "Q_SIGNALS")
        cur = prev
    return False


def _is_anonymous_namespace(node) -> bool:
    """True when a namespace_definition has no name (`namespace { ... }`).
    The grammar exposes the name as a `namespace_identifier` or
    `nested_namespace_specifier` child; anonymous namespaces have neither."""
    for child in node.children:
        if child.type in ("namespace_identifier", "nested_namespace_specifier"):
            return False
    return True


def _anon_member_label(node, src: bytes) -> str:
    """Return a short human label for a top-level entity inside an anonymous
    namespace -- function name, type name, or first declared identifier --
    so the report can point at the actual symbol rather than just the line."""
    if node.type == "function_definition":
        name = _function_name(node, src)
        return name if name else "<function>"
    if node.type in ("class_specifier", "struct_specifier", "enum_specifier",
                     "union_specifier", "type_definition", "alias_declaration"):
        name = _type_name(node, src)
        return name if name else "<type>"
    if node.type == "template_declaration":
        for child in node.children:
            if child.type in ("function_definition", "class_specifier",
                              "struct_specifier"):
                return _anon_member_label(child, src)
        return "<template>"
    if node.type == "declaration":
        for child in _walk(node):
            if child.type in ("identifier", "field_identifier"):
                return _node_text(child, src)
    return "<entity>"


# ---------------------------------------------------------------------------
# Line-text helpers (string/comment stripping)
# ---------------------------------------------------------------------------

_STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"')
_CHAR_RE = re.compile(r"'(?:\\.|[^'\\])'")


def _strip_strings_and_line_comments(line: str) -> str:
    """Replace string / char literals and `//` line comments with spaces of
    the same length. Used before token-level regex scans so a `// emit` in
    a comment can't trigger qt-bare-emit. Block comments (`/* */`) are
    left alone -- multi-line comment handling lives in the formatter."""
    # Doxygen / block-comment continuation lines: ` * emit ...` is prose,
    # never code, even though it isn't a `//` line comment. Strip the
    # whole line when it starts with optional whitespace + `*`.
    if re.match(r"^\s*\*(?!\w)", line):
        return " " * len(line)
    line = _STRING_RE.sub(lambda m: " " * len(m.group(0)), line)
    line = _CHAR_RE.sub(lambda m: " " * len(m.group(0)), line)
    idx = line.find("//")
    if idx >= 0:
        line = line[:idx] + " " * (len(line) - idx)
    return line


def _strip_line_comments(line: str) -> str:
    """Replace only `//` line comments with spaces -- string literals remain
    visible. Used when the rule cares about the contents of strings."""
    idx = line.find("//")
    if idx >= 0:
        return line[:idx] + " " * (len(line) - idx)
    return line


def _camel(snake: str) -> str:
    """Map a JSON key to its Frame.h Keys:: identifier (PascalCase). The
    constants in Frame.h are PascalCase versions of the camelCase JSON
    keys -- e.g. `frameStart` -> `FrameStart`."""
    return snake[:1].upper() + snake[1:]


# ---------------------------------------------------------------------------
# Comment-narration rule (CLAUDE.md "Comments & Doxygen" tone bans)
# ---------------------------------------------------------------------------
#
# CLAUDE.md bans tutorial voice ("we", "let's"), throat-clearing ("Note that",
# "FYI"), rot-references ("this PR", "the recent fix"), caller references
# ("Used by X", "Called from Y"), hedging ("for now", "ideally"), and
# tutorial-restating ("This is a function that...", "Iterates over..."). The
# scanner runs over `//` line-comment text and ` * ` doxygen continuation
# text; @brief lines are excluded (a one-liner @brief that mentions "we" is
# rare but possible -- better than nuking 100% of legitimate phrasing).
#
# Third-party files keep their upstream prose unchanged. Detection by path
# (`ThirdParty/`, the SimpleCrypt vendored crypto helper, the Lemon Squeezy
# example template) avoids touching code we don't own.

_NARRATION_PATTERNS = [
    # Tutorial voice -- "we", "let's", "now we"
    (re.compile(r"\b(?:we|let's|let us|now we|first we)\b", re.IGNORECASE),
     "tutorial voice (`we`/`let's`) -- rewrite without the first person"),
    # "This is a helper / function / class that..."
    (re.compile(
        r"\bThis is a (?:helper |small |simple |utility )?"
        r"(?:function|class|method|variable|helper|piece of code|wrapper|macro)\b"),
     "tutorial voice (`This is a function/class/...`) -- the code already says that"),
    # Throat-clearing prefixes
    (re.compile(r"\b(?:Note that|Please note|FYI)\b"),
     "throat-clearing (`Note that`/`FYI`) -- drop the prefix or drop the comment"),
    # Caller references
    (re.compile(
        r"\b(?:Used by|Called from|Called by|Invoked by|invoked from)\b"),
     "caller-reference (`Used by`/`Called from`) -- those rot; if it's load-bearing, "
     "encode it as an invariant in the function instead"),
    # Rot-references (phrasing tied to a transient state)
    (re.compile(
        r"\b(?:this PR|recent fix|the recent (?:fix|change)|"
        r"as mentioned above|see below|see above)\b", re.IGNORECASE),
     "rot-reference (`this PR`/`see below`) -- moves the moment it ships; put context "
     "in the commit message"),
    # Restating the code
    (re.compile(r"\b(?:Loops over|Iterates over|Iterate through)\b"),
     "restating the code (`Loops over`/`Iterates over`) -- the loop already says that"),
    # Hedging
    (re.compile(
        r"\b(?:For now,|for now,|For clarity,|for clarity,|ideally,)\b"),
     "hedging (`for now`/`ideally`) -- be definite or delete"),
    # Filler adverbs
    (re.compile(r"\b(?:simply|basically)\b", re.IGNORECASE),
     "filler adverb (`simply`/`basically`) -- delete or rephrase"),
]

# Files whose prose is upstream and not authored by this codebase.
_NARRATION_VENDORED_PATH_HINTS = (
    "/ThirdParty/",
    "/lemonsqueezy/",  # Pro license-server example template
    "/SimpleCrypt.",   # Andre Somers' vendored crypto helper
)


def _is_brief_line(text: str) -> bool:
    """True when this comment line carries the doxygen `@brief` marker.

    A one-line `@brief` that happens to contain a banned word is rare but
    legitimate (e.g., the @brief of a `sharedDatasetUnit` helper that returns
    "the unit shared by all datasets, or empty when they disagree" — the
    word `they` could trip a future tutorial-voice check). Excluding @brief
    lines keeps the false-positive rate low without making the rule toothless.
    """
    return "@brief" in text


def _is_vendored_path(path: Path) -> bool:
    """True when @p path points to a vendored / upstream-prose file. The
    check is path-based because vendored files don't carry a uniform marker
    in their text -- some have an SPDX banner, some have a license comment,
    some have neither."""
    s = str(path)
    return any(hint in s for hint in _NARRATION_VENDORED_PATH_HINTS)


# ---------------------------------------------------------------------------
# Qt-style preferences for console output and stream formatting
# ---------------------------------------------------------------------------
#
# CLAUDE.md does not call these out by name, but they are universally Qt-style
# violations: a Qt application that mixes `printf` / `std::cout` / `std::endl`
# with `qDebug` / `\n` is harder to retarget at the Qt logging-categories
# infrastructure, breaks message-handler routing, and pays a synchronous-flush
# cost on every emission. Two cases legitimately need raw stdio (the qDebug
# message handler itself, and Windows console attachment) -- those wrap the
# region in `// code-verify off` to silence the rule with intent.

_STDIO_PATTERNS = [
    (re.compile(r"\bstd::(?:cout|cerr|clog)\b"),
     "qt-prefer-qdebug",
     "`std::cout` / `std::cerr` -- prefer `qDebug()` / `qWarning()` "
     "(routes through the Qt message handler and the Console widget)"),
    (re.compile(r"^\s*#include\s+<iostream>"),
     "qt-prefer-qdebug",
     "`<iostream>` -- prefer `<QDebug>` (Qt streams integrate with the message handler)"),
    (re.compile(r"\bprintf\s*\("),
     "qt-prefer-qdebug",
     "`printf(...)` -- prefer `qDebug()` (Qt routes through the message handler "
     "and the Console widget)"),
    (re.compile(r"\bstd::endl\b"),
     "qt-prefer-newline",
     "`std::endl` flushes the stream on every line -- use `'\\n'` "
     "(or Qt::endl when explicit flushing is intentional)"),
]


def _stdio_findings(src_text: str, path: Path,
                    fence_mask: list[bool]) -> list[Finding]:
    """Flag raw-stdio output (`std::cout`, `<iostream>`, `printf`, `std::endl`)
    in Qt source code. Strings and `//` line comments are masked first so
    `// printf("...")` in prose doesn't fire."""
    if path.suffix not in (".cpp", ".cc", ".cxx", ".mm", ".h", ".hpp", ".hxx"):
        return []
    if _is_vendored_path(path):
        return []
    out: list[Finding] = []
    for i, raw in enumerate(src_text.split("\n"), start=1):
        if i - 1 < len(fence_mask) and fence_mask[i - 1]:
            continue
        scrubbed = _strip_strings_and_line_comments(raw)
        for pat, kind, msg in _STDIO_PATTERNS:
            if pat.search(scrubbed):
                out.append(Finding(i, kind, msg))
                break
    return out


_TRAILING_DOXY_RE = re.compile(r"/\*\*<")


def _trailing_doxy_findings(src_text: str, path: Path,
                            fence_mask: list[bool]) -> list[Finding]:
    """Flag trailing-style doxygen `/**< ... */` member comments in headers.

    CLAUDE.md "Headers (.h) -- strict rule" forbids member-variable
    comments. Trailing `/**< description */` is the doxygen-specific form
    of that ban -- the underlying problem is the same: names + types are
    the documentation.
    """
    if path.suffix not in (".h", ".hpp", ".hxx"):
        return []
    if _is_vendored_path(path):
        return []
    out: list[Finding] = []
    for i, raw in enumerate(src_text.split("\n"), start=1):
        if i - 1 < len(fence_mask) and fence_mask[i - 1]:
            continue
        if _TRAILING_DOXY_RE.search(raw):
            out.append(Finding(
                i, "doc-trailing-member",
                "header member-variable trailing doxygen `/**< ... */` -- "
                "delete it (CLAUDE.md \"No member-variable comments\" rule)"))
    return out


def _comment_narration_findings(src_text: str, path: Path,
                                fence_mask: list[bool]) -> list[Finding]:
    """Scan comment text in @p src_text and emit one finding per banned pattern.

    Walks every line: `// ...` line comments contribute their tail text, and
    ` * ...` lines inside a doxygen / block comment contribute their body
    (minus the leading `*`). String literals and code outside comments are
    not visited. @brief lines are skipped.
    """
    if _is_vendored_path(path):
        return []

    out: list[Finding] = []
    lines = src_text.split("\n")
    for i, raw in enumerate(lines, start=1):
        if i - 1 < len(fence_mask) and fence_mask[i - 1]:
            continue

        # `//` line comment -- everything after the marker is prose.
        m_line = re.search(r"//(.*)$", raw)
        # ` * ` doxygen continuation -- a line starting with whitespace + `*`
        # but NOT starting a `*/` close or a `/**` open is prose. The opener
        # `/**` line itself often contains @brief and runs through the
        # @brief filter below.
        m_doxy = re.match(r"^\s*\*\s?(.*)$", raw)

        if m_line:
            text = m_line.group(1)
        elif m_doxy and not m_doxy.group(1).startswith("/"):
            text = m_doxy.group(1)
        else:
            continue

        if _is_brief_line(text):
            continue

        for pat, msg in _NARRATION_PATTERNS:
            if pat.search(text):
                out.append(Finding(i, "comment-narration", msg))
                break  # one finding per line is plenty
    return out


# ---------------------------------------------------------------------------
# QML semantic rules (line-based -- no tree-sitter for QML)
# ---------------------------------------------------------------------------

# Files that are allowed to use direct font.pixelSize/font.family. These are
# the dashboard widgets that compute zoom-dependent font sizes; the
# `font:` helpers don't accept dynamic sizes.
_FONT_PIXEL_OK_FILES = frozenset({
    "FFTPlot.qml", "Plot.qml", "MultiPlot.qml", "Bar.qml",
    "Gauge.qml", "Compass.qml", "DataGrid.qml", "Accelerometer.qml",
    "Gyroscope.qml", "GPS.qml", "LEDPanel.qml", "Plot3D.qml",
    "Waterfall.qml", "ImageView.qml", "Terminal.qml", "PlotWidget.qml",
    "VisualRange.qml", "NotificationLog.qml",
    # Project-editor visualizations that compute zoom-dependent font sizes
    # are also exempt -- the `font:` helpers don't accept a dynamic
    # pixelSize, and these views need it for Ctrl+Wheel zoom.
    "FlowDiagram.qml",
})


def _qml_rules(src: str, path: Path, fence_mask: list[bool]) -> list[Finding]:
    """QML semantic rules. Line-based: tree-sitter doesn't have a published
    QML grammar, and the existing tokenizer in code-verify.py already
    tracks the structure we need."""
    out: list[Finding] = []
    if path.suffix != ".qml":
        return out

    file_allows_pixel = path.name in _FONT_PIXEL_OK_FILES

    for i, raw in enumerate(src.split("\n"), start=1):
        if i - 1 < len(fence_mask) and fence_mask[i - 1]:
            continue
        # Strip trailing comment -- still a flat scan, but enough to avoid
        # complaining about comment text.
        line = _strip_line_comments(raw)

        # qml-font-pixel: `font.pixelSize:` / `font.family:` outside the
        # dashboard whitelist. JS function bodies inside QML still match,
        # but those very rarely set fonts.
        if not file_allows_pixel:
            if re.search(r"\bfont\.(pixelSize|pointSize|family|bold|italic"
                         r"|weight|capitalization)\s*:", line):
                out.append(Finding(
                    i, "qml-font-pixel",
                    "use `font: Cpp_Misc_CommonFonts.uiFont` (or another "
                    "helper) instead of individual `font.*` sub-properties"))

        # qml-bus-type-int: `busType: <integer>` or `BusType: <int>`. The
        # property name on a `Source` is `busType`; the C++ enum is
        # `SerialStudio.BusType.UART`. Reject literal ints.
        m = re.search(r"\bbusType\s*:\s*(-?\d+)\b", line)
        if m:
            out.append(Finding(
                i, "qml-bus-type-int",
                f"hardcoded busType `{m.group(1)}` -- use "
                f"`SerialStudio.BusType.<NAME>`"))

    return out


# ---------------------------------------------------------------------------
# Public entry point
# ---------------------------------------------------------------------------

def analyze(path: Path, src_text: str, fence_mask: list[bool]) -> list[Finding]:
    """Run every applicable rule against `src_text` for `path`. The driver
    in code-verify.py wraps each Finding as a Violation."""
    suffix = path.suffix.lower()
    out: list[Finding] = []
    if suffix in (".cpp", ".cc", ".cxx", ".c", ".h", ".hpp", ".hxx", ".mm"):
        out.extend(_cpp_rules(src_text.encode("utf-8"), path, fence_mask))
        out.extend(_comment_narration_findings(src_text, path, fence_mask))
        out.extend(_trailing_doxy_findings(src_text, path, fence_mask))
        out.extend(_stdio_findings(src_text, path, fence_mask))
        return out
    if suffix == ".qml":
        out.extend(_qml_rules(src_text, path, fence_mask))
        out.extend(_comment_narration_findings(src_text, path, fence_mask))
        return out
    return out
