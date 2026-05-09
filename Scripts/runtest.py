import os
import shutil
import argparse
import platform
import subprocess
import webbrowser
import xml.etree.ElementTree as ET

def parse_args():
    parser = argparse.ArgumentParser(prog="lyra-testkit", description="helper script to run tests for Lyra-Engine")
    parser.add_argument("executable", help="path to the executable for testkit")
    parser.add_argument("--directory", help="target directory for the generated test output and report")
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
        return True#, "vulkaninfo command ran successfully."
    except FileNotFoundError:
        return False#, "vulkaninfo executable not found. The Vulkan SDK or tools package may not be installed."
    except subprocess.CalledProcessError as e:
        return False#, f"vulkaninfo command failed to run: {e.output.decode().strip()}. This often indicates no drivers were found."
    except Exception as e:
        return False#, f"An unexpected error occurred: {e}"

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
    output = os.path.join(args.directory, "tests.xml")
    subprocess.check_call([args.executable, '-ltc', "-r=xml", f"-out={output}"])
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

def run_rhi_tests(args, buckets):
    results = {}

    git_root = find_git_root()
    assert git_root, "Not within a git repository!"

    for test_name, variants in buckets.items():
        directory = os.path.join(args.directory, test_name)
        os.makedirs(directory, exist_ok=True)
        results[test_name] = {}
        results[test_name]["reference"] = os.path.join(git_root, "TestKit", test_name, "reference.png")
        for variant in variants:
            print("::".join(variant["components"]))
            full_name = variant["name"]
            backend = variant["components"][1]
            try:
                subprocess.check_call([args.executable, f"-tc={full_name}"], cwd=directory)
            except subprocess.CalledProcessError:
                print(f"Failed to run test: {full_name}")
            finally:
                test_result = os.path.join(directory, f"{backend}.png")
                results[test_name][backend] = test_result

    return results

def run_unit_tests(args):
    git_root = find_git_root()
    assert git_root, "Not within a git repository!"
    subprocess.check_call([args.executable, f"-tce=rhi*"])

def generate_html_report(args, results):
    import pathlib
    import datetime
    sequence = ["reference"]

    os_name = platform.system()
    if os_name == "Windows":
        sequence.append("d3d12")
    if os_name == "Darwin":
        sequence.append("metal")
    if check_vulkan_info():
        sequence.append("vulkan")

    total_tests = len(results)
    timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    html_content = []
    html_content.append(f'''
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Lyra TestKit Report</title>
        <link rel="preconnect" href="https://fonts.googleapis.com">
        <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
        <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600;700&family=Roboto+Mono:wght@400;500&display=swap" rel="stylesheet">
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

            .img-container {{
                display: flex;
                justify-content: center;
                align-items: center;
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
                background-color: rgba(0, 0, 0, 0.4);
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
                background: rgba(0,0,0,0.7);
                padding: 0.5rem 1.5rem;
                border-radius: 2rem;
                font-family: var(--font-code);
                color: var(--accent);
                border: 1px solid var(--border);
            }}

            ::-webkit-scrollbar {{ width: 10px; height: 10px; }}
            ::-webkit-scrollbar-track {{ background: var(--bg); }}
            ::-webkit-scrollbar-thumb {{ background: var(--border); border-radius: 5px; }}
            ::-webkit-scrollbar-thumb:hover {{ background: var(--text-muted); }}
        </style>
    </head>
    ''')
    html_content.append('<body>')
    html_content.append('<div class="container">')
    html_content.append(f'''
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
                    <span class="stat-value">{total_tests}</span>
                </div>
                <div class="stat">
                    <span class="stat-label">Timestamp</span>
                    <span class="stat-value">{timestamp}</span>
                </div>
            </div>
        </header>
    ''')

    html_content.append("<table>")
    html_content.append("<thead>")
    html_content.append('<tr>')
    html_content.append(f'<th>Target Test</th>')
    for key in sequence:
        html_content.append(f'<th style="text-align: center;">{key}</th>')
    html_content.append('</tr>')
    html_content.append('</thead>')
    html_content.append("<tbody>")
    git_root = find_git_root()
    import base64
    for test_name, buckets in results.items():
        readme_path = os.path.join(git_root, "TestKit", test_name, "README.md")
        has_readme = os.path.exists(readme_path)
        readme_content = ""
        if has_readme:
            with open(readme_path, "r", encoding="utf-8") as f:
                readme_content = f.read()

        # Base64 encode to safely embed in data attribute
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
        for key in sequence:
            html_content.append('<td><div class="img-container">')
            if key in buckets:
                image = buckets[key]
                image_uri = pathlib.Path(os.path.abspath(image)).as_uri()
                html_content.append(f'<img src="{image_uri}" class="test-img" onclick="openLightbox(this.src, \'{test_name} [{key.upper()}]\')"/>')
            else:
                html_content.append('<span class="empty-state">Not Available</span>')
            html_content.append('</div></td>')
        html_content.append('</tr>')
    html_content.append("</tbody>")
    html_content.append("</table>")

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
    ''')

    html_content.append('</div>')

    html_content.append('''
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
    ''')

    html_content.append('</body>')
    html_content.append('</html>')

    html = "\n".join(html_content)
    path = os.path.join(args.directory, "report.html")
    with open(path, "w") as f:
        print(html, file=f)
    return path

def main():
    args = parse_args();
    prepare_run(args)
    run_unit_tests(args)
    tests = list_tests(args)
    tests = filter_rhi_tests(tests)
    if tests:
        buckets = bucketize_tests(tests)
        results = run_rhi_tests(args, buckets)
        report = generate_html_report(args, results)
        webbrowser.open(report)

if __name__ == "__main__":
    main()
