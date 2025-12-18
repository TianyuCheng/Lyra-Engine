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
    sequence = ["reference", "vulkan"]

    os_name = platorm.system()
    match os_name:
        case "Windows":
            sequence.append("d3d12")
        case "Darwin":
            sequence.append("metal")

    html_content = []
    html_content.append('''
    <!DOCTYPE html>
    <html>
    <head>
        <title>Test Results</title>
        <style>
            td img {
                max-width: 100%;
                height: auto;
                border: 1px solid black;
            }
        </style>

        <link rel="stylesheet" href="https://fonts.googleapis.com/css?family=Roboto:300,300italic,700,700italic">
        <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/normalize/8.0.1/normalize.css">
        <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/milligram/1.4.1/milligram.css">
    </head>
    ''')
    html_content.append('<body>')
    html_content.append('<div class="container">')
    html_content.append('<h1>Test Result</h1>')

    html_content.append("<table>")
    html_content.append("<thead>")
    html_content.append('<tr>')
    html_content.append(f'<th>name</th>')
    for key in sequence:
        html_content.append(f'<th>{key}</th>')
    html_content.append('</tr>')
    html_content.append('</thead>')
    html_content.append("<tbody>")
    for test_name, buckets in results.items():
        html_content.append('<tr>')
        html_content.append(f'<td>{test_name}</td>')
        for key in sequence:
            html_content.append('<td>')
            if key in buckets:
                image = buckets[key]
                html_content.append(f'<img src="{image}"/>')
            html_content.append('</td>')
        html_content.append('</tr>')
    html_content.append("</tbody>")
    html_content.append("</table>")

    html_content.append('</div>')
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
