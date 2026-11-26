import os
import sys
import time
import math
import json
import zlib
import queue
import struct
import shutil
import base64
import pathlib
import datetime
import argparse
import platform
import subprocess
import webbrowser
import threading
import http.server
import urllib.parse
import concurrent.futures
import xml.etree.ElementTree as ET

def parse_args():
    parser = argparse.ArgumentParser(prog="lyra-rhi-tests", description="helper script to run tests for Lyra-Engine")
    parser.add_argument("executable", help="path to the executable for rhi-tests / testkit")
    parser.add_argument("--directory", help="target directory for the generated test output and report")
    parser.add_argument("-j", "--jobs", type=int, default=None, help="number of parallel test worker processes (default: min(cpu_count, 8))")
    parser.add_argument("--pixel-tolerance", type=int, default=2, help="per-channel threshold for imperceptible noise (default: 2)")
    parser.add_argument("--mismatch-threshold", type=float, default=1.0, help="max percentage of mismatched pixels allowed (default: 1.0%%)")
    parser.add_argument("--rmse-threshold", type=float, default=2.0, help="max allowed RMSE across RGB channels (default: 2.0)")
    parser.add_argument("--interactive", action="store_true", default=None, help="open browser immediately and stream results live via local HTTP server")
    parser.add_argument("--non-interactive", "--headless", dest="non_interactive", action="store_true", default=False, help="run in non-interactive / headless mode without opening the browser")
    parser.add_argument("--serve", action="store_true", default=False, help="keep the live HTTP server running after tests finish until Ctrl+C")
    parser.add_argument("--stage", choices=["all", "primitive", "usecase"], default="all", help="filter tests by stage tier: 'primitive', 'usecase', or 'all' (default: all)")
    parser.add_argument("--skip-expensive", "--skip-long", dest="skip_expensive", action="store_true", default=None, help="skip computationally expensive tests marked in their config.json")
    parser.add_argument("--no-skip-expensive", "--include-expensive", "--no-skip-long", "--include-long", dest="skip_expensive", action="store_false", default=None, help="do not skip expensive tests")
    args = parser.parse_args()
    args.executable = os.path.abspath(args.executable)
    args.directory = os.path.join(args.directory, "runs")
    if args.skip_expensive is None:
        env_skip = os.environ.get("LYRA_TESTKIT_SKIP_EXPENSIVE") or os.environ.get("LYRA_TESTKIT_SKIP_LONG")
        if env_skip is not None:
            args.skip_expensive = env_skip.strip().lower() in ("1", "true", "yes", "on")
        else:
            args.skip_expensive = False
    print("TestKit executable:", args.executable)
    print("TestKit directory:", args.directory)
    print("TestKit skip expensive tests:", args.skip_expensive)
    return args

def find_git_root(start_path=None):
    if start_path is None:
        start_path = os.getcwd()

    current_path = os.path.abspath(start_path)
    while True:
        git_dir = os.path.join(current_path, '.git')
        if os.path.exists(git_dir):
            return current_path

        parent_path = os.path.dirname(current_path)
        if parent_path == current_path:
            return None

        current_path = parent_path

def check_vulkan_info():
    try:
        subprocess.check_output(["vulkaninfo", "--summary"], stderr=subprocess.STDOUT)
        return True
    except FileNotFoundError:
        return False
    except subprocess.CalledProcessError:
        return False
    except Exception:
        return False

def prepare_run(args):
    os.makedirs(args.directory, exist_ok=True)
    for filename in os.listdir(args.directory):
        file_path = os.path.join(args.directory, filename)
        try:
            if os.path.isfile(file_path) or os.path.islink(file_path):
                os.unlink(file_path)
            elif os.path.isdir(file_path):
                shutil.rmtree(file_path)
        except Exception as e:
            print(f'Failed to delete {file_path}. Reason: {e}')

def parse_tests(path):
    tree = ET.parse(path)
    root = tree.getroot()
    tests = []
    for test in root.findall('TestCase'):
        name = test.get("name")
        assert name is not None
        test_info = {
            "name": name,
            "filename": test.get("filename"),
            "components": name.split("::"),
            "skipped": test.get("skipped", "false").lower() == "true"
        }
        tests.append(test_info)
    return tests

def list_tests(args):
    git_root = find_git_root()
    output = os.path.join(args.directory, "tests.xml")
    subprocess.check_call([args.executable, '-ltc', "-r=xml", f"-out={output}"], cwd=git_root)
    return parse_tests(output)

def filter_env_tests(tests):
    filtered_tests = tests
    filter_key = os.environ.get("LYRA_TESTKIT_FILTER", None)
    if filter_key:
        filtered_tests = [test for test in tests if filter_key in test["name"]]
    return filtered_tests

def filter_rhi_tests(tests):
    filtered_tests = []
    for test in tests:
        if test["components"][0] == "rhi":
            filtered_tests.append(test)
    return filter_env_tests(filtered_tests)

def bucketize_tests(tests):
    buckets = {}
    for test in tests:
        name = test["components"][-1]
        if name not in buckets:
            buckets[name] = []
        buckets[name].append(test)
    return buckets

RHI_TEST_ORDER = [
    # 1. Pipeline & Geometry Drawing
    "pipeline_basic",
    "pipeline_immediates",
    "pipeline_dynamic_uniform",
    "pipeline_primitive_topologies",
    "pipeline_index_formats_offsets",
    "pipeline_instanced_drawing",
    "pipeline_indirect_drawing",

    # 2. Rasterizer & Fixed-Function State
    "raster_culling_winding",
    "raster_scissor_viewport",
    "raster_fragment_discard",
    "raster_color_write_mask",
    "raster_blend_modes",

    # 3. Depth & Stencil Operations
    "depth_test",
    "depth_compare_ops",
    "depth_bias",
    "depth_stencil_test",

    # 4. Textures & Samplers
    "texture_sampling",
    "texture_sampler_modes",
    "texture_perspective_correction",
    "texture_lod",
    "texture_anisotropic_filtering",

    # 5. Render Targets & Framebuffer
    "render_target_mrt",

    # 6. Compute Pipeline & Storage
    "compute_storage_buffer",
    "compute_storage_image",
    "compute_shared_memory",
    "compute_indirect_dispatch",

    # 7. Advanced RHI Subsystems
    "ray_tracing",
    "frame_graph",

    # 8. Post-Processing Pipeline
    "postprocess_tonemapping",
    "postprocess_bloom",
    "postprocess_dof",
    "postprocess_fxaa",

    # 9. Materials & Environment
    "shading_models",
    "atmosphere",
    "atmosphere_ibl",

    # 10. Scene Renderers & Path Tracing
    "forward_renderer",
    "forward_renderer_multilight",
    "deferred_renderer",
    "deferred_renderer_multilight",
    "path_tracer",

    # 11. Specialized Surface Renderers
    "ocean",
    "volumetric_clouds",

    # 12. UI System
    "ui_system",
    "ui_node_editor",
]

RHI_TEST_ASPECTS = {
    # 1. Pipeline & Geometry Drawing
    "pipeline_basic": "Pipeline & Geometry",
    "pipeline_immediates": "Pipeline & Geometry",
    "pipeline_dynamic_uniform": "Pipeline & Geometry",
    "pipeline_primitive_topologies": "Pipeline & Geometry",
    "pipeline_index_formats_offsets": "Pipeline & Geometry",
    "pipeline_instanced_drawing": "Pipeline & Geometry",
    "pipeline_indirect_drawing": "Pipeline & Geometry",

    # 2. Rasterizer & Fixed-Function State
    "raster_culling_winding": "Rasterizer & State",
    "raster_scissor_viewport": "Rasterizer & State",
    "raster_fragment_discard": "Rasterizer & State",
    "raster_color_write_mask": "Rasterizer & State",
    "raster_blend_modes": "Rasterizer & State",

    # 3. Depth & Stencil Operations
    "depth_test": "Depth & Stencil",
    "depth_compare_ops": "Depth & Stencil",
    "depth_bias": "Depth & Stencil",
    "depth_stencil_test": "Depth & Stencil",

    # 4. Textures & Samplers
    "texture_sampling": "Textures & Samplers",
    "texture_sampler_modes": "Textures & Samplers",
    "texture_perspective_correction": "Textures & Samplers",
    "texture_lod": "Textures & Samplers",
    "texture_anisotropic_filtering": "Textures & Samplers",

    # 5. Render Targets & Framebuffer
    "render_target_mrt": "Render Targets",

    # 6. Compute Pipeline & Storage
    "compute_storage_buffer": "Compute Pipeline",
    "compute_storage_image": "Compute Pipeline",
    "compute_shared_memory": "Compute Pipeline",
    "compute_indirect_dispatch": "Compute Pipeline",

    # 7. Advanced RHI Subsystems
    "ray_tracing": "Ray Tracing",
    "frame_graph": "Frame Graph",

    # 8. Post-Processing Pipeline
    "postprocess_tonemapping": "Post-Processing",
    "postprocess_bloom": "Post-Processing",
    "postprocess_dof": "Post-Processing",
    "postprocess_fxaa": "Post-Processing",

    # 9. Materials & Environment
    "shading_models": "Materials & Environment",
    "atmosphere": "Materials & Environment",
    "atmosphere_ibl": "Materials & Environment",

    # 10. Scene Renderers & Path Tracing
    "forward_renderer": "Scene Renderers",
    "forward_renderer_multilight": "Scene Renderers",
    "deferred_renderer": "Scene Renderers",
    "deferred_renderer_multilight": "Scene Renderers",
    "path_tracer": "Scene Renderers",

    # 11. Specialized Surface Renderers
    "ocean": "Specialized Renderers",
    "volumetric_clouds": "Specialized Renderers",

    # 12. UI System
    "ui_system": "UI System",
    "ui_node_editor": "UI System",
}

