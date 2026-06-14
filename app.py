"""
TinyLang — Flask Web Compiler
Thin subprocess wrapper: all compiler logic lives in the C executable.
"""

# os module file paths aur system operations ke liye use hota hai
import os

# sys module python runtime info aur stdout handle ke liye
import sys

# json module data ko JSON format me convert/read karne ke liye
import json

# time module compilation ka time measure karne ke liye
import time

# subprocess external executable (C compiler) run karne ke liye
import subprocess

# temporary file banane ke liye use hota hai
import tempfile

# logs print aur debugging ke liye
import logging


# ---------------- Safe Logging Setup ----------------

# tinylang naam ka logger object banaya
log = logging.getLogger('tinylang')

# logger level DEBUG rakha taaki sab logs aaye
log.setLevel(logging.DEBUG)

# duplicate handlers avoid karne ke liye check
if not log.handlers:
    try:
        # stdout par logs print karne ke liye stream handler
        _handler = logging.StreamHandler(sys.stdout)

        # log format set kiya
        _handler.setFormatter(
            logging.Formatter('[%(levelname)s] %(message)s')
        )

        # handler logger me add kiya
        log.addHandler(_handler)

    except OSError:
        # agar stdout issue de to silently ignore
        pass


# Flask imports
from flask import Flask, render_template, request, jsonify

# Grammar lab ka analyzer import kiya
from compiler.grammar_tool import analyze_grammar


# Flask app object create kiya
app = Flask(__name__)


# ---------------- Compiler Executable Find Function ----------------

def _find_compiler():
    """
    tinylang_compiler(.exe) ka path return karega
    agar file na mile to None return hoga
    """

    # current file ka folder path nikala
    base_dir = os.path.dirname(os.path.abspath(__file__))

    # possible executable names
    candidates = [
        os.path.join(base_dir, 'tinylang_compiler.exe'),
        os.path.join(base_dir, 'tinylang_compiler'),
    ]

    # ek ek candidate check karo
    for c in candidates:
        if os.path.isfile(c):
            return c   # mil gaya to return

    return None   # nahi mila


# ---------------- Home Route ----------------

@app.route('/')
def index():
    # index.html page open karega
    return render_template('index.html')


# ---------------- Compile Route ----------------

