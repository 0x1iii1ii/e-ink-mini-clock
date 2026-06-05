Import("env")
import subprocess, sys

result = subprocess.run(
    [env.subst("$PYTHONEXE"), "tools/web2gz/convert2gz.py"], check=True
)
