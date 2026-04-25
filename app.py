"""
TinyLang — Flask Web Compiler
Thin subprocess wrapper: all compiler logic lives in the C executable.
"""

import os
import sys
import json
import time
import subprocess
import tempfile
import logging

# ── Safe logging (avoids OSError on Windows with Flask reloader) ──
log = logging.getLogger('tinylang')
log.setLevel(logging.DEBUG)
# Only add handler if none exist (avoid duplicates on reload)
if not log.handlers:
    try:
        _handler = logging.StreamHandler(sys.stdout)
        _handler.setFormatter(logging.Formatter('[%(levelname)s] %(message)s'))
        log.addHandler(_handler)
    except OSError:
        pass  # stdout also broken — silently skip

from flask import Flask, render_template, request, jsonify
from compiler.grammar_tool import analyze_grammar

app = Flask(__name__)

# ── Locate the C compiler binary ───────────────────────────────
def _find_compiler():
    """Return the path to tinylang_compiler(.exe), or None if not found."""
    base_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(base_dir, 'tinylang_compiler.exe'),
        os.path.join(base_dir, 'tinylang_compiler'),
    ]
    for c in candidates:
        if os.path.isfile(c):
            return c
    return None


@app.route('/')
def index():
    return render_template('index.html')


@app.route('/compile', methods=['POST'])
def compile_code():
  try:
    data = request.get_json(silent=True) or {}

    # ── Debug: log received payload ──────────────────────────
    log.debug(f"/compile received keys: {list(data.keys())}")

    # Support both 'code' (frontend sends this) and 'source_code'
    source_code = data.get('code', data.get('source_code', ''))

    log.debug(f"source_code length : {len(source_code)}")
    log.debug(f"source_code empty  : {not source_code.strip()}")
    if source_code:
        preview = source_code[:200].replace('\n', '\\n').replace('\r', '\\r')
        log.debug(f"source_code preview: {preview}")

    compiler_path = _find_compiler()
    log.debug(f"compiler_path      : {compiler_path}")

    if not compiler_path:
        return jsonify({
            'status': 'error',
            'error': 'Compiler not found. Run `make` (or build.bat) in the project root first.',
            'tokens': [], 'parse_table': {}, 'ast': {},
            'symbol_table': [], 'optimizer': {}, 'compile_time_ms': 0,
        })

    if not source_code.strip():
        return jsonify({
            'status': 'error',
            'error': 'No source code provided.',
            'tokens': [], 'parse_table': {}, 'ast': {},
            'symbol_table': [], 'optimizer': {}, 'compile_time_ms': 0,
        })

    start = time.time()

    # ── Write source to a temp file and pass path as argv[1] ──
    # This avoids Windows stdin-pipe deadlocks and \r\n corruption.
    tmp_file = None
    try:
        with tempfile.NamedTemporaryFile(
                mode='w', suffix='.tl', delete=False, encoding='utf-8') as f:
            f.write(source_code)
            tmp_file = f.name

        log.debug(f"temp file          : {tmp_file}")

        try:
            proc = subprocess.run(
                [compiler_path, tmp_file],
                capture_output=True,
                text=True,
                timeout=10,
                encoding='utf-8',
                errors='replace',
            )
        except FileNotFoundError:
            return jsonify({
                'status': 'error',
                'error': 'Compiler not found. Run `make` (or build.bat) in the project root first.',
                'tokens': [], 'parse_table': {}, 'ast': {},
                'symbol_table': [], 'optimizer': {}, 'compile_time_ms': 0,
            })
        except subprocess.TimeoutExpired:
            return jsonify({
                'status': 'error',
                'error': 'Compilation timed out.',
                'tokens': [], 'parse_table': {}, 'ast': {},
                'symbol_table': [], 'optimizer': {}, 'compile_time_ms': 10000,
            })

    finally:
        # Always clean up temp file
        if tmp_file and os.path.exists(tmp_file):
            try:
                os.unlink(tmp_file)
            except OSError:
                pass

    elapsed_ms = round((time.time() - start) * 1000, 2)

    # ── Debug: log subprocess results ────────────────────────
    log.debug(f"proc.returncode    : {proc.returncode}")
    if proc.stderr:
        log.debug(f"proc.stderr        : {proc.stderr[:300]}")
    log.debug(f"proc.stdout (500ch): {proc.stdout[:500]}")

    if proc.returncode != 0:
        # Try to parse stdout as JSON error first
        stderr_msg = (proc.stderr or '').strip() or 'Unknown compiler error.'
        try:
            err_json = json.loads(proc.stdout)
            error_msg = err_json.get('error', stderr_msg)
        except (json.JSONDecodeError, ValueError):
            error_msg = stderr_msg
        return jsonify({
            'status': 'error',
            'error': error_msg,
            'tokens': [], 'parse_table': {}, 'ast': {},
            'symbol_table': [], 'optimizer': {}, 'compile_time_ms': elapsed_ms,
        })

    # Parse JSON from compiler stdout
    try:
        result = json.loads(proc.stdout)
    except (json.JSONDecodeError, ValueError):
        log.error(f"JSON parse failed on stdout (first 1000 chars): {proc.stdout[:1000]}")
        return jsonify({
            'status': 'error',
            'error': 'Compiler returned malformed output.',
            'tokens': [], 'parse_table': {}, 'ast': {},
            'symbol_table': [], 'optimizer': {}, 'compile_time_ms': elapsed_ms,
        })

    # Ensure all expected top-level keys exist for the frontend
    result.setdefault('tokens',       [])
    result.setdefault('parse_table',  {})
    result.setdefault('ast',          {})
    result.setdefault('symbol_table', [])
    result.setdefault('optimizer',    {})
    result.setdefault('status',       'success')
    result.setdefault('error',        '')
    result['compile_time_ms'] = elapsed_ms

    return jsonify(result)

  except Exception as exc:
    # Top-level safety net: always return JSON, never an HTML error page
    log.exception("Unhandled exception in /compile")
    return jsonify({
        'status': 'error',
        'error': f'Internal server error: {str(exc)}',
        'tokens': [], 'parse_table': {}, 'ast': {},
        'symbol_table': [], 'optimizer': {}, 'compile_time_ms': 0,
    }), 500


@app.route('/grammar-analyze', methods=['POST'])
def grammar_analyze():
    """Analyze a user-provided grammar: FIRST/FOLLOW, parse table, conflicts.
    This route is kept in Python for the Grammar Lab (supports arbitrary grammars).
    """
    data = request.get_json(silent=True) or {}
    grammar_text  = data.get('grammar', '')
    input_string  = data.get('input_string', '')

    if not grammar_text.strip():
        return jsonify({'status': 'error', 'error': 'No grammar provided.'})

    try:
        result = analyze_grammar(grammar_text, input_string or None)
        return jsonify(result)
    except Exception as e:
        return jsonify({'status': 'error', 'error': f'Analysis error: {str(e)}'})


if __name__ == '__main__':
    app.run(debug=True, port=3000)