def get_test_order_index(test_name):
    if test_name in RHI_TEST_ORDER:
        return RHI_TEST_ORDER.index(test_name)
    return len(RHI_TEST_ORDER) + 100

def get_test_metadata(test_name, git_root):
    """
    Returns (tier_int, complexity_float, tier_name, aspect_str) for a test.
    tier 0 -> "primitive" (default)
    tier 1 -> "usecase"
    complexity -> float between 0.0 and 1.0 (default: 0.0)
    aspect_str -> optional category/aspect string from config.json or None
    """
    cfg_path = os.path.join(git_root, "Tests", test_name, "config.json")
    tier_int, tier_name = 0, "primitive"
    complexity = 0.0
    aspect_str = None
    if os.path.exists(cfg_path):
        try:
            with open(cfg_path, "r", encoding="utf-8") as f:
                data = json.load(f)
                tier_val = data.get("tier")
                if tier_val is not None:
                    if isinstance(tier_val, int):
                        tier_int = tier_val
                        tier_name = "usecase" if tier_int > 0 else "primitive"
                    else:
                        tier_str = str(tier_val).strip().lower()
                        if tier_str in ("usecase", "use_case", "usecases"):
                            tier_int, tier_name = 1, "usecase"
                        elif tier_str in ("primitive", "primitives", "core"):
                            tier_int, tier_name = 0, "primitive"
                comp_val = data.get("complexity")
                if comp_val is not None:
                    try:
                        complexity = max(0.0, min(1.0, float(comp_val)))
                    except (ValueError, TypeError):
                        pass
                if "category" in data and isinstance(data["category"], str):
                    aspect_str = data["category"]
                elif "aspect" in data and isinstance(data["aspect"], str):
                    aspect_str = data["aspect"]
        except Exception:
            pass
    return tier_int, complexity, tier_name, aspect_str

def is_test_expensive_for_backend(test_name, backend, git_root):
    """
    Checks if a test is marked as expensive in Tests/<test_name>/config.json.
    "expensive": true -> expensive on all backends.
    "expensive": ["softgpu", ...] or "expensive": "softgpu" -> expensive on specified backend(s).
    """
    cfg_path = os.path.join(git_root, "Tests", test_name, "config.json")
    if os.path.exists(cfg_path):
        try:
            with open(cfg_path, "r", encoding="utf-8") as f:
                data = json.load(f)
                exp_val = data.get("expensive") if "expensive" in data else data.get("long")
                if exp_val is True:
                    return True
                elif isinstance(exp_val, list):
                    return backend.lower() in [str(b).strip().lower() for b in exp_val]
                elif isinstance(exp_val, str):
                    return backend.lower() == exp_val.strip().lower()
        except Exception:
            pass
    return False

def get_test_expensive_info(test_name, git_root):
    """Returns (is_expensive_any, expensive_desc) for display in the test card."""
    cfg_path = os.path.join(git_root, "Tests", test_name, "config.json")
    if os.path.exists(cfg_path):
        try:
            with open(cfg_path, "r", encoding="utf-8") as f:
                data = json.load(f)
                exp_val = data.get("expensive") if "expensive" in data else data.get("long")
                if exp_val is True:
                    return True, "all"
                elif isinstance(exp_val, list) and exp_val:
                    return True, ", ".join(str(b).strip() for b in exp_val)
                elif isinstance(exp_val, str) and exp_val:
                    return True, exp_val.strip()
        except Exception:
            pass
    return False, None

def read_png_rgba(path):
    """Reads an 8-bit PNG file using only python standard libraries and returns (width, height, bytearray_rgba)."""
    with open(path, "rb") as f:
        sig = f.read(8)
        if sig != b"\x89PNG\r\n\x1a\n":
            raise ValueError(f"Invalid PNG header: {path}")
        ihdr = None
        idat = []
        while True:
            hdr = f.read(8)
            if len(hdr) < 8:
                break
            length, ctype = struct.unpack(">I4s", hdr)
            data = f.read(length)
            f.read(4)  # CRC
            if ctype == b"IHDR":
                ihdr = data
            elif ctype == b"IDAT":
                idat.append(data)
            elif ctype == b"IEND":
                break

    if not ihdr:
        raise ValueError(f"Missing IHDR chunk: {path}")

    width, height, bitdepth, colortype, comp, filt, interlace = struct.unpack(">IIBBBBB", ihdr)
    if bitdepth != 8 or interlace != 0:
        raise ValueError(f"Unsupported PNG format in {path}: bitdepth={bitdepth}, interlace={interlace}")

    channels_map = {0: 1, 2: 3, 4: 2, 6: 4}
    if colortype not in channels_map:
        raise ValueError(f"Unsupported PNG colortype={colortype} in {path}")
    channels = channels_map[colortype]

    raw = zlib.decompress(b"".join(idat))
    stride = 1 + width * channels
    expected_len = stride * height
    if len(raw) != expected_len:
        raise ValueError(f"Decompressed data size mismatch in {path}: {len(raw)} != {expected_len}")

    recon = bytearray(width * height * channels)

    for y in range(height):
        row_start = y * stride
        ftype = raw[row_start]
        row_in = raw[row_start + 1 : row_start + stride]
        out_start = y * width * channels
        prior_start = (y - 1) * width * channels

        if ftype == 0:  # None
            recon[out_start : out_start + width * channels] = row_in
        elif ftype == 1:  # Sub
            for i in range(width * channels):
                a = recon[out_start + i - channels] if i >= channels else 0
                recon[out_start + i] = (row_in[i] + a) & 0xFF
        elif ftype == 2:  # Up
            for i in range(width * channels):
                b = recon[prior_start + i] if y > 0 else 0
                recon[out_start + i] = (row_in[i] + b) & 0xFF
        elif ftype == 3:  # Average
            for i in range(width * channels):
                a = recon[out_start + i - channels] if i >= channels else 0
                b = recon[prior_start + i] if y > 0 else 0
                recon[out_start + i] = (row_in[i] + ((a + b) >> 1)) & 0xFF
        elif ftype == 4:  # Paeth
            for i in range(width * channels):
                a = recon[out_start + i - channels] if i >= channels else 0
                b = recon[prior_start + i] if y > 0 else 0
                c = recon[prior_start + i - channels] if (y > 0 and i >= channels) else 0
                p = a + b - c
                pa = abs(p - a)
                pb = abs(p - b)
                pc = abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                recon[out_start + i] = (row_in[i] + pr) & 0xFF
        else:
            raise ValueError(f"Unknown filter type {ftype} in {path}")

    if colortype == 6:
        return width, height, recon

    rgba = bytearray(width * height * 4)
    total_pix = width * height
    if colortype == 2:  # RGB -> RGBA
        for p in range(total_pix):
            rgba[p * 4 : p * 4 + 3] = recon[p * 3 : p * 3 + 3]
            rgba[p * 4 + 3] = 255
    elif colortype == 0:  # Grayscale -> RGBA
        for p in range(total_pix):
            g = recon[p]
            rgba[p * 4] = g
            rgba[p * 4 + 1] = g
            rgba[p * 4 + 2] = g
            rgba[p * 4 + 3] = 255
    elif colortype == 4:  # GA -> RGBA
        for p in range(total_pix):
            g = recon[p * 2]
            rgba[p * 4] = g
            rgba[p * 4 + 1] = g
            rgba[p * 4 + 2] = g
            rgba[p * 4 + 3] = recon[p * 2 + 1]

    return width, height, rgba

