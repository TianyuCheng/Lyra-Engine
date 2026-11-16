import os
import sys
import math
import zlib
import struct
import shutil
import base64
import pathlib
import datetime
import argparse
import platform
import subprocess
import webbrowser
import xml.etree.ElementTree as ET

def parse_args():
    parser = argparse.ArgumentParser(prog="lyra-rhi-tests", description="helper script to run tests for Lyra-Engine")
    parser.add_argument("executable", help="path to the executable for rhi-tests / testkit")
    parser.add_argument("--directory", help="target directory for the generated test output and report")
    parser.add_argument("--pixel-tolerance", type=int, default=2, help="per-channel threshold for imperceptible noise (default: 2)")
    parser.add_argument("--mismatch-threshold", type=float, default=1.0, help="max percentage of mismatched pixels allowed (default: 1.0%%)")
    parser.add_argument("--rmse-threshold", type=float, default=2.0, help="max allowed RMSE across RGB channels (default: 2.0)")
    args = parser.parse_args()
    args.directory = os.path.join(args.directory, "runs")
    print("TestKit executable:", args.executable)
    print("TestKit directory:", args.directory)
    return args

def find_git_root(start_path=None):
    if start_path is None:
        start_path = os.getcwd()

    # convert to absolute path
    current_path = os.path.abspath(start_path)
    while True:
        # check if .git directory exists in current path
        git_dir = os.path.join(current_path, '.git')
        if os.path.exists(git_dir):
            return current_path

        # get parent directory
        parent_path = os.path.dirname(current_path)

        # if we've reached the root of the filesystem, stop
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

    # Channels map: 0: Grayscale (1), 2: RGB (3), 4: GA (2), 6: RGBA (4)
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

    # Normalize to RGBA (4 channels)
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
        # IHDR chunk
        ihdr_data = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
        f.write(struct.pack(">I4s", len(ihdr_data), b"IHDR") + ihdr_data + struct.pack(">I", zlib.crc32(b"IHDR" + ihdr_data)))
        # IDAT chunk
        f.write(struct.pack(">I4s", len(compressed), b"IDAT") + compressed + struct.pack(">I", zlib.crc32(b"IDAT" + compressed)))
        # IEND chunk
        f.write(struct.pack(">I4s", 0, b"IEND") + struct.pack(">I", zlib.crc32(b"IEND")))

def check_perceptual_mismatch(ref_path, test_path, diff_output_path=None, pixel_tolerance=2, max_mismatch_pct=1.0, rmse_threshold=2.0):
    """
    Perceptual image comparison using Python standard libraries.
    - pixel_tolerance: per-channel absolute delta threshold below which differences are deemed imperceptible noise.
    - max_mismatch_pct: maximum allowed percentage of perceptually mismatched pixels (0-100%).
    - rmse_threshold: maximum allowed Root Mean Square Error (RMSE) across RGB channels.
    """
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
        w_ref, h_ref, ref_rgba = read_png_rgba(ref_path)
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

    diff_rgba = bytearray(total_pixels * 4) if diff_output_path else None

    for i in range(0, total_pixels * 4, 4):
        dr = abs(ref_rgba[i] - test_rgba[i])
        dg = abs(ref_rgba[i + 1] - test_rgba[i + 1])
        db = abs(ref_rgba[i + 2] - test_rgba[i + 2])
        da = abs(ref_rgba[i + 3] - test_rgba[i + 3])

        m = max(dr, dg, db, da)
        if m > max_diff:
            max_diff = m

        sq_err_sum += dr * dr + dg * dg + db * db

        is_mismatch = m > pixel_tolerance
        if is_mismatch:
            diff_pixels += 1

        if diff_rgba is not None:
            if is_mismatch:
                # Highlight differing pixels in high-contrast vivid red/magenta
                diff_rgba[i] = 255
                diff_rgba[i + 1] = 0
                diff_rgba[i + 2] = 120
                diff_rgba[i + 3] = 255
            else:
                # Dim matching pixels to dark grayscale for visual context
                gray = int(0.299 * ref_rgba[i] + 0.587 * ref_rgba[i + 1] + 0.114 * ref_rgba[i + 2]) >> 2
                diff_rgba[i] = gray
                diff_rgba[i + 1] = gray
                diff_rgba[i + 2] = gray
                diff_rgba[i + 3] = 255

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
    if diff_output_path and diff_rgba is not None:
        try:
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

