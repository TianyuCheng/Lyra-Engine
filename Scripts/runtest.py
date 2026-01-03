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
            subprocess.check_call([args.executable, f"-tc={full_name}"], cwd=directory)
            test_result = os.path.join(directory, f"{backend}.png")
            results[test_name][backend] = test_result

    return results

def run_unit_tests(args):
    git_root = find_git_root()
    assert git_root, "Not within a git repository!"
    subprocess.check_call([args.executable, f"-tce=rhi*"])

def generate_html_report(args, results):
    import pathlib
    sequence = ["reference"]

    os_name = platform.system()
    if os_name == "Windows":
        sequence.append("d3d12")
    if os_name == "Darwin":
        sequence.append("metal")
    if check_vulkan_info():
        sequence.append("vulkan")

    html_content = []
    html_content.append('''
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Lyra TestKit Report</title>
        <link rel="stylesheet" href="https://fonts.googleapis.com/css?family=Roboto:300,300italic,700,700italic">
        <style>
            :root {
                --background-color: #282c34;
                --text-color: #abb2bf;
                --header-color: #61afef;
                --table-header-bg: #3c4048;
                --table-border-color: #454a54;
                --table-row-even-bg: #31363f;
                --image-border-color: #454a54;
                --image-shadow: 0 3px 5px 0 rgba(0, 0, 0, 0.2);
                --image-hover-shadow: 0 5px 11px 0 rgba(0, 0, 0, 0.4);
                --link-color: #61afef;
                --link-hover-color: #c678dd;
            }
            body {
                background-color: var(--background-color);
                color: var(--text-color);
                font-family: 'Roboto', sans-serif;
                margin: 0;
                padding: 1.33em;
            }
            .container {
                max-width: 70%;
                min-width: 500px;
                margin: 0 auto;
            }
            h1 {
                color: var(--header-color);
                text-align: center;
                margin-bottom: 1.33rem;
            }
            table {
                width: 100%;
                border-collapse: collapse;
                margin-top: 1.33rem;
            }
            th, td {
                text-align: center;
                padding: 0.66rem;
                border: 1px solid var(--table-border-color);
            }
            th {
                background-color: var(--table-header-bg);
                text-transform: capitalize;
                font-size: 0.73em;
            }
            tr:nth-child(even) {
                background-color: var(--table-row-even-bg);
            }
            td.test-name {
                font-weight: bold;
                font-size: 0.73em;
                word-break: break-all;
            }
            td img {
                max-width: 266px;
                height: auto;
                border: 1px solid var(--image-border-color);
                border-radius: 5px;
                box-shadow: var(--image-shadow);
                transition: transform 0.2s, box-shadow 0.2s;
                cursor: pointer;
            }
            td img:hover {
                transform: scale(1.05);
                box-shadow: var(--image-hover-shadow);
            }
            /* Lightbox styles */
            .lightbox {
                display: none;
                position: fixed;
                z-index: 1000;
                left: 0;
                top: 0;
                width: 100%;
                height: 100%;
                overflow: auto;
                background-color: rgba(0,0,0,0.9);
                justify-content: center;
                align-items: center;
            }
            .lightbox-content {
                max-width: 90vw;
                max-height: 90vh;
                animation: zoom 0.3s;
            }
            @keyframes zoom {
                from {transform:scale(0)}
                to {transform:scale(1)}
            }
            .close {
                position: absolute;
                top: 10px;
                right: 23px;
                color: #f1f1f1;
                font-size: 27px;
                font-weight: bold;
                transition: 0.3s;
                cursor: pointer;
            }
            .close:hover,
            .close:focus {
                color: #bbb;
            }
        </style>
    </head>
    ''')
    html_content.append('<body>')
    html_content.append('<div class="container">')
    html_content.append('<h1>Lyra TestKit Report</h1>')

    html_content.append("<table>")
    html_content.append("<thead>")
    html_content.append('<tr>')
    html_content.append(f'<th>Test Case</th>')
    for key in sequence:
        html_content.append(f'<th>{key}</th>')
    html_content.append('</tr>')
    html_content.append('</thead>')
    html_content.append("<tbody>")
    for test_name, buckets in results.items():
        html_content.append('<tr>')
        html_content.append(f'<td class="test-name">{test_name}</td>')
        for key in sequence:
            html_content.append('<td>')
            if key in buckets:
                image = buckets[key]
                image_uri = pathlib.Path(os.path.abspath(image)).as_uri()
                html_content.append(f'<img src="{image_uri}" onclick="openLightbox(this.src)"/>')
            html_content.append('</td>')
        html_content.append('</tr>')
    html_content.append("</tbody>")
    html_content.append("</table>")

    html_content.append('''
        <div id="myLightbox" class="lightbox">
            <span class="close" onclick="closeLightbox()">&times;</span>
            <img class="lightbox-content" id="lightboxImage">
        </div>
    ''')

    html_content.append('</div>')

    html_content.append('''
    <script>
        const lightbox = document.getElementById('myLightbox');
        const lightboxImg = document.getElementById('lightboxImage');

        function openLightbox(src) {
            lightbox.style.display = 'flex';
            lightboxImg.src = src;
        }

        function closeLightbox() {
            lightbox.style.display = 'none';
        }

        lightbox.addEventListener('click', function(event) {
            if (event.target === lightbox) {
                closeLightbox();
            }
        });

        document.addEventListener('keydown', function(event) {
            if (event.key === 'Escape') {
                closeLightbox();
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
    buckets = bucketize_tests(tests)
    results = run_rhi_tests(args, buckets)
    report = generate_html_report(args, results)
    webbrowser.open(report)

if __name__ == "__main__":
    main()