def write_png_rgba(path, width, height, rgba_data):
    """Writes an 8-bit RGBA PNG file using only python standard libraries."""
    raw_scanlines = bytearray()
    row_bytes = width * 4
    for y in range(height):
        raw_scanlines.append(0)  # filter type 0 (None)
        raw_scanlines.extend(rgba_data[y * row_bytes : (y + 1) * row_bytes])
    compressed = zlib.compress(raw_scanlines, level=6)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        ihdr_data = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
        f.write(struct.pack(">I4s", len(ihdr_data), b"IHDR") + ihdr_data + struct.pack(">I", zlib.crc32(b"IHDR" + ihdr_data)))
        f.write(struct.pack(">I4s", len(compressed), b"IDAT") + compressed + struct.pack(">I", zlib.crc32(b"IDAT" + compressed)))
        f.write(struct.pack(">I4s", 0, b"IEND") + struct.pack(">I", zlib.crc32(b"IEND")))

_REF_IMAGE_CACHE_LOCK = threading.Lock()
_REF_IMAGE_CACHE = {}

def get_cached_ref_rgba(path):
    with _REF_IMAGE_CACHE_LOCK:
        if path not in _REF_IMAGE_CACHE:
            _REF_IMAGE_CACHE[path] = read_png_rgba(path)
        return _REF_IMAGE_CACHE[path]

def check_perceptual_mismatch(ref_path, test_path, diff_output_path=None, pixel_tolerance=2, max_mismatch_pct=1.0, rmse_threshold=2.0):
    if not os.path.exists(ref_path):
        return {
            "passed": False,
            "error": f"Reference image missing: {ref_path}",
            "rmse": 0.0,
            "mismatch_pct": 100.0,
            "diff_pixels": 0,
            "total_pixels": 0,
            "max_diff": 0,
            "diff_image": None,
        }

    if not os.path.exists(test_path):
        return {
            "passed": False,
            "error": f"Output image missing: {test_path}",
            "rmse": 0.0,
            "mismatch_pct": 100.0,
            "diff_pixels": 0,
            "total_pixels": 0,
            "max_diff": 0,
            "diff_image": None,
        }

    try:
        w_ref, h_ref, ref_rgba = get_cached_ref_rgba(ref_path)
        w_test, h_test, test_rgba = read_png_rgba(test_path)
    except Exception as e:
        return {
            "passed": False,
            "error": f"PNG decode failed: {e}",
            "rmse": 0.0,
            "mismatch_pct": 100.0,
            "diff_pixels": 0,
            "total_pixels": 0,
            "max_diff": 0,
            "diff_image": None,
        }

    if (w_ref, h_ref) != (w_test, h_test):
        return {
            "passed": False,
            "error": f"Dimension mismatch: Reference ({w_ref}x{h_ref}) vs Output ({w_test}x{h_test})",
            "rmse": 0.0,
            "mismatch_pct": 100.0,
            "diff_pixels": 0,
            "total_pixels": 0,
            "max_diff": 0,
            "diff_image": None,
        }

    total_pixels = w_ref * h_ref
    diff_pixels = 0
    sq_err_sum = 0
    max_diff = 0

    for i in range(0, total_pixels * 4, 4):
        dr = abs(ref_rgba[i] - test_rgba[i])
        dg = abs(ref_rgba[i + 1] - test_rgba[i + 1])
        db = abs(ref_rgba[i + 2] - test_rgba[i + 2])
        da = abs(ref_rgba[i + 3] - test_rgba[i + 3])

        m = max(dr, dg, db, da)
        if m > max_diff:
            max_diff = m

        sq_err_sum += dr * dr + dg * dg + db * db

        if m > pixel_tolerance:
            diff_pixels += 1

    rmse = math.sqrt(sq_err_sum / (total_pixels * 3))
    mismatch_pct = (diff_pixels / total_pixels) * 100.0

    passed = (rmse <= rmse_threshold) and (mismatch_pct <= max_mismatch_pct)
    error_msg = None
    if not passed:
        reasons = []
        if rmse > rmse_threshold:
            reasons.append(f"RMSE {rmse:.2f} > {rmse_threshold:.2f}")
        if mismatch_pct > max_mismatch_pct:
            reasons.append(f"Mismatch {mismatch_pct:.2f}% > {max_mismatch_pct:.2f}%")
        error_msg = "Perceptual mismatch: " + ", ".join(reasons)

    diff_image_saved = None
    if diff_output_path and diff_pixels > 0:
        try:
            diff_rgba = bytearray(total_pixels * 4)
            for i in range(0, total_pixels * 4, 4):
                dr = abs(ref_rgba[i] - test_rgba[i])
                dg = abs(ref_rgba[i + 1] - test_rgba[i + 1])
                db = abs(ref_rgba[i + 2] - test_rgba[i + 2])
                da = abs(ref_rgba[i + 3] - test_rgba[i + 3])
                if max(dr, dg, db, da) > pixel_tolerance:
                    diff_rgba[i] = 255
                    diff_rgba[i + 1] = 0
                    diff_rgba[i + 2] = 120
                    diff_rgba[i + 3] = 255
                else:
                    gray = int(0.299 * ref_rgba[i] + 0.587 * ref_rgba[i + 1] + 0.114 * ref_rgba[i + 2]) >> 2
                    diff_rgba[i] = gray
                    diff_rgba[i + 1] = gray
                    diff_rgba[i + 2] = gray
                    diff_rgba[i + 3] = 255
            write_png_rgba(diff_output_path, w_ref, h_ref, diff_rgba)
            diff_image_saved = diff_output_path
        except Exception as e:
            print(f"Warning: Failed to write diff image: {e}")

    return {
        "passed": passed,
        "error": error_msg,
        "rmse": rmse,
        "mismatch_pct": mismatch_pct,
        "diff_pixels": diff_pixels,
        "total_pixels": total_pixels,
        "max_diff": max_diff,
        "diff_image": diff_image_saved,
    }

class SSEBroadcaster:
    """Manages active Server-Sent Events (SSE) connections for live browser updates."""
    def __init__(self):
        self._lock = threading.Lock()
        self._clients = set()

    def add_client(self):
        q = queue.Queue(maxsize=1000)
        with self._lock:
            self._clients.add(q)
        return q

    def remove_client(self, q):
        with self._lock:
            self._clients.discard(q)

    def broadcast(self, data):
        with self._lock:
            for q in list(self._clients):
                try:
                    q.put_nowait(data)
                except queue.Full:
                    pass

class LiveTestRequestHandler(http.server.BaseHTTPRequestHandler):
    """Zero-dependency HTTP request handler for the live test report and image streaming."""
    broadcaster = None
    runs_dir = None
    git_root = None
    html_provider = None

    def log_message(self, format, *args):
        # Silence access logs to keep terminal output clean
        pass

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path

        if path in ("/", "/index.html", "/report.html"):
            html = self.__class__.html_provider()
            encoded = html.encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(encoded)))
            self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            self.wfile.write(encoded)
            return

        if path == "/events":
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Connection", "keep-alive")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()

            q = self.broadcaster.add_client()
            try:
                self.wfile.write(b": connected\n\n")
                self.wfile.flush()
                while True:
                    try:
                        msg = q.get(timeout=1.0)
                        payload = f"data: {json.dumps(msg)}\n\n".encode("utf-8")
                        self.wfile.write(payload)
                        self.wfile.flush()
                    except queue.Empty:
                        self.wfile.write(b": ping\n\n")
                        self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError, Exception):
                pass
            finally:
                self.broadcaster.remove_client(q)
            return

        if path.startswith("/runs/"):
            rel_path = urllib.parse.unquote(path[len("/runs/"):])
            full_path = os.path.abspath(os.path.join(self.runs_dir, rel_path))
            runs_root = os.path.abspath(self.runs_dir)
            if os.path.commonpath([full_path, runs_root]) == runs_root and os.path.isfile(full_path):
                self._serve_file(full_path)
                return

        if path.startswith("/Tests/"):
            rel_path = urllib.parse.unquote(path[len("/Tests/"):])
            full_path = os.path.abspath(os.path.join(self.git_root, "Tests", rel_path))
            tests_root = os.path.abspath(os.path.join(self.git_root, "Tests"))
            if os.path.commonpath([full_path, tests_root]) == tests_root and os.path.isfile(full_path):
                self._serve_file(full_path)
                return

        self.send_error(404, "Not Found")

    def _serve_file(self, full_path):
        content_type = "application/octet-stream"
        if full_path.endswith(".png"):
            content_type = "image/png"
        elif full_path.endswith(".html"):
            content_type = "text/html; charset=utf-8"
        elif full_path.endswith(".json"):
            content_type = "application/json"
        elif full_path.endswith(".md") or full_path.endswith(".txt"):
            content_type = "text/plain; charset=utf-8"

        try:
            with open(full_path, "rb") as f:
                data = f.read()
            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            self.wfile.write(data)
        except Exception:
            self.send_error(500, "Error reading file")

def start_live_server(runs_dir, git_root, html_provider):
    """Starts an ephemeral local HTTP server on an unoccupied port."""
    broadcaster = SSEBroadcaster()

    class Handler(LiveTestRequestHandler):
        pass

    Handler.broadcaster = broadcaster
    Handler.runs_dir = runs_dir
    Handler.git_root = git_root
    Handler.html_provider = staticmethod(html_provider)

    # Binding to port 0 asks the OS to assign an available, unoccupied port
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    port = server.server_address[1]

    server_thread = threading.Thread(target=server.serve_forever, daemon=True)
    server_thread.start()
    return server, port, broadcaster