def run_rhi_tests(args, buckets):
    results = {}
    all_passed = True

    git_root = find_git_root()
    assert git_root, "Not within a git repository!"

    for test_name, variants in buckets.items():
        directory = os.path.join(args.directory, test_name)
        os.makedirs(directory, exist_ok=True)
        results[test_name] = {}
        results[test_name]["reference"] = os.path.join(git_root, "Tests", test_name, "reference.png")
        results[test_name]["_metrics"] = {}

        ref_image = results[test_name]["reference"]

        for variant in variants:
            full_name = variant["name"]
            backend = variant["components"][1]
            print(f"::{full_name}")

            proc_success = True
            try:
                subprocess.check_call([args.executable, f"-tc={full_name}"], cwd=directory)
            except subprocess.CalledProcessError:
                print(f"  -> [FAIL] Execution failed: {full_name}")
                proc_success = False

            test_result = os.path.join(directory, f"{backend}.png")
            diff_result = os.path.join(directory, f"{backend}_diff.png")
            results[test_name][backend] = test_result

            if proc_success:
                metrics = check_perceptual_mismatch(
                    ref_image,
                    test_result,
                    diff_output_path=diff_result,
                    pixel_tolerance=args.pixel_tolerance,
                    max_mismatch_pct=args.mismatch_threshold,
                    rmse_threshold=args.rmse_threshold,
                )
                results[test_name]["_metrics"][backend] = metrics
                if metrics.get("diff_image"):
                    results[test_name][f"{backend}_diff"] = metrics["diff_image"]

                if metrics["passed"]:
                    print(f"  -> [PASS] {backend} (RMSE: {metrics['rmse']:.2f}, Diff: {metrics['mismatch_pct']:.2f}%, MaxDiff: {metrics['max_diff']})")
                else:
                    all_passed = False
                    print(f"  -> [FAIL] {backend}: {metrics['error']}")
            else:
                all_passed = False
                results[test_name]["_metrics"][backend] = {
                    "passed": False,
                    "error": "Process execution failed",
                    "rmse": 0.0,
                    "mismatch_pct": 100.0,
                    "diff_pixels": 0,
                    "total_pixels": 0,
                    "max_diff": 0,
                    "diff_image": None,
                }

    return results, all_passed

def run_unit_tests(args):
    git_root = find_git_root()
    assert git_root, "Not within a git repository!"
    exec_dir = os.path.dirname(args.executable)
    exec_ext = os.path.splitext(args.executable)[1]
    unit_exec = os.path.join(exec_dir, f"lyra-unit-tests{exec_ext}")
    print(">>> Running Unit Tests...")
    try:
        if os.path.exists(unit_exec):
            subprocess.check_call([unit_exec], cwd=git_root)
        else:
            subprocess.check_call([args.executable, "-tce=rhi*"], cwd=git_root)
        print(">>> Unit Tests PASSED.")
        return True
    except subprocess.CalledProcessError as e:
        print(f">>> Unit Tests FAILED with exit code {e.returncode}.")
        return False