@app.route('/compile', methods=['POST'])
def compile_code():
    try:
        # frontend se json data lo
        data = request.get_json(silent=True) or {}

        # debug me received keys print karo
        log.debug(f"/compile received keys: {list(data.keys())}")

        # code ya source_code dono keys support karta hai
        source_code = data.get('code', data.get('source_code', ''))

        # source code ki details logs me
        log.debug(f"source_code length : {len(source_code)}")
        log.debug(f"source_code empty  : {not source_code.strip()}")

        # agar code hai to preview dikhao
        if source_code:
            preview = source_code[:200].replace('\n', '\\n').replace('\r', '\\r')
            log.debug(f"source_code preview: {preview}")

        # compiler executable ka path find karo
        compiler_path = _find_compiler()

        log.debug(f"compiler_path      : {compiler_path}")

        # agar compiler nahi mila
        if not compiler_path:
            return jsonify({
                'status': 'error',
                'error': 'Compiler not found. Run make/build first.',
                'tokens': [],
                'parse_table': {},
                'ast': {},
                'symbol_table': [],
                'optimizer': {},
                'compile_time_ms': 0
            })

        # agar source code blank hai
        if not source_code.strip():
            return jsonify({
                'status': 'error',
                'error': 'No source code provided.',
                'tokens': [],
                'parse_table': {},
                'ast': {},
                'symbol_table': [],
                'optimizer': {},
                'compile_time_ms': 0
            })

        # start time note karo
        start = time.time()

        # temp file variable
        tmp_file = None

        try:
            # temp source file create karo
            with tempfile.NamedTemporaryFile(
                mode='w',
                suffix='.tl',
                delete=False,
                encoding='utf-8'
            ) as f:

                # source code file me likho
                f.write(source_code)

                # temp file ka naam save karo
                tmp_file = f.name

            log.debug(f"temp file          : {tmp_file}")

            try:
                # C compiler run karo
                proc = subprocess.run(
                    [compiler_path, tmp_file],
                    capture_output=True,   # stdout/stderr capture karega
                    text=True,             # output text form me
                    timeout=10,            # max 10 sec
                    encoding='utf-8',
                    errors='replace'
                )

            except FileNotFoundError:
                # executable missing ho gaya
                return jsonify({
                    'status': 'error',
                    'error': 'Compiler executable not found.',
                    'tokens': [],
                    'parse_table': {},
                    'ast': {},
                    'symbol_table': [],
                    'optimizer': {},
                    'compile_time_ms': 0
                })

            except subprocess.TimeoutExpired:
                # compiler bahut der le raha tha
                return jsonify({
                    'status': 'error',
                    'error': 'Compilation timed out.',
                    'tokens': [],
                    'parse_table': {},
                    'ast': {},
                    'symbol_table': [],
                    'optimizer': {},
                    'compile_time_ms': 10000
                })

        finally:
            # temp file hamesha delete karo
            if tmp_file and os.path.exists(tmp_file):
                try:
                    os.unlink(tmp_file)
                except OSError:
                    pass

        # total compile time calculate karo
        elapsed_ms = round((time.time() - start) * 1000, 2)

        # subprocess logs
        log.debug(f"proc.returncode    : {proc.returncode}")

        if proc.stderr:
            log.debug(f"proc.stderr        : {proc.stderr[:300]}")

        log.debug(f"proc.stdout (500ch): {proc.stdout[:500]}")

        # agar compiler error return kare
        if proc.returncode != 0:

            stderr_msg = (proc.stderr or '').strip() or 'Unknown compiler error.'

            try:
                # stdout me json error ho sakta hai
                err_json = json.loads(proc.stdout)
                error_msg = err_json.get('error', stderr_msg)

            except:
                error_msg = stderr_msg

            return jsonify({
                'status': 'error',
                'error': error_msg,
                'tokens': [],
                'parse_table': {},
                'ast': {},
                'symbol_table': [],
                'optimizer': {},
                'compile_time_ms': elapsed_ms
            })

        # successful output json parse karo
        try:
            result = json.loads(proc.stdout)

        except:
            log.error("JSON parse failed")

            return jsonify({
                'status': 'error',
                'error': 'Compiler returned malformed output.',
                'tokens': [],
                'parse_table': {},
                'ast': {},
                'symbol_table': [],
                'optimizer': {},
                'compile_time_ms': elapsed_ms
            })

        # frontend ke liye missing keys default set karo
        result.setdefault('tokens', [])
        result.setdefault('parse_table', {})
        result.setdefault('ast', {})
        result.setdefault('symbol_table', [])
        result.setdefault('optimizer', {})
        result.setdefault('status', 'success')
        result.setdefault('error', '')

        # compile time add karo
        result['compile_time_ms'] = elapsed_ms

        # final response bhejo
        return jsonify(result)

    except Exception as exc:
        # koi bhi unexpected error handle
        log.exception("Unhandled exception in /compile")

        return jsonify({
            'status': 'error',
            'error': f'Internal server error: {str(exc)}',
            'tokens': [],
            'parse_table': {},
            'ast': {},
            'symbol_table': [],
            'optimizer': {},
            'compile_time_ms': 0
        }), 500


# ---------------- Grammar Analyze Route ----------------

@app.route('/grammar-analyze', methods=['POST'])
def grammar_analyze():
    """
    Grammar Lab route:
    FIRST, FOLLOW, Parse Table, conflicts check karega
    """

    # frontend se json data lo
    data = request.get_json(silent=True) or {}

    # grammar text lo
    grammar_text = data.get('grammar', '')

    # optional input string lo
    input_string = data.get('input_string', '')

    # grammar blank hua to error
    if not grammar_text.strip():
        return jsonify({
            'status': 'error',
            'error': 'No grammar provided.'
        })

    try:
        # grammar analysis function call
        result = analyze_grammar(grammar_text, input_string or None)

        return jsonify(result)

    except Exception as e:
        return jsonify({
            'status': 'error',
            'error': f'Analysis error: {str(e)}'
        })


# ---------------- Main Run ----------------

if __name__ == '__main__':
    # flask app run karo port 3000 par
    app.run(debug=True, port=3000)