def generate_results_json(args, results, unit_passed=True, finished=True):
    """Generates a structured, machine-readable results.json for CI/CD pipelines."""
    total_checks = 0
    passed_checks = 0
    skipped_checks = 0
    failed_checks = 0

    test_data = {}
    for test_name, buckets in results.items():
        test_data[test_name] = {}
        metrics_map = buckets.get("_metrics", {})
        for backend, m in metrics_map.items():
            total_checks += 1
            is_skipped = bool(m.get("skipped", False))
            is_passed = bool(m.get("passed", False))
            if is_skipped:
                skipped_checks += 1
            elif is_passed:
                passed_checks += 1
            else:
                failed_checks += 1

            img_path = buckets.get(backend)
            diff_path = buckets.get(f"{backend}_diff")

            test_data[test_name][backend] = {
                "passed": is_passed,
                "skipped": is_skipped,
                "skip_reason": m.get("skip_reason"),
                "error": m.get("error"),
                "rmse": m.get("rmse", 0.0),
                "mismatch_pct": m.get("mismatch_pct", 0.0),
                "diff_pixels": m.get("diff_pixels", 0),
                "total_pixels": m.get("total_pixels", 0),
                "max_diff": m.get("max_diff", 0),
                "image": os.path.relpath(img_path, args.directory) if img_path and os.path.exists(img_path) else None,
                "diff": os.path.relpath(diff_path, args.directory) if diff_path and os.path.exists(diff_path) else None,
            }

    overall_passed = (failed_checks == 0) and unit_passed
    summary = {
        "timestamp": datetime.datetime.now().isoformat(),
        "platform": platform.system(),
        "machine": platform.machine(),
        "finished": finished,
        "overall_passed": overall_passed,
        "unit_tests_passed": unit_passed,
        "total_checks": total_checks,
        "passed_checks": passed_checks,
        "skipped_checks": skipped_checks,
        "failed_checks": failed_checks,
        "tests": test_data,
    }

    path = os.path.join(args.directory, "results.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)
    return path, summary

def run_rhi_tests(args, buckets, on_progress=None):
    results = {}
    all_passed = True

    git_root = find_git_root()
    assert git_root, "Not within a git repository!"

    # Initialize results structure
    for test_name in buckets:
        results[test_name] = {
            "reference": os.path.join(git_root, "Tests", test_name, "reference.png"),
            "_metrics": {},
            "_available_backends": [v["components"][1] for v in buckets[test_name]],
        }

    # Partition tasks by stage tier
    staged_tasks = {}
    skipped_expensive_tasks = []
    for test_name, variants in buckets.items():
        directory = os.path.join(args.directory, test_name)
        os.makedirs(directory, exist_ok=True)
        ref_image = results[test_name]["reference"]
        tier_int, complexity, tier_name, _ = get_test_metadata(test_name, git_root)
        if tier_int not in staged_tasks:
            staged_tasks[tier_int] = (tier_name, [])
        for variant in variants:
            backend = variant["components"][1]
            item = (test_name, variant, directory, ref_image, complexity)
            if getattr(args, "skip_expensive", False) and is_test_expensive_for_backend(test_name, backend, git_root):
                skipped_expensive_tasks.append(item)
            else:
                staged_tasks[tier_int][1].append(item)

    results_lock = threading.Lock()

    def handle_test_finished(item, proc_success, proc_output):
        nonlocal all_passed
        test_name, variant, directory, ref_image, _ = item
        full_name = variant["name"]
        backend = variant["components"][1]

        test_result = os.path.join(directory, f"{backend}.png")
        diff_result = os.path.join(directory, f"{backend}_diff.png")
        skip_file = os.path.join(directory, f"{backend}.skip")

        if proc_success and os.path.exists(skip_file):
            skip_reason = "Feature not supported"
            try:
                with open(skip_file, "r", encoding="utf-8") as sf:
                    line = sf.readline().strip()
                    if line:
                        skip_reason = line
            except Exception:
                pass
            metrics = {
                "passed": True,
                "skipped": True,
                "skip_reason": skip_reason,
                "error": None,
                "rmse": 0.0,
                "mismatch_pct": 0.0,
                "diff_pixels": 0,
                "total_pixels": 0,
                "max_diff": 0,
                "diff_image": None,
            }
        elif proc_success and os.path.exists(test_result):
            test_cfg_path = os.path.join(git_root, "Tests", test_name, "config.json")
            cur_mismatch_thresh = args.mismatch_threshold
            cur_rmse_thresh = args.rmse_threshold
            cur_pixel_tol = args.pixel_tolerance
            if os.path.exists(test_cfg_path):
                try:
                    with open(test_cfg_path, "r") as cfg_f:
                        cfg_data = json.load(cfg_f)
                        cur_mismatch_thresh = cfg_data.get("mismatch_threshold", cur_mismatch_thresh)
                        cur_rmse_thresh = cfg_data.get("rmse_threshold", cur_rmse_thresh)
                        cur_pixel_tol = cfg_data.get("pixel_tolerance", cur_pixel_tol)
                except Exception:
                    pass

            metrics = check_perceptual_mismatch(
                ref_image,
                test_result,
                diff_output_path=diff_result,
                pixel_tolerance=cur_pixel_tol,
                max_mismatch_pct=cur_mismatch_thresh,
                rmse_threshold=cur_rmse_thresh,
            )
        else:
            err_detail = "Execution failed"
            if not os.path.exists(test_result) and proc_success:
                err_detail = "Output image not generated"
            metrics = {
                "passed": False,
                "error": err_detail,
                "rmse": 0.0,
                "mismatch_pct": 100.0,
                "diff_pixels": 0,
                "total_pixels": 0,
                "max_diff": 0,
                "diff_image": None,
            }

        with results_lock:
            results[test_name][backend] = test_result
            results[test_name]["_metrics"][backend] = metrics
            if metrics.get("diff_image"):
                results[test_name][f"{backend}_diff"] = metrics["diff_image"]

            print(f"::{full_name}")
            if not proc_success:
                all_passed = False
                print(f"  -> [FAIL] Execution failed: {full_name}")
                if proc_output.strip():
                    print(f"     Output: {proc_output.strip()[:500]}")
            elif metrics.get("skipped"):
                print(f"  -> [SKIP] {backend} ({metrics.get('skip_reason', 'Feature not supported')})")
            elif metrics["passed"]:
                print(f"  -> [PASS] {backend} (RMSE: {metrics['rmse']:.2f}, Diff: {metrics['mismatch_pct']:.2f}%, MaxDiff: {metrics['max_diff']})")
            else:
                all_passed = False
                print(f"  -> [FAIL] {backend}: {metrics['error']}")

            if on_progress:
                on_progress(test_name, backend, metrics, test_result, results[test_name].get(f"{backend}_diff"))

    def run_backend_chunk(backend, chunk):
        remaining_tasks = list(chunk)
        proc_env = os.environ.copy()
        proc_env["LYRA_REPO_ROOT"] = git_root
        proc_env["LYRA_TESTKIT_RUNS_DIR"] = args.directory

        while remaining_tasks:
            current_batch = remaining_tasks
            remaining_tasks = []

            task_map = {item[1]["name"]: item for item in current_batch}
            filter_str = ",".join(task_map.keys())

            cmd = [args.executable, f"-tc={filter_str}"]
            try:
                proc = subprocess.Popen(
                    cmd,
                    cwd=git_root,
                    env=proc_env,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    bufsize=1,
                )
            except Exception as e:
                for item in current_batch:
                    handle_test_finished(item, False, str(e))
                continue

            current_test_name = None
            current_test_output = []
            executed_in_chunk = set()

            for raw_line in iter(proc.stdout.readline, ""):
                line = raw_line.rstrip("\r\n")
                if line.startswith(">>> TEST_START: "):
                    current_test_name = line[len(">>> TEST_START: "):].strip()
                    current_test_output = []
                elif line.startswith(">>> TEST_END: "):
                    status_str = line[len(">>> TEST_END: "):].strip()
                    test_success = (status_str == "PASS")
                    if current_test_name in task_map:
                        item = task_map[current_test_name]
                        handle_test_finished(item, test_success, "\n".join(current_test_output))
                        executed_in_chunk.add(current_test_name)
                    current_test_name = None
                    current_test_output = []
                else:
                    if current_test_name is not None:
                        current_test_output.append(line)

            proc.stdout.close()
            ret_code = proc.wait()

            if current_test_name and current_test_name in task_map:
                item = task_map[current_test_name]
                crash_output = f"Process terminated unexpectedly with code {ret_code}"
                if current_test_output:
                    crash_output += ":\n" + "\n".join(current_test_output[-10:])
                handle_test_finished(item, False, crash_output)
                executed_in_chunk.add(current_test_name)

            unexecuted = [item for item in current_batch if item[1]["name"] not in executed_in_chunk]
            if unexecuted:
                remaining_tasks = unexecuted

    # Immediately report skipped expensive tests without spawning worker processes
    if skipped_expensive_tasks:
        print(f"\n>>> Skipping {len(skipped_expensive_tasks)} expensive test variant(s)...")
        for item in skipped_expensive_tasks:
            test_name, variant, directory, ref_image, _ = item
            full_name = variant["name"]
            backend = variant["components"][1]
            metrics = {
                "passed": True,
                "skipped": True,
                "skip_reason": "Skipped (configured as expensive test)",
                "error": None,
                "rmse": 0.0,
                "mismatch_pct": 0.0,
                "diff_pixels": 0,
                "total_pixels": 0,
                "max_diff": 0,
                "diff_image": None,
            }
            with results_lock:
                results[test_name][backend] = None
                results[test_name]["_metrics"][backend] = metrics
                print(f"::{full_name}")
                print(f"  -> [SKIP] {backend} (Skipped: configured as expensive test)")
                if on_progress:
                    on_progress(test_name, backend, metrics, None, None)

    max_workers = args.jobs if (args.jobs is not None and args.jobs > 0) else max(1, min(os.cpu_count() or 4, 8))
    sorted_tiers = [k for k in sorted(staged_tasks.keys()) if len(staged_tasks[k][1]) > 0]
    total_stages = len(sorted_tiers)
    total_tasks_count = sum(len(staged_tasks[k][1]) for k in sorted_tiers)
    print(f">>> Running {total_tasks_count} test variants across {total_stages} stage(s) with {max_workers} parallel workers...")

    for idx, tier_int in enumerate(sorted_tiers, 1):
        tier_name, stage_tasks = staged_tasks[tier_int]
        stage_title = tier_name.capitalize() if tier_name else f"Tier {tier_int}"
        print(f"\n>>> [Stage {idx}/{total_stages}: {stage_title}] Running {len(stage_tasks)} test variants...")

        # For usecase tests (higher complexity), use smaller chunk sizes so heavy CPU/GPU tests distribute across all cores
        chunk_size = 2 if tier_int > 0 else 4

        backend_groups = {}
        for item in stage_tasks:
            backend = item[1]["components"][1]
            if backend not in backend_groups:
                backend_groups[backend] = []
            backend_groups[backend].append(item)

        backend_chunks = {backend: [] for backend in backend_groups}
        for backend, b_tasks in backend_groups.items():
            b_tasks.sort(key=lambda it: (get_test_order_index(it[0]), it[1]["name"]))
            for i in range(0, len(b_tasks), chunk_size):
                backend_chunks[backend].append((backend, b_tasks[i : i + chunk_size]))

        batches = []
        max_chunks = max((len(c) for c in backend_chunks.values()), default=0)
        for i in range(max_chunks):
            for backend in ["softgpu", "metal", "d3d12", "vulkan"]:
                if backend in backend_chunks and i < len(backend_chunks[backend]):
                    batches.append(backend_chunks[backend][i])

        with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as executor:
            futures = [
                executor.submit(run_backend_chunk, backend, chunk)
                for backend, chunk in batches
            ]
            concurrent.futures.wait(futures)

    return results, all_passed

def run_unit_tests(args):
    git_root = find_git_root()
    assert git_root, "Not within a git repository!"
    exec_dir = os.path.dirname(args.executable)
    exec_ext = os.path.splitext(args.executable)[1]
    unit_exec = os.path.join(exec_dir, f"lyra-unit-tests{exec_ext}")
    print(">>> Running Unit Tests...")
    try:
        unit_filter = os.environ.get("LYRA_UNIT_FILTER", None)
        if os.path.exists(unit_exec):
            cmd = [unit_exec]
            if unit_filter:
                cmd.append(f"-tc={unit_filter}")
            subprocess.check_call(cmd, cwd=git_root)
        else:
            cmd = [args.executable, "-tce=rhi*"]
            if unit_filter:
                cmd.append(f"-tc={unit_filter}")
            subprocess.check_call(cmd, cwd=git_root)
        print(">>> Unit Tests PASSED.")
        return True
    except subprocess.CalledProcessError as e:
        print(f">>> Unit Tests FAILED with exit code {e.returncode}.")
        return False

def generate_html_report(args, results, unit_success=True, finished=True, for_web_server=False):
    sequence = ["reference", "softgpu"]

    os_name = platform.system()
    if os_name == "Windows":
        sequence.append("d3d12")
    if os_name == "Darwin":
        sequence.append("metal")
    if check_vulkan_info():
        sequence.append("vulkan")

    total_test_groups = len(results)
    timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    total_checks = 0
    passed_checks = 0
    skipped_checks = 0
    failed_checks = 0
    completed_checks = 0

    # Count total expected checks
    for _, variants in results.items():
        for key in sequence:
            if key != "reference":
                total_checks += 1

    for _, buckets in results.items():
        metrics_dict = buckets.get("_metrics", {})
        for _, metrics in metrics_dict.items():
            completed_checks += 1
            if metrics.get("skipped", False):
                skipped_checks += 1
            elif metrics.get("passed", False):
                passed_checks += 1
            else:
                failed_checks += 1

    if not finished:
        overall_status_text = "RUNNING..."
        overall_status_class = "stat-running"
    elif failed_checks == 0 and unit_success:
        overall_status_text = "ALL PASSED"
        overall_status_class = "stat-passed"
    else:
        overall_status_text = "FAILED"
        overall_status_class = "stat-failed"

    html_content = []
    html_content.append(f'''<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Lyra TestKit Report</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600;700&family=Roboto+Mono:wght@400;500;600&display=swap" rel="stylesheet">
    <style>
        :root {{
            --bg: #282c34;
            --surface: #21252b;
            --surface-low: #181a1f;
            --surface-high: #2c313a;
            --text: #abb2bf;
            --text-muted: #5c6370;
            --accent: #61afef;
            --accent-glow: rgba(97, 175, 239, 0.3);
            --border: #3e4451;
            --success: #98c379;
            --error: #e06c75;
            --font-main: 'Inter', sans-serif;
            --font-code: 'Roboto Mono', monospace;
        }}
        * {{ box-sizing: border-box; }}
        body {{
            background-color: var(--bg);
            color: var(--text);
            font-family: var(--font-main);
            margin: 0;
            padding: 0;
            line-height: 1.6;
        }}
        .container {{
            max-width: 98%;
            margin: 0 auto;
            padding: 2rem 3rem;
        }}
        header {{
            display: flex;
            justify-content: space-between;
            align-items: flex-end;
            border-bottom: 2px solid var(--border);
            padding-bottom: 1.5rem;
            margin-bottom: 2rem;
        }}
        .brand {{
            display: flex;
            flex-direction: column;
        }}
        h1 {{
            margin: 0;
            font-size: 2rem;
            font-weight: 700;
            letter-spacing: -0.05rem;
            color: var(--accent);
            text-transform: uppercase;
        }}
        .subtitle {{
            color: var(--text-muted);
            font-size: 0.9rem;
            margin-top: 0.2rem;
        }}
        .summary {{
            display: flex;
            gap: 2rem;
        }}
        .stat {{
            display: flex;
            flex-direction: column;
            align-items: flex-end;
        }}
        .stat-label {{ font-size: 0.7rem; text-transform: uppercase; color: var(--text-muted); font-weight: 600; }}
        .stat-value {{ font-family: var(--font-code); font-size: 1.1rem; color: var(--text); }}
        .stat-badge {{
            font-weight: 700;
            font-size: 1.1rem;
            font-family: var(--font-code);
            transition: color 0.3s;
        }}
        .stat-running {{
            color: var(--accent);
            animation: pulse 1.2s infinite ease-in-out;
        }}
        .stat-passed {{ color: var(--success); }}
        .stat-failed {{ color: var(--error); }}

        @keyframes pulse {{
            0%, 100% {{ opacity: 0.6; }}
            50% {{ opacity: 1; }}
        }}

        table {{
            width: 100%;
            border-collapse: separate;
            border-spacing: 0;
            background: var(--surface);
            border-radius: 12px;
            overflow: hidden;
            border: 1px solid var(--border);
            table-layout: fixed;
        }}
        th {{
            background-color: var(--surface-low);
            color: var(--text-muted);
            text-transform: uppercase;
            font-size: 0.75rem;
            font-weight: 700;
            padding: 1rem 0.75rem;
            text-align: left;
            border-bottom: 1px solid var(--border);
            position: sticky;
            top: 0;
            z-index: 10;
        }}
        th.col-target {{
            width: 260px;
        }}
        th.col-backend {{
            text-align: center;
        }}
        td {{
            padding: 0.85rem 0.75rem;
            border-bottom: 1px solid var(--border);
            vertical-align: top;
        }}
        tr:last-child td {{ border-bottom: none; }}
        tr:hover td {{ background-color: var(--surface-high); }}

        .test-info {{
            width: 260px;
            vertical-align: top;
        }}
        .test-card {{
            display: flex;
            flex-direction: column;
            gap: 0.45rem;
            padding: 0.1rem 0;
        }}
        .test-name {{
            font-family: var(--font-code);
            font-weight: 700;
            font-size: 0.95rem;
            display: block;
            color: var(--accent);
            word-break: break-word;
            line-height: 1.35;
        }}
        .test-tag {{
            display: inline-block;
            align-self: flex-start;
            padding: 0.15rem 0.5rem;
            background: var(--surface-low);
            border: 1px solid var(--border);
            border-radius: 4px;
            font-size: 0.72rem;
            color: var(--text-muted);
            font-weight: 500;
        }}
        .test-row {{
            display: flex;
            justify-content: space-between;
            align-items: center;
            font-size: 0.74rem;
            font-family: var(--font-code);
            border-bottom: 1px dashed rgba(255, 255, 255, 0.08);
            padding-bottom: 0.2rem;
        }}
        .test-row-label {{
            color: var(--text-muted);
        }}
        .test-row-val {{
            color: var(--text);
            font-weight: 600;
        }}
        .test-row-val.primitive {{ color: var(--accent); }}
        .test-row-val.usecase {{ color: #e5c07b; }}
        .test-docs-row {{
            margin-top: 0.35rem;
        }}

        .docs-btn {{
            display: inline-flex;
            align-items: center;
            justify-content: center;
            width: 100%;
            padding: 0.4rem 0.8rem;
            background-color: var(--surface-high);
            color: var(--accent);
            border: 1px solid var(--border);
            border-radius: 6px;
            text-decoration: none;
            font-size: 0.75rem;
            font-weight: 600;
            transition: all 0.2s;
            cursor: pointer;
        }}
        .docs-btn:hover {{
            background-color: var(--accent);
            color: var(--bg);
            border-color: var(--accent);
            box-shadow: 0 0 15px var(--accent-glow);
        }}
        .docs-btn.disabled {{
            opacity: 0.3;
            pointer-events: none;
            filter: grayscale(1);
        }}

        .img-cell {{
            display: flex;
            flex-direction: column;
            align-items: center;
            width: 100%;
        }}
        .img-wrap {{
            width: 100%;
            aspect-ratio: 16 / 9;
            display: flex;
            align-items: center;
            justify-content: center;
            background: #101216;
            border: 1px solid var(--border);
            border-radius: 8px;
            overflow: hidden;
            position: relative;
        }}
        .empty-thumb {{
            background: rgba(0, 0, 0, 0.25);
            border: 1px dashed var(--border);
        }}
        .test-img {{
            width: 100%;
            height: 100%;
            object-fit: contain;
            background: #000;
            transition: transform 0.25s cubic-bezier(0.4, 0, 0.2, 1), box-shadow 0.25s;
            cursor: pointer;
            display: block;
        }}
        .test-img:hover {{
            transform: scale(1.03);
            box-shadow: 0 8px 24px rgba(0, 0, 0, 0.6);
        }}

        .meta-wrap {{
            width: 100%;
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 0.35rem;
            margin-top: 0.5rem;
            min-height: 52px;
            justify-content: flex-start;
        }}

        .badge {{
            display: inline-block;
            padding: 0.25rem 0.55rem;
            font-size: 0.7rem;
            font-family: var(--font-code);
            font-weight: 600;
            border-radius: 4px;
            text-align: center;
            max-width: 100%;
            word-break: break-word;
            line-height: 1.3;
        }}
        .badge-ref {{
            background-color: rgba(97, 175, 239, 0.15);
            color: var(--accent);
            border: 1px solid rgba(97, 175, 239, 0.4);
        }}
        .badge-pass {{
            background-color: rgba(152, 195, 121, 0.15);
            color: var(--success);
            border: 1px solid rgba(152, 195, 121, 0.4);
        }}
        .badge-fail {{
            background-color: rgba(224, 108, 117, 0.15);
            color: var(--error);
            border: 1px solid rgba(224, 108, 117, 0.4);
        }}
        .badge-skip {{
            background-color: rgba(229, 192, 123, 0.15);
            color: var(--warning);
            border: 1px solid rgba(229, 192, 123, 0.4);
        }}
        .badge-pending {{
            background-color: rgba(92, 99, 112, 0.15);
            color: var(--text-muted);
            border: 1px dashed var(--border);
            animation: pulse 1.5s infinite ease-in-out;
        }}

        .skip-reason {{
            font-size: 0.7rem;
            color: var(--text-muted);
            font-family: var(--font-code);
            text-align: center;
            max-width: 100%;
            word-break: break-word;
            line-height: 1.3;
        }}

        .diff-btn {{
            display: inline-flex;
            align-items: center;
            padding: 0.22rem 0.55rem;
            font-size: 0.7rem;
            font-family: var(--font-code);
            background: var(--surface-low);
            color: var(--text-muted);
            border: 1px solid var(--border);
            border-radius: 4px;
            cursor: pointer;
            transition: all 0.2s;
        }}
        .diff-btn:hover {{
            background: var(--accent);
            color: var(--bg);
            border-color: var(--accent);
        }}

        .empty-state {{
            color: var(--text-muted);
            font-style: italic;
            font-size: 0.8rem;
        }}

        /* Lightbox */
        .lightbox {{
            display: none;
            position: fixed;
            z-index: 10000;
            left: 0;
            top: 0;
            width: 100%;
            height: 100%;
            background-color: rgba(0, 0, 0, 0.6);
            backdrop-filter: blur(8px);
            justify-content: center;
            align-items: center;
            cursor: zoom-out;
        }}
        .lightbox-content {{
            max-width: 95vw;
            max-height: 90vh;
            object-fit: contain;
            border-radius: 4px;
            background-color: #000;
            box-shadow: 0 0 50px rgba(0,0,0,0.5);
            animation: zoom-in 0.2s ease-out;
        }}
        @keyframes zoom-in {{
            from {{ transform: scale(0.9); opacity: 0; }}
            to {{ transform: scale(1); opacity: 1; }}
        }}
        .lightbox-title {{
            position: absolute;
            bottom: 2rem;
            background: rgba(0,0,0,0.8);
            padding: 0.5rem 1.5rem;
            border-radius: 2rem;
            font-family: var(--font-code);
            color: var(--accent);
            border: 1px solid var(--border);
        }}

        .docs-content {{
            background-color: var(--surface);
            width: 60%;
            max-width: 900px;
            max-height: 80vh;
            border-radius: 12px;
            border: 1px solid var(--border);
            display: flex;
            flex-direction: column;
            box-shadow: 0 20px 50px rgba(0,0,0,0.5);
            animation: zoom-in 0.2s ease-out;
            cursor: default;
        }}
        .docs-header {{
            padding: 1rem 1.5rem;
            border-bottom: 1px solid var(--border);
            display: flex;
            justify-content: space-between;
            align-items: center;
            background-color: var(--surface-low);
        }}
        #docsTitle {{
            font-family: var(--font-code);
            font-weight: 700;
            color: var(--accent);
            font-size: 1.1rem;
        }}
        .close-docs {{
            font-size: 1.5rem;
            cursor: pointer;
            color: var(--text-muted);
            transition: color 0.2s;
        }}
        .close-docs:hover {{ color: var(--error); }}
        .docs-body {{
            padding: 1.5rem;
            overflow-y: auto;
        }}
        #docsText {{
            margin: 0;
            white-space: pre-wrap;
            font-family: var(--font-main);
            font-size: 0.95rem;
            color: var(--text);
            line-height: 1.7;
        }}

        ::-webkit-scrollbar {{ width: 10px; height: 10px; }}
        ::-webkit-scrollbar-track {{ background: var(--bg); }}
        ::-webkit-scrollbar-thumb {{ background: var(--border); border-radius: 5px; }}
        ::-webkit-scrollbar-thumb:hover {{ background: var(--text-muted); }}
    </style>
</head>
<body>
<div class="container">
    <header>
        <div class="brand">
            <h1>Lyra TestKit</h1>
            <div class="subtitle">Rendering Hardware Interface Regression Report</div>
        </div>
        <div class="summary">
            <div class="stat">
                <span class="stat-label">System</span>
                <span class="stat-value">{os_name} {platform.machine()}</span>
            </div>
            <div class="stat">
                <span class="stat-label">Tests</span>
                <span class="stat-value">{total_test_groups}</span>
            </div>
            <div class="stat">
                <span class="stat-label">Visual Checks</span>
                <span class="stat-value" id="stat-progress">{completed_checks}/{total_checks} ({passed_checks} Pass, {skipped_checks} Skip, {failed_checks} Fail)</span>
            </div>
            <div class="stat">
                <span class="stat-label">Status</span>
                <span class="stat-badge {overall_status_class}" id="stat-status">{overall_status_text}</span>
            </div>
            <div class="stat">
                <span class="stat-label">Timestamp</span>
                <span class="stat-value">{timestamp}</span>
            </div>
        </div>
    </header>
''')

    html_content.append("<table>")
    html_content.append("<thead><tr>")
    html_content.append('<th class="col-target">Target Test</th>')
    for key in sequence:
        html_content.append(f'<th class="col-backend">{key}</th>')
    html_content.append('</tr></thead>')
    html_content.append("<tbody>")

    git_root = find_git_root()
    for test_name, buckets in results.items():
        tier_int, complexity, tier_name, cfg_aspect = get_test_metadata(test_name, git_root)
        aspect_name = cfg_aspect or RHI_TEST_ASPECTS.get(test_name, "Rendering")
        is_expensive, expensive_backends = get_test_expensive_info(test_name, git_root)
        readme_path = os.path.join(git_root, "Tests", test_name, "README.md")
        has_readme = os.path.exists(readme_path)
        readme_content = ""
        if has_readme:
            with open(readme_path, "r", encoding="utf-8") as f:
                readme_content = f.read()

        encoded_docs = base64.b64encode(readme_content.encode("utf-8")).decode("utf-8")

        expensive_row_html = f'''
                    <div class="test-row">
                        <span class="test-row-label">Expensive</span>
                        <span class="test-row-val">{expensive_backends}</span>
                    </div>''' if is_expensive else ''

        html_content.append('<tr>')
        html_content.append(f'''
            <td class="test-info">
                <div class="test-card">
                    <span class="test-name">{test_name}</span>
                    <span class="test-tag">{aspect_name}</span>
                    <div class="test-row">
                        <span class="test-row-label">Tier</span>
                        <span class="test-row-val {tier_name}">{tier_name.capitalize()}</span>
                    </div>
                    <div class="test-row">
                        <span class="test-row-label">Complexity</span>
                        <span class="test-row-val">{complexity:.2f}</span>
                    </div>{expensive_row_html}
                    <div class="test-docs-row">
                        <button class="docs-btn {"" if has_readme else "disabled"}"
                                onclick="openDocs('{encoded_docs}', '{test_name}')"
                                {"disabled" if not has_readme else ""}>
                            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="width:14px;height:14px;margin-right:6px;"><path d="M4 19.5A2.5 2.5 0 0 1 6.5 17H20"></path><path d="M6.5 2H20v20H6.5A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2z"></path></svg>
                            View Docs
                        </button>
                    </div>
                </div>
            </td>''')

        metrics_map = buckets.get("_metrics", {})
        available_backends = buckets.get("_available_backends")

        for key in sequence:
            cell_id = f"cell-{test_name}-{key}"
            html_content.append(f'<td id="{cell_id}"><div class="img-cell" id="container-{test_name}-{key}">')
            if key == "reference":
                if "reference" in buckets and os.path.exists(buckets["reference"]):
                    ref_uri = f"/Tests/{test_name}/reference.png" if for_web_server else pathlib.Path(os.path.abspath(buckets["reference"])).as_uri()
                    html_content.append(f'''
                        <div class="img-wrap">
                            <img src="{ref_uri}" class="test-img" onclick="openLightbox(this.src, \'{test_name} [REFERENCE]\')"/>
                        </div>
                        <div class="meta-wrap">
                            <span class="badge badge-ref">REFERENCE</span>
                        </div>
                    ''')
                else:
                    html_content.append('''
                        <div class="img-wrap empty-thumb">
                            <span class="empty-state">No Reference</span>
                        </div>
                        <div class="meta-wrap"></div>
                    ''')
            elif key in buckets and buckets[key] and os.path.exists(buckets[key]):
                image = buckets[key]
                image_uri = f"/runs/{test_name}/{key}.png" if for_web_server else pathlib.Path(os.path.abspath(image)).as_uri()

                badge_html = ""
                diff_btn_html = ""
                if key in metrics_map:
                    m = metrics_map[key]
                    if m.get("passed", False):
                        badge_html = f'<span class="badge badge-pass">PASS &bull; RMSE {m["rmse"]:.2f} &bull; {m["mismatch_pct"]:.2f}%</span>'
                    else:
                        err = m.get("error") or f'RMSE {m["rmse"]:.2f}'
                        badge_html = f'<span class="badge badge-fail" title="{err}">FAIL &bull; {err}</span>'

                    diff_img = buckets.get(f"{key}_diff")
                    if diff_img and os.path.exists(diff_img):
                        diff_uri = f"/runs/{test_name}/{key}_diff.png" if for_web_server else pathlib.Path(os.path.abspath(diff_img)).as_uri()
                        diff_btn_html = f'<button class="diff-btn" onclick="openLightbox(\'{diff_uri}\', \'{test_name} [{key.upper()} DIFF]\')">View Diff</button>'

                html_content.append(f'''
                    <div class="img-wrap">
                        <img src="{image_uri}" class="test-img" onclick="openLightbox(this.src, \'{test_name} [{key.upper()}]\')"/>
                    </div>
                    <div class="meta-wrap">
                        {badge_html}
                        {diff_btn_html}
                    </div>
                ''')
            elif key in metrics_map and metrics_map[key].get("skipped"):
                m = metrics_map[key]
                reason = m.get("skip_reason", "Feature not supported")
                html_content.append(f'''
                    <div class="img-wrap empty-thumb">
                        <span class="badge badge-skip">SKIPPED</span>
                    </div>
                    <div class="meta-wrap">
                        <span class="skip-reason" title="{reason}">{reason}</span>
                    </div>
                ''')
            else:
                if finished or (available_backends is not None and key not in available_backends):
                    html_content.append('''
                        <div class="img-wrap empty-thumb">
                            <span class="empty-state">Not Available</span>
                        </div>
                        <div class="meta-wrap"></div>
                    ''')
                else:
                    html_content.append('''
                        <div class="img-wrap empty-thumb">
                            <span class="badge badge-pending">PENDING</span>
                        </div>
                        <div class="meta-wrap"></div>
                    ''')
            html_content.append('</div></td>')

        html_content.append('</tr>')

    html_content.append("</tbody></table>")

    html_content.append('''
        <!-- Image Lightbox -->
        <div id="myLightbox" class="lightbox" onclick="closeLightbox()">
            <div id="lightboxTitle" class="lightbox-title"></div>
            <img class="lightbox-content" id="lightboxImage">
        </div>

        <!-- Docs Modal -->
        <div id="docsModal" class="lightbox" onclick="closeDocs()">
            <div class="docs-content" onclick="event.stopPropagation()">
                <div class="docs-header">
                    <span id="docsTitle"></span>
                    <span class="close-docs" onclick="closeDocs()">&times;</span>
                </div>
                <div class="docs-body">
                    <pre id="docsText"></pre>
                </div>
            </div>
        </div>
    </div>

    <script>
        const lightbox = document.getElementById('myLightbox');
        const lightboxImg = document.getElementById('lightboxImage');
        const lightboxTitle = document.getElementById('lightboxTitle');

        const docsModal = document.getElementById('docsModal');
        const docsText = document.getElementById('docsText');
        const docsTitle = document.getElementById('docsTitle');

        function openLightbox(src, title) {
            lightbox.style.display = 'flex';
            lightboxImg.src = src;
            lightboxTitle.textContent = title;
            document.body.style.overflow = 'hidden';
        }

        function closeLightbox() {
            lightbox.style.display = 'none';
            document.body.style.overflow = 'auto';
        }

        function openDocs(encodedContent, title) {
            docsModal.style.display = 'flex';
            docsText.textContent = atob(encodedContent);
            docsTitle.textContent = "Documentation: " + title;
            document.body.style.overflow = 'hidden';
        }

        function closeDocs() {
            docsModal.style.display = 'none';
            document.body.style.overflow = 'auto';
        }

        document.addEventListener('keydown', function(event) {
            if (event.key === 'Escape') {
                closeLightbox();
                closeDocs();
            }
        });

        // Live SSE Streaming Mechanism
        if (window.EventSource && location.protocol.startsWith('http')) {
            const evtSource = new EventSource('/events');

            evtSource.onmessage = function(e) {
                try {
                    const msg = JSON.parse(e.data);
                    handleLiveMessage(msg);
                } catch (err) {
                    // Ignore keepalive comments or non-JSON payloads
                }
            };

            evtSource.onerror = function() {
                // Server closed or network disconnected
            };
        }

        function handleLiveMessage(msg) {
            if (!msg) return;

            const progressElem = document.getElementById('stat-progress');
            if (progressElem && msg.completed_count !== undefined) {
                progressElem.textContent = `${msg.completed_count}/${msg.total_count} (${msg.passed_count} Pass, ${msg.skipped_count || 0} Skip, ${msg.failed_count} Fail)`;
            }

            const statusElem = document.getElementById('stat-status');

            if (msg.type === 'progress') {
                const cellId = `cell-${msg.test_name}-${msg.backend}`;
                const cell = document.getElementById(cellId);
                if (cell) {
                    const container = cell.querySelector('.img-cell') || cell;
                    if (msg.skipped) {
                        const reason = msg.skip_reason || "Feature not supported";
                        container.innerHTML = `
                            <div class="img-wrap empty-thumb">
                                <span class="badge badge-skip">SKIPPED</span>
                            </div>
                            <div class="meta-wrap">
                                <span class="skip-reason" title="${reason}">${reason}</span>
                            </div>
                        `;
                    } else if (msg.image_url) {
                        let badgeHtml = '';
                        if (msg.passed) {
                            badgeHtml = `<span class="badge badge-pass">PASS &bull; RMSE ${msg.rmse.toFixed(2)} &bull; ${msg.mismatch_pct.toFixed(2)}%</span>`;
                        } else {
                            const err = msg.error || `RMSE ${msg.rmse.toFixed(2)}`;
                            badgeHtml = `<span class="badge badge-fail" title="${err}">FAIL &bull; ${err}</span>`;
                        }

                        let diffBtnHtml = '';
                        if (msg.diff_url) {
                            const preload = new Image();
                            preload.src = msg.diff_url;
                            diffBtnHtml = `<button class="diff-btn" onclick="openLightbox('${msg.diff_url}', '${msg.test_name} [${msg.backend.toUpperCase()} DIFF]')">View Diff</button>`;
                        }

                        container.innerHTML = `
                            <div class="img-wrap">
                                <img src="${msg.image_url}" class="test-img" onclick="openLightbox('${msg.image_url}', '${msg.test_name} [${msg.backend.toUpperCase()}]')"/>
                            </div>
                            <div class="meta-wrap">
                                ${badgeHtml}
                                ${diffBtnHtml}
                            </div>
                        `;
                    }
                }
            } else if (msg.type === 'unit_tests') {
                if (!msg.passed && statusElem) {
                    statusElem.textContent = "UNIT FAILED";
                    statusElem.className = "stat-badge stat-failed";
                }
            } else if (msg.type === 'finished') {
                if (statusElem) {
                    document.querySelectorAll('.badge-pending').forEach(el => {
                        const wrap = el.closest('.img-wrap') || el;
                        wrap.innerHTML = '<span class="empty-state">Not Available</span>';
                    });
                    if (msg.overall_passed) {
                        statusElem.textContent = "ALL PASSED";
                        statusElem.className = "stat-badge stat-passed";
                    } else {
                        statusElem.textContent = "FAILED";
                        statusElem.className = "stat-badge stat-failed";
                    }
                }
            }
        }
    </script>
</body>
</html>
''')

    return "\n".join(html_content)

def save_static_html_report(args, results, unit_success=True, finished=True):
    """Saves the final standalone static HTML report to disk for offline viewing."""
    html = generate_html_report(args, results, unit_success=unit_success, finished=finished, for_web_server=False)
    path = os.path.join(args.directory, "report.html")
    with open(path, "w", encoding="utf-8") as f:
        f.write(html)
    return path

def main():
    args = parse_args()

    # Determine interactive mode
    if args.non_interactive:
        is_interactive = False
    elif args.interactive is True:
        is_interactive = True
    elif os.environ.get("CI") or os.environ.get("LYRA_TESTKIT_NON_INTERACTIVE"):
        is_interactive = False
    else:
        is_interactive = True

    prepare_run(args)
    tests = list_tests(args)
    tests = filter_rhi_tests(tests)
    git_root = find_git_root()
    if getattr(args, "stage", "all") != "all":
        tests = [t for t in tests if get_test_metadata(t["components"][-1], git_root)[2] == args.stage]
    raw_buckets = bucketize_tests(tests) if tests else {}

    # Sort test buckets by tier, canonical aspect order, and name
    sorted_names = sorted(
        raw_buckets.keys(),
        key=lambda name: (
            get_test_metadata(name, git_root)[0],
            get_test_order_index(name),
            name
        )
    )
    buckets = {name: raw_buckets[name] for name in sorted_names}

    # Pre-populate test results structure
    initial_results = {}
    for test_name in buckets:
        initial_results[test_name] = {
            "reference": os.path.join(git_root, "Tests", test_name, "reference.png"),
            "_metrics": {},
            "_available_backends": [v["components"][1] for v in buckets[test_name]]
        }

    total_expected = len(tests)

    live_state = {
        "finished": False,
        "unit_passed": True,
        "completed_count": 0,
        "total_count": total_expected,
        "passed_count": 0,
        "skipped_count": 0,
        "failed_count": 0,
    }

    broadcaster = None
    server_port = None
    current_results = initial_results

    if is_interactive:
        _, server_port, broadcaster = start_live_server(
            runs_dir=args.directory,
            git_root=git_root,
            html_provider=lambda: generate_html_report(
                args,
                current_results,
                unit_success=live_state["unit_passed"],
                finished=live_state["finished"],
                for_web_server=True
            )
        )
        url = f"http://127.0.0.1:{server_port}/"
        print(f">>> Interactive mode enabled: live report available at {url}")
        try:
            webbrowser.open(url)
        except Exception as e:
            print(f"Note: Could not automatically open browser: {e}")

    # Run unit tests
    unit_passed = run_unit_tests(args)
    live_state["unit_passed"] = unit_passed
    if broadcaster:
        broadcaster.broadcast({
            "type": "unit_tests",
            "passed": unit_passed,
        })

    rhi_passed = True
    progress_lock = threading.Lock()
    if buckets:
        def on_progress(test_name, backend, metrics, image_path, diff_path):
            with progress_lock:
                live_state["completed_count"] += 1
                if metrics.get("skipped", False):
                    live_state["skipped_count"] += 1
                elif metrics.get("passed", False):
                    live_state["passed_count"] += 1
                else:
                    live_state["failed_count"] += 1

                if broadcaster:
                    img_url = f"/runs/{test_name}/{backend}.png" if image_path and os.path.exists(image_path) else None
                    diff_url = f"/runs/{test_name}/{backend}_diff.png" if diff_path and os.path.exists(diff_path) else None
                    broadcaster.broadcast({
                        "type": "progress",
                        "test_name": test_name,
                        "backend": backend,
                        "passed": metrics.get("passed", False),
                        "skipped": metrics.get("skipped", False),
                        "skip_reason": metrics.get("skip_reason"),
                        "rmse": metrics.get("rmse", 0.0),
                        "mismatch_pct": metrics.get("mismatch_pct", 0.0),
                        "max_diff": metrics.get("max_diff", 0),
                        "error": metrics.get("error"),
                        "image_url": img_url,
                        "diff_url": diff_url,
                        "completed_count": live_state["completed_count"],
                        "total_count": live_state["total_count"],
                        "passed_count": live_state["passed_count"],
                        "skipped_count": live_state["skipped_count"],
                        "failed_count": live_state["failed_count"],
                    })

        results, rhi_passed = run_rhi_tests(args, buckets, on_progress=on_progress)
        current_results = results
    else:
        results = initial_results
        current_results = results

    # Finalize state
    live_state["finished"] = True
    overall_passed = unit_passed and rhi_passed

    if broadcaster:
        broadcaster.broadcast({
            "type": "finished",
            "overall_passed": overall_passed,
            "completed_count": live_state["completed_count"],
            "total_count": live_state["total_count"],
            "passed_count": live_state["passed_count"],
            "skipped_count": live_state["skipped_count"],
            "failed_count": live_state["failed_count"],
        })

    # Generate CI/CD manifest (results.json) and static standalone report.html
    manifest_path, _ = generate_results_json(args, results, unit_passed=unit_passed, finished=True)
    report_path = save_static_html_report(args, results, unit_success=unit_passed, finished=True)

    print("=" * 60)
    print(f"Lyra TestKit Result: {'SUCCESS' if overall_passed else 'FAILURE'}")
    print(f"  Unit Tests: {'PASSED' if unit_passed else 'FAILED'}")
    print(f"  RHI Tests:  {'PASSED' if rhi_passed else 'FAILED'}")
    print(f"  Report:     {report_path}")
    print(f"  Manifest:   {manifest_path}")
    if is_interactive and server_port:
        print(f"  Live URL:   http://127.0.0.1:{server_port}/")
    print("=" * 60)

    if is_interactive and getattr(args, "serve", False):
        print(f"\n>>> Live server running at http://127.0.0.1:{server_port}/. Press Ctrl+C to stop.")
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            print("\n>>> Server stopped.")

    if not overall_passed:
        sys.exit(1)

if __name__ == "__main__":
    main()
