set shell := ["sh", "-cu"]
set windows-shell := ["powershell.exe", "-NoLogo", "-Command"]

PYTHON := if os() == "windows" { "python" } else { "python3" }

list:
    @cmake --list-presets=all

config generator preset="debug":
    @{{PYTHON}} Tools/Scripts/build.py config {{generator}} {{preset}}

switch preset:
    @{{PYTHON}} Tools/Scripts/build.py switch {{preset}}

build target="all":
    @{{PYTHON}} Tools/Scripts/build.py build --target {{target}}

run target="all" *args="":
    @{{PYTHON}} Tools/Scripts/build.py run --target {{target}} -- {{args}}

test target="all":
    @{{PYTHON}} Tools/Scripts/build.py build --target testkit
    @{{PYTHON}} Tools/Scripts/build.py test --target {{target}}

[confirm("This will clean all build products! (y/n)")]
clean:
    @{{PYTHON}} Tools/Scripts/build.py run --target clean
