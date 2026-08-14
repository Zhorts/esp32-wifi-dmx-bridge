Import("env")
import subprocess

def git(*args):
    try:
        return subprocess.check_output(
            ["git", *args], stderr=subprocess.DEVNULL
        ).strip().decode()
    except Exception:
        return "unknown"

build_number = git("rev-list", "--count", "HEAD")
version_string = git("describe", "--tags", "--always", "--dirty")

env.Append(CPPDEFINES=[
    ("BUILD_NUMBER", build_number),
    ("GIT_VERSION", '\\"%s\\"' % version_string),
])