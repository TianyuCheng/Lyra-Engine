set shell := ["sh", "-cu"]
set windows-shell := ["powershell.exe", "-NoLogo", "-Command"]

BUILD := "Scratch"

list:
    @cmake --list-presets=all

configure preset:
    cmake --preset {{preset}}

build preset:
    cmake --build --preset {{preset}}

test preset:
    cmake --build --preset {{preset}}
    cmake --build --preset {{preset}} --target testkit

run preset target:
    cmake --build --preset {{preset}}
    cmake --build --preset {{preset}} --target {{target}}

[confirm("This will clean all build products! (y/n)")]
clean preset:
    cmake --build --preset {{preset}} --target clean