def generate_html_report(args, results, unit_success=True):
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

    # Compute summary stats
    total_checks = 0
    passed_checks = 0
    failed_checks = 0
    for _, buckets in results.items():
        metrics_dict = buckets.get("_metrics", {})
        for _, metrics in metrics_dict.items():
            total_checks += 1
            if metrics.get("passed", False):
                passed_checks += 1
            else:
                failed_checks += 1

    overall_status_text = "ALL PASSED" if (failed_checks == 0 and unit_success) else "FAILED"
    overall_status_color = "var(--success)" if (failed_checks == 0 and unit_success) else "var(--error)"

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
            color: {overall_status_color};
        }}

        table {{
            width: 100%;
            border-collapse: separate;
            border-spacing: 0;
            background: var(--surface);
            border-radius: 12px;
            overflow: hidden;
            border: 1px solid var(--border);
            table-layout: auto;
        }}
        th {{
            background-color: var(--surface-low);
            color: var(--text-muted);
            text-transform: uppercase;
            font-size: 0.75rem;
            font-weight: 700;
            padding: 1rem;
            text-align: left;
            border-bottom: 1px solid var(--border);
            position: sticky;
            top: 0;
            z-index: 10;
        }}
        td {{
            padding: 1rem;
            border-bottom: 1px solid var(--border);
            vertical-align: middle;
        }}
        tr:last-child td {{ border-bottom: none; }}
        tr:hover td {{ background-color: var(--surface-high); }}

        .test-info {{
            padding-left: 1.5rem;
        }}
        .test-name {{
            font-family: var(--font-code);
            font-weight: 600;
            font-size: 1rem;
            display: block;
            color: var(--text);
            word-break: break-all;
        }}

        .docs-btn {{
            display: inline-flex;
            align-items: center;
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
            gap: 0.4rem;
        }}
        .test-img {{
            max-width: 320px;
            height: auto;
            border-radius: 8px;
            border: 1px solid var(--border);
            background: #000;
            transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
            cursor: pointer;
        }}
        .test-img:hover {{
            transform: translateY(-4px) scale(1.02);
            border-color: var(--accent);
            box-shadow: 0 8px 24px rgba(0,0,0,0.5), 0 0 0 2px var(--accent-glow);
        }}

        .badge {{
            display: inline-block;
            padding: 0.25rem 0.6rem;
            font-size: 0.72rem;
            font-family: var(--font-code);
            font-weight: 600;
            border-radius: 4px;
            text-align: center;
            white-space: nowrap;
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

        .diff-btn {{
            display: inline-flex;
            align-items: center;
            margin-top: 0.2rem;
            padding: 0.2rem 0.5rem;
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
                <span class="stat-label">RHI Checks</span>
                <span class="stat-value">{passed_checks}/{total_checks} Pass</span>
            </div>
            <div class="stat">
                <span class="stat-label">Status</span>
                <span class="stat-badge">{overall_status_text}</span>
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
    html_content.append('<th>Target Test</th>')
    for key in sequence:
        html_content.append(f'<th style="text-align: center;">{key}</th>')
    html_content.append('</tr></thead>')
    html_content.append("<tbody>")

    git_root = find_git_root()
    for test_name, buckets in results.items():
        readme_path = os.path.join(git_root, "Tests", test_name, "README.md")
        has_readme = os.path.exists(readme_path)
        readme_content = ""
        if has_readme:
            with open(readme_path, "r", encoding="utf-8") as f:
                readme_content = f.read()

        encoded_docs = base64.b64encode(readme_content.encode("utf-8")).decode("utf-8")

        html_content.append('<tr>')
        html_content.append(f'''
            <td class="test-info">
                <span class="test-name">{test_name}</span>
                <div style="margin-top: 0.75rem;">
                    <button class="docs-btn {"" if has_readme else "disabled"}"
                            onclick="openDocs('{encoded_docs}', '{test_name}')"
                            {"disabled" if not has_readme else ""}>
                        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="width:14px;height:14px;margin-right:6px;"><path d="M4 19.5A2.5 2.5 0 0 1 6.5 17H20"></path><path d="M6.5 2H20v20H6.5A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2z"></path></svg>
                        View Docs
                    </button>
                </div>
            </td>''')

        metrics_map = buckets.get("_metrics", {})

        for key in sequence:
            html_content.append('<td><div class="img-cell">')
            if key in buckets and os.path.exists(buckets[key]):
                image = buckets[key]
                image_uri = pathlib.Path(os.path.abspath(image)).as_uri()
                html_content.append(f'<img src="{image_uri}" class="test-img" onclick="openLightbox(this.src, \'{test_name} [{key.upper()}]\')"/>')

                if key == "reference":
                    html_content.append('<span class="badge badge-ref">REFERENCE</span>')
                elif key in metrics_map:
                    m = metrics_map[key]
                    if m.get("passed", False):
                        html_content.append(f'<span class="badge badge-pass">PASS &bull; RMSE {m["rmse"]:.2f} &bull; {m["mismatch_pct"]:.2f}% diff</span>')
                    else:
                        err = m.get("error") or f'RMSE {m["rmse"]:.2f}'
                        html_content.append(f'<span class="badge badge-fail">FAIL &bull; {err}</span>')

                    diff_img = buckets.get(f"{key}_diff")
                    if diff_img and os.path.exists(diff_img):
                        diff_uri = pathlib.Path(os.path.abspath(diff_img)).as_uri()
                        html_content.append(f'<button class="diff-btn" onclick="openLightbox(\'{diff_uri}\', \'{test_name} [{key.upper()} DIFF]\')">View Diff</button>')
            else:
                html_content.append('<span class="empty-state">Not Available</span>')
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
    </script>
</body>
</html>
''')

    html = "\n".join(html_content)
    path = os.path.join(args.directory, "report.html")
    with open(path, "w", encoding="utf-8") as f:
        f.write(html)
    return path

def main():
    args = parse_args()
    prepare_run(args)
    unit_passed = run_unit_tests(args)
    tests = list_tests(args)
    tests = filter_rhi_tests(tests)
    rhi_passed = True
    if tests:
        buckets = bucketize_tests(tests)
        results, rhi_passed = run_rhi_tests(args, buckets)
        report = generate_html_report(args, results, unit_success=unit_passed)
        try:
            webbrowser.open(report)
        except Exception:
            pass

    overall_passed = unit_passed and rhi_passed
    print("=" * 60)
    print(f"Lyra TestKit Result: {'SUCCESS' if overall_passed else 'FAILURE'}")
    print(f"  Unit Tests: {'PASSED' if unit_passed else 'FAILED'}")
    print(f"  RHI Tests:  {'PASSED' if rhi_passed else 'FAILED'}")
    print("=" * 60)

    if not overall_passed:
        sys.exit(1)

if __name__ == "__main__":
    main()
