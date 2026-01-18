set shell := ["sh", "-cu"]
set windows-shell := ["powershell.exe", "-NoLogo", "-Command"]

PYTHON := if os() == "windows" { "python" } else { "python3" }

list:
    @cmake --list-presets=all

config generator preset="debug":
    @{{PYTHON}} Scripts/build.py config {{generator}} {{preset}}

switch preset:
    @{{PYTHON}} Scripts/build.py switch {{preset}}

build target="all":
    @{{PYTHON}} Scripts/build.py build --target {{target}}

run target="all":
    @{{PYTHON}} Scripts/build.py run --target {{target}}

test target="all":
    @{{PYTHON}} Scripts/build.py build --target testkit
    @{{PYTHON}} Scripts/build.py test --target {{target}}

[confirm("This will clean all build products! (y/n)")]
clean:
    @{{PYTHON}} Scripts/build.py run --target clean